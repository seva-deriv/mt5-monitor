#!/usr/bin/env pwsh

#Requires -Version 7

[CmdletBinding()]
param (
    [switch]
    $Clear = $false,

    [int]
    $Parallel = [System.Environment]::ProcessorCount,

    [string]
    [ValidateSet('Windows', 'Linux')]
    $TargetPlatform = $IsWindows ? 'Windows' : 'Linux',

    # MSBuild fails to build and test, because it breaks on catch tests discovery.
    # So Ninja should be considered as a good cross-platform choice.
    # Makefiles are also known to work fine.
    [string]
    $Generator = 'Ninja',

    [string]
    [ValidateSet('Debug', 'Release', 'MinSizeRel', 'RelWithDebInfo')]
    $Configuration = 'RelWithDebInfo',

    [string]
    [ValidateSet('', 'libstdc++', 'libc++')]
    $LibCxxImpl = '',

    [string]
    [ValidateSet('', 'address', 'thread', 'undefined')]
    $Sanitize = '',

    [string]
    [ValidateSet('None', 'Report', 'Show')]
    $Coverage = 'None',

    [string]
    $BuildDestination = (Join-Path $PSScriptRoot 'build' $TargetPlatform.ToLowerInvariant()),

    [string]
    $LLVMVer = $Env:LLVM_VER,

    [string]
    $MSVCVer = "16",

    [switch]
    $UseClang = $false,

    [switch]
    $UseConan = $true,

    [string]
    $ConanDestination = $BuildDestination,

    [string]
    $ConanConfiguration = $Configuration,

    [string]
    $ToolchainsDir = (Join-Path $PSScriptRoot "cmake"),

    [switch]
    $NoConan = $false,

    [switch]
    $NoConfigure = $false,

    [switch]
    $NoBuild = $false,

    [switch]
    $NoTest = $false,

    [switch]
    $NoCoverage = $false
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$HostPlatform = 'Other'
if ($IsLinux) {
    $HostPlatform = "Linux"
}
elseif ($IsWindows) {
    $HostPlatform = "Windows"
}

$ExeSuffixes = @{
    Linux = '';
    Windows = '.exe';
}

$HostExeSuffix = $ExeSuffixes[$HostPlatform]
$TargetExeSuffix = $ExeSuffixes[$TargetPlatform]

if ($LLVMVer) {
    # User specified LLVM version thus it must be used.
    $UseClang = $true
    $LlvmSuffix = "-$LLVMVer"
    $Env:LLVM_VER = $LLVMVer  # Propagate to cmake
} else {
    $LlvmSuffix = ""
}

$ClangIsAvailable = Get-Command "clang${LlvmSuffix}${HostExeSuffix}" -ErrorAction SilentlyContinue
if ($UseClang -and -not $ClangIsAvailable) {
    Write-Error "Unable to find LLVM/Clang with specified version (LLVMVer=$LLVMVer)."
}

$Toolchains = @{
    Linux = @{
        Windows = @('clang-cl.cmake', 'wine');
        Linux = @($UseClang ? 'clang-gcc.cmake' : 'default', '');
    };
    Windows = @{
        Windows = @($UseClang ? 'clang-cl.cmake' : 'default', '');
        Linux = @('error', '');
    };
}

$Toolchain, $TargetExeRunner = $Toolchains[$HostPlatform][$TargetPlatform]

if ($Toolchain -match '^clang') {
    if (Get-Command llvm-config$LlvmSuffix$HostExeSuffix -ErrorAction SilentlyContinue) {
        Invoke-Expression "llvm-config${LlvmSuffix}${HostExeSuffix} --version" | Set-Variable LlvmVersion
    } elseif (Get-Command llvm-ar$LlvmSuffix$HostExeSuffix -ErrorAction SilentlyContinue) {
        # llvm-config is more convenient but it is unavailable on Windows, thus we use llvm-ar and parse its output.
        $LlvmVersion = (Invoke-Expression "llvm-ar${LlvmSuffix}${HostExeSuffix} --version" | Where-Object { $_ -match '\d+\.\d+\.\d+' }) -replace '.*(\d+\.\d+\.\d+).*','$1'
    } else {
        Write-Error "LLVM-based compiler is requested, but we are unable to find LLVM.`n`
        Consider using -LLVMVer argument or check if its value is correct."
    }
    $Env:LLVM_VERSION=$LlvmVersion
}

$ConanInstallArgs = @()
if ($TargetPlatform -eq 'Windows') {
    $ConanInstallArgs += @(
        '-s', 'os=Windows',
        '-s', 'compiler="Visual Studio"',
        '-s', "compiler.version=$MSVCVer",
        '-s', "build_type=$ConanConfiguration")
} elseif ($TargetPlatform -eq 'Linux') {
    $ConanInstallArgs += @(
        '-s', 'os=Linux',
        '-s', "build_type=$ConanConfiguration")
    if ($UseClang) {
        $ConanLlvmVer = $LLVMVer ?? ($LlvmVersion -replace '(\d+)(\.\d+)*','$1')
        $ConanInstallArgs += @(
            '-s', 'compiler=clang',
            '-s', "compiler.version=$ConanLlvmVer")
    }
    $ConanLibCxxImpl = $LibCxxImpl
    if ($ConanLibCxxImpl -match '^(libstdc\+\+|)$') {
        $ConanLibCxxImpl = 'libstdc++11'  # Use C++11 ABI, otherwise we get ABI mismatch
    }
    $ConanInstallArgs += @('-s', "compiler.libcxx=$ConanLibCxxImpl")
}

$CMakeConfigureArgs = @("-DCMAKE_BUILD_TYPE=$Configuration")

if ($Toolchain -eq 'error') {
    Write-Error "Cross-compilation is supported only in Linux -> Windows direction"
} elseif ($Toolchain -ne 'default') {
    $CMakeConfigureArgs += @("--toolchain", (Join-Path $ToolchainsDir $Toolchain))
}

if ($Generator) {
    $CMakeConfigureArgs += @('-G', "'$Generator'")
}

# Unconditionally set variables to make cmake work correctly from dirty build dir,
# when on previous pass values were set but on current are not.
$CMakeConfigureArgs += @("-DLIBCXXIMPL=$LibCxxImpl", "-DSANITIZE=$Sanitize")

if ($Coverage -ne 'None') {
    if ($Toolchain -notmatch '^clang') {
        Write-Error "Coverage collecting is not implemented for not LLVM-based compilers"
    }
    $ProfDataPath = Join-Path $BuildDestination default.profdata
    $Env:LLVM_PROFILE_FILE = Join-Path $BuildDestination default.profraw
    $CMakeConfigureArgs += @('-DLLVM_COVERAGE=1')
} else {
    $CMakeConfigureArgs += @('-DLLVM_COVERAGE=0')
}

$CMakeBuildArgs = @()

if ($Generator -match 'Visual Studio') {
    $CMakeBuildArgs += @("/m:$Parallel")
} else {
    $CMakeBuildArgs += @("-j$Parallel")
}

if ($Clear) {
    if ($UseConan -and (Test-Path $ConanDestination)) {
        Remove-Item -Recurse -Force $ConanDestination
    }
    if (Test-Path $BuildDestination) {
        Remove-Item -Recurse -Force $BuildDestination
    }
}

if (-not (Test-Path $ConanDestination)) {
    New-Item -ItemType Directory -Path $ConanDestination | Out-Null
}

Push-Location $ConanDestination
try {
    if ($UseConan) {
        if (-not $NoConan) {
            Invoke-Expression "conan install $ConanInstallArgs $PSScriptRoot --build=missing"
            if ($LASTEXITCODE -ne 0) { Write-Error "Conan install step failed" }
        }
        $CMakeConfigureArgs += @('-DNO_CONAN=0', "-DCONAN_DESTINATION=$ConanDestination")
    } else {
        # Is it still needed?
        Get-ChildItem $ConanDestination -Filter 'conan*' | Remove-Item
        $CMakeConfigureArgs += @('-DNO_CONAN=1')
    }
}
finally {
    Pop-Location
}

if (-not (Test-Path $BuildDestination)) {
    New-Item -ItemType Directory -Path $BuildDestination | Out-Null
}

Push-Location $BuildDestination
try {
    if (-not $NoConfigure) {
        Invoke-Expression "cmake $CMakeConfigureArgs $PSScriptRoot"
        if ($LASTEXITCODE -ne 0) { Write-Error "Configure step failed" }
    }
    if (-not $NoBuild) {
        Invoke-Expression "cmake --build . -- $CMakeBuildArgs"
        if ($LASTEXITCODE -ne 0) { Write-Error "Build step failed" }
    }
    if (-not $NoTest) {
        Invoke-Expression "ctest -j$Parallel --output-on-failure --output-junit $(Join-Path $BuildDestination result-junit.xml)"
        if ($LASTEXITCODE -ne 0) { Write-Error "Test step failed" }
    }
}
finally {
    Pop-Location
}

if (-not $NoCoverage -and $Coverage -ne 'None') {
    Push-Location $PSScriptRoot  # 'Hidden knowledge' that megatest should be run from the repo root
    try {
        $MegatestPath = Join-Path $BuildDestination MT5-Relay bin megatest$TargetExeSuffix
        Invoke-Expression "$TargetExeRunner $MegatestPath"
        & llvm-profdata$LlvmSuffix merge -sparse "$Env:LLVM_PROFILE_FILE" -o "$ProfDataPath"
        if ($LASTEXITCODE -ne 0) { Write-Error "llvm-profdata failed" }
        & llvm-cov$LlvmSuffix $Coverage.ToLowerInvariant() `
            $MegatestPath `
            --instr-profile="$ProfDataPath" `
            --ignore-filename-regex="\.wine|xwin|$BuildDestination|lib/"
        if ($LASTEXITCODE -ne 0) { Write-Error "llvm-cov failed" }
    }
    finally {
        Pop-Location
    }
}
