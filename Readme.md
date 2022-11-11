# What is mt5-monitor?

This repository provides `mt5-monitor` Windows executable.
This executable connects to the MetaTrader 5 (MT5) server (given server host, username and password)
and subscribes to receive ticks (quotes) for all available symbols (e.g. EURUSD).

Receiving every tick monitor checks if it pass some simple conditions, namely:
 - bad-bid-ask
 - quote-from-future
 - quote-from-past
 - market-is-closed

Then monitor puts the following metrics per tick to the DataDog:
 - feed.mt5_relay.quote.count
 - feed.mt5_relay.quote.latency
 - feed.mt5_relay.quote.ask
 - feed.listener.status_fail -- if any check fails

# How to build mt5-monitor? 

## Tools prerequisites

For developing you need the following tools:

1. Visual Studio Community Edition 2022 you can get it from [here](<https://visualstudio.microsoft.com/downloads/>), make sure to enable CMake option.
1. Conan package manager. It is obtained from `pip` which is Python package manager. So you should have Python with pip installed. You may get Python [here](https://www.python.org/downloads/windows/), pip is included.

To install conan execute the following command in your favourite shell (cmd, pwsh, bash...):

```
pip install conan
```

**Note!** A good practice is not to install python packages globally (system/user-wide) but to create an isolated environments with venv/pipenv/conda/etc.

## Command line based manual building approach

Then to build and test the project you should create a directory where you want to store your build artifacts, step into it and run the following commands:

```
conan install <path_to_repo_root> --build=missing
cmake <path_to_repo_root>
cmake --build .
ctest --output-on-failure
```

**Warning!** The main downside of this approach is the need for "proper" configuration of conan.
This approach is NOT recommended. See the following sections for alternatives.

## Command line based automated building approach

The easier way to build/test is to run `./build_and_test.ps1` pwsh-script.
This script could be found in repo root.
It has several command-line (configuration) options which may change its behavior,
and should be studied explicitly by reading the `build_and_test.ps1` script contents,
but primary use does not require any additional arguments.

The main thing the script automates for you in this context is conan configuration --
it passes additional arguments with conan configuration to conan as command line arguments.

**Note** To run pwsh-script you need pwsh (Powershell 7+) interpretter.
You may find it in google and download e.g. from Microsoft Github repo (binary prebuilt version).

## Developing with Microsoft Visual Studio.

Here we assume that you know how to use Visual Studio with CMake-based projects.

The main specifics of this project/repo is that it uses conan
and it is not run automatically during build process.
If you don't run conan manually compilation will fail.

As currently Visual Studio runs cmake in `<repo_root>/out/<configuration_name>/` folder
you are proposed to do one of the following things.

First of all run build which is expected to fail. It will create cmake output folder for you.
Then in the `pwsh` interpretter step into it and run `conan install <repo_root> --build=missing` in it. Then build in Visual Studio should success. If not -- you are in trouble :)

Alternative approach is to run

```pwsh
<repo_root>/build_and_test.ps1 -ConanDestination <path_to_vs_cmake_dir> -ConanConfiguration <Release|Debug|RelWithDebInfo>
```

# How to run mt5-server?

To run mt5-monitor from the command line you should provide it the following arguments
(ask your TL for specific credentials):

```shell
mt5-monitor.exe --login XXXX --password YYYY --server ZZZZ
```

This applies to running in debug session e.g. in MS VS or in VSCode.
