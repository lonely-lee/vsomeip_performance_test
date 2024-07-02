set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if (CROSS_TOOLS_PATH)
    set(CMAKE_C_COMPILER ${CROSS_TOOLS_PATH}/bin/aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER ${CROSS_TOOLS_PATH}/bin/aarch64-linux-gnu-g++)
else()
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread -ldl")

set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")

set(CMAKE_PREFIX_PATH "/home/hikerlee02/workspace/armtarget")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)