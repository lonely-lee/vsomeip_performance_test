cmake_minimum_required(VERSION 3.13)
project(vsomeip_performance_test)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()

if(NOT DEFINED CMAKE_INSTALL_PREFIX)  
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/_output" CACHE PATH "Install path prefix, prepended onto install directories." FORCE)  
endif()  

# set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/_output")  
message("-- Install path:${CMAKE_INSTALL_PREFIX}")

find_package(vsomeip3 3.4.10 REQUIRED)

add_executable(test_method_client 
    src/test_method_client.cpp
    src/common.cpp
)

target_link_libraries(test_method_client
PRIVATE
    vsomeip3
)

add_executable(test_method_server 
    src/test_method_server.cpp 
    src/common.cpp
)

target_link_libraries(test_method_server
PRIVATE
    vsomeip3
)

add_executable(test_event_client 
    src/test_event_client.cpp
    src/common.cpp
)

target_link_libraries(test_event_client
PRIVATE
    vsomeip3
)

add_executable(test_event_server 
    src/test_event_server.cpp 
    src/common.cpp
)

target_link_libraries(test_event_server
PRIVATE
    vsomeip3
)

add_executable(test_mix_server 
    src/test_mix_server.cpp 
    src/common.cpp
)

target_link_libraries(test_mix_server
PRIVATE
    vsomeip3
)

if(NOT DEFINED TEST_IP_MASTER)
    set(TEST_IP_MASTER "192.168.31.101")
endif()
if(NOT DEFINED TEST_IP_SLAVE)
    set(TEST_IP_SLAVE "192.168.31.90")
endif()
configure_file(etc/vsomeip-event-service-master.json.in etc/vsomeip-event-service-master.json @ONLY)
configure_file(etc/vsomeip-event-service-slave.json.in etc/vsomeip-event-service-slave.json @ONLY)
configure_file(etc/vsomeip-event-client-master.json.in etc/vsomeip-event-client-master.json @ONLY)
configure_file(etc/vsomeip-event-client-slave.json.in etc/vsomeip-event-client-slave.json @ONLY)
configure_file(etc/vsomeip-method-service-master.json.in etc/vsomeip-method-service-master.json @ONLY)
configure_file(etc/vsomeip-method-service-slave.json.in etc/vsomeip-method-service-slave.json @ONLY)
configure_file(etc/vsomeip-method-client-master.json.in etc/vsomeip-method-client-master.json @ONLY)
configure_file(etc/vsomeip-method-client-slave.json.in etc/vsomeip-method-client-slave.json @ONLY)


# 安装可执行文件
install(TARGETS 
    test_method_client 
    test_method_server 
    test_event_client 
    test_event_server 
    test_mix_server 
    DESTINATION bin)

# 安装库文件  （相对于CMAKE_INSTALL_PREFIX）  CMAKE_PREFIX_PATH
set(LIB_INSTALL_DIR "lib")  
install(FILES  
    "${CMAKE_PREFIX_PATH}/lib/libboost_filesystem.so"
    "${CMAKE_PREFIX_PATH}/lib/libboost_filesystem.so.1.74.0"
    "${CMAKE_PREFIX_PATH}/lib/libboost_system.so"
    "${CMAKE_PREFIX_PATH}/lib/libboost_system.so.1.74.0"
    "${CMAKE_PREFIX_PATH}/lib/libboost_thread.so"
    "${CMAKE_PREFIX_PATH}/lib/libboost_thread.so.1.74.0"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-cfg.so"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-cfg.so.3"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-cfg.so.3.4.10"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-e2e.so"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-e2e.so.3"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-e2e.so.3.4.10"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-sd.so"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-sd.so.3"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3-sd.so.3.4.10"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3.so"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3.so.3"
    "${CMAKE_PREFIX_PATH}/lib/libvsomeip3.so.3.4.10" 
    DESTINATION ${LIB_INSTALL_DIR}  
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE  
)

# 安装配置文件  （相对于CMAKE_INSTALL_PREFIX）  
set(CONF_INSTALL_DIR "etc")  
install(FILES  
    "${CMAKE_CURRENT_BINARY_DIR}/etc/vsomeip-event-service-master.json"  
    "${CMAKE_CURRENT_BINARY_DIR}/etc/vsomeip-event-service-slave.json"
    "${CMAKE_CURRENT_BINARY_DIR}/etc/vsomeip-event-client-master.json"  
    "${CMAKE_CURRENT_BINARY_DIR}/etc/vsomeip-event-client-slave.json"
    "${CMAKE_CURRENT_BINARY_DIR}/etc/vsomeip-method-service-master.json"  
    "${CMAKE_CURRENT_BINARY_DIR}/etc/vsomeip-method-service-slave.json"
    "${CMAKE_CURRENT_BINARY_DIR}/etc/vsomeip-method-client-master.json"  
    "${CMAKE_CURRENT_BINARY_DIR}/etc/vsomeip-method-client-slave.json" 
    DESTINATION ${CONF_INSTALL_DIR}  
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE  
)

# 安装测试脚本文件  （相对于CMAKE_INSTALL_PREFIX）  
set(SCRIPT_INSTALL_DIR "bin")  
install(FILES  
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-event-master.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-event-slave.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-method-master.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-method-slave.sh"   
    DESTINATION ${SCRIPT_INSTALL_DIR}  
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE  
)