cmake_minimum_required(VERSION 3.13)
project(vsomeip_performance_test)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()

find_package(vsomeip3 3.4.10 REQUIRED)

add_executable(test_method_client 
    src/test_method_client.cpp
    src/cpu_load_measurer.cpp
    src/common.cpp
)

target_link_libraries(test_method_client
PRIVATE
    vsomeip3
)

add_executable(test_method_server 
    src/test_method_server.cpp 
    src/cpu_load_measurer.cpp
    src/common.cpp
)

target_link_libraries(test_method_server
PRIVATE
    vsomeip3
)

add_executable(test_event_client 
    src/test_event_client.cpp
    src/cpu_load_measurer.cpp
    src/common.cpp
)

target_link_libraries(test_event_client
PRIVATE
    vsomeip3
)

add_executable(test_event_server 
    src/test_event_server.cpp 
    src/cpu_load_measurer.cpp
    src/common.cpp
)

target_link_libraries(test_event_server
PRIVATE
    vsomeip3
)

# add_executable(test_mix_client
#     src/test_mix_client.cpp 
#     src/cpu_load_measurer.cpp
#     src/common.cpp
# )

# target_link_libraries(test_mix_client
# PRIVATE
#     vsomeip3
# )

add_executable(test_mix_server 
    src/test_mix_server.cpp 
    src/cpu_load_measurer.cpp
    src/common.cpp
)

target_link_libraries(test_mix_server
PRIVATE
    vsomeip3
)