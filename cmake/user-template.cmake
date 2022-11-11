# This file may be used for user-specific configuration.
# How to use it:
#  1. Copy it to user.cmake
#  2. Set variables to corresponding values specific to your environment.
# Note that user.cmake itself is in .gitignore list, so it will never be commited
# as your personal environment is not interesting to other developers.

# !!! DON'T ADD TRAILING SLASH !!!

SET(BOOST_ROOT "")        # E.g. "C:\\development\\boost_1_80_0"
SET(BOOST_LIBRARYDIR "")  # E.g. "C:\\development\\boost_1_80_0\\lib64-msvc-14.3"
SET(OPENSSL_ROOT_DIR "")  # E.g. "C:\\development\\openssl"

# !!! Normally you don't need these variables to be explicitly set.
SET(XWIN_PREFIX "")  # Set this to 'splat' dir if you are cross-compiling and have xwin-downloaded WinSDK
SET(LLVM_PREFIX "")  # Set this and following two if you are cross-compiling and have native LLVM installed
SET(LLVM_VER "")     # Example: "14"
SET(LLVM_VERSION "") # Example: "14.0.0"
