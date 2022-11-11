cmake_minimum_required (VERSION 3.2)
project(mt5-monitor)

# For std::chrono calendar
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT NO_CONAN)
    if (NOT CONAN_DESTINATION)  # Is defined by build_and_test.ps1
        set(CONAN_DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    endif ()
    if (NOT EXISTS "${CONAN_DESTINATION}/conan_paths.cmake")
        message(FATAL_ERROR
            "Conan is not run: ${CONAN_DESTINATION}/conan_paths.cmake file is missing. "
            "Conan is a python module so to run it you should first install python, "
            "then run 'pip install conan' and then either run "
            "${CMAKE_CURRENT_SOURCE_DIR}/build_and_test.ps1 -ConanDestination ${CONAN_DESTINATION} "
        )
    endif ()
    include(${CONAN_DESTINATION}/conan_paths.cmake)
endif ()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)

add_subdirectory(lib/statsd-client)

set(MANAGER_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/mt5-libs/manager_api")

set(Boost_USE_STATIC_LIBS  ON)
find_package(
    Boost
    COMPONENTS
        log
        nowide
        program_options
    REQUIRED
)

add_definitions(-D_WIN32_WINNT=0x0601)  # Unify ABI version, required for boost log

add_executable(
    mt5-monitor
    src/mt5-monitor.cpp
    src/Logging.cpp
    )
target_include_directories(
    mt5-monitor
    PUBLIC
    ${MANAGER_API_DIR}/Include
    # These two should be provided by StatsdClient lib when appropriate PR is merged
    ${StatsdClient_SOURCE_DIR}/src
    ${StatsdClient_BINARY_DIR}
)
target_link_libraries(
    mt5-monitor
    Boost::program_options
    Boost::log
    Boost::filesystem
    Boost::boost
    StatsdClient
)
file(
    COPY
    ${MANAGER_API_DIR}/Libs/MT5APIManager64.dll
    DESTINATION ${EXECUTABLE_OUTPUT_PATH}
)


# Testing
enable_testing()
find_package(Catch2 REQUIRED)
include(Catch)

add_executable(
    megatest
    src/test/MT5Monitor.cpp
)

target_include_directories(
    megatest
    PUBLIC
    ${MANAGER_API_DIR}/Include
    # These two should be provided by StatsdClient lib when appropriate PR is merged
    ${StatsdClient_SOURCE_DIR}/src
    ${StatsdClient_BINARY_DIR}
)

target_link_libraries(
    megatest
    Catch2::Catch2WithMain
    Boost::boost
    Boost::log
    StatsdClient
)

catch_discover_tests(megatest)
