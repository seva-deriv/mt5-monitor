# SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_C_COMPILER  clang)
SET(CMAKE_CXX_COMPILER clang++)
SET(CMAKE_LINKER ld.lld)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")