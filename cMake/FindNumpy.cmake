# Find numpy (including C API)
#
# It defines the following variables
# NUMPY_FOUND - True if libclang has been found.
# NUMPY_ROOT - where to find libclang
# NUMPY_INCLUDE_DIRS - where to find libclang include files
# NUMPY_LIBRARIES - libraries to be linked

INCLUDE (FindPackageHandleStandardArgs)

# Lets numpy say us where we can find the headers
EXECUTE_PROCESS(
    COMMAND ${PYTHON_EXECUTABLE} -c "import numpy; print(numpy.get_include())"
    ERROR_VARIABLE NUMPY_INCLUDE_ERROR
    RESULT_VARIABLE NUMPY_INCLUDE_RESULT
    OUTPUT_VARIABLE NUMPY_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

IF(NUMPY_INCLUDE_ERROR OR NUMPY_INCLUDE_RESULT)
    MESSAGE(FATAL_ERROR "Failure finding numpy headers")
ENDIF(NUMPY_INCLUDE_ERROR OR NUMPY_INCLUDE_RESULT)

FIND_PATH(NUMPY_INCLUDE_DIRS
  NAMES
    numpy/npy_common.h
  PATHS
    ${NUMPY_INCLUDE_DIR}
)

# Lets find the numpy module
EXECUTE_PROCESS(
    COMMAND ${PYTHON_EXECUTABLE} -c "import numpy, os; print(os.path.dirname(numpy.__file__))"
    ERROR_VARIABLE NUMPY_ROOT_ERROR
    RESULT_VARIABLE NUMPY_ROOT_RESULT
    OUTPUT_VARIABLE NUMPY_ROOT_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
IF(NUMPY_ROOT_ERROR OR NUMPY_ROOT_RESULT)
    MESSAGE(FATAL_ERROR "Failure finding numpy module path")
ENDIF(NUMPY_ROOT_ERROR OR NUMPY_ROOT_RESULT)

SET(NUMPY_ROOT ${NUMPY_ROOT_DIR})

MARK_AS_ADVANCED(NUMPY_ROOT NUMPY_INCLUDE_DIRS)

FIND_PACKAGE_HANDLE_STANDARD_ARGS (NUMPY REQUIRED_VARS NUMPY_ROOT
  NUMPY_INCLUDE_DIRS)