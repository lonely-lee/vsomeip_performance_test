#ifndef VSOMEIP_PERFORMANCE_TEST_COMMON_HPP
#define VSOMEIP_PERFORMANCE_TEST_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include <fstream>  
#include <sstream>  
#include <string>  
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <sys/socket.h>  
#include <arpa/inet.h>  
#include <unistd.h>  

#define TEST_SERVICE_ID       0x1234
#define TEST_INSTANCE_ID      0x5678
#define TEST_METHOD_ID        0x0421
#define START_METHOD_ID       0x0422
#define STOP_METHOD_ID        0x0423
#define SHUTDOWN_METHOD_ID    0x0424

#define TEST_EVENT_UDP_ID         0x8778
#define TEST_EVENT_TCP_ID         0x8779

#define TEST_EVENTGROUP_ID    0x4465

enum class protocol_e {
    PR_UNKNOWN,
    PR_TCP,
    PR_UDP
};

using ByteType = std::uint8_t;
using ByteVec = std::vector<ByteType>;

constexpr std::size_t time_payload_size = sizeof(timespec) + sizeof(std::size_t);
static constexpr uint8_t load_test_data(0xDD);

[[nodiscard]] std::string timespec_to_str(const timespec &ts);

bool timespec_to_bytes(const timespec &ts, ByteType *bytes, std::size_t size);

bool bytes_to_timespec(const ByteType *bytes, std::size_t size, timespec &ts);

bool get_now_time(timespec &ts);

bool get_mem_usage(std::size_t& mem_sizes);

[[nodiscard]] timespec timespec_diff(const timespec &start, const timespec &end);


/* Handle Data with method*/
bool handleDatas(std::string filename, bool is_udp,
                uint32_t number_of_request, uint32_t number_of_calls, std::size_t payload_size,
                const unsigned long average_throughput,const unsigned long average_latency);

/* Handle Data with method*/
bool handleDatas(std::string filename, bool is_udp,uint32_t cycle,
                uint32_t number_of_notify, uint32_t number_of_test, std::size_t payload_size,
                const unsigned long average_throughput,const unsigned long average_latency);


#endif // VSOMEIP_PERFORMANCE_TEST_COMMON_HPP