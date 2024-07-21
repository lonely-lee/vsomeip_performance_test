set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)

if (CROSS_TOOLS_PATH)
    set(CMAKE_C_COMPILER ${CROSS_TOOLS_PATH}/bin/arm-linux-gnueabihf-gcc)
    set(CMAKE_CXX_COMPILER ${CROSS_TOOLS_PATH}/bin/arm-linux-gnueabihf-g++)
else()
    set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
    set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread -ldl -march=armv7-a")

set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")

set(CMAKE_PREFIX_PATH "/home/lee_home/workspace/arm_lib_5.5")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)