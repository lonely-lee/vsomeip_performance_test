#include "common.hpp"
#include <cstring>
#include <thread>

thread_local char time_char[50];

[[nodiscard]] std::string timespec_to_str(const timespec &ts) {
    tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);
    char format[] = "%Y-%m-%d %H:%M:%S";
    strftime(time_char, sizeof(time_char), format, &tm_info);
    auto msec = ts.tv_nsec / 1000;
    snprintf(time_char + strlen(time_char), sizeof(time_char) - strlen(time_char), ".%06ld", msec);

    return std::string(time_char);
}

bool timespec_to_bytes(const timespec &ts, ByteType *bytes, std::size_t size) {
    if (size > time_payload_size) {
        std::memcpy(bytes, &ts.tv_sec, sizeof(ts.tv_sec));
        std::memcpy(bytes + sizeof(ts.tv_sec), &ts.tv_nsec, sizeof(ts.tv_nsec));
        return true;
    }

    return false;
}

bool bytes_to_timespec(const ByteType *bytes, std::size_t size, timespec &ts) {
    if (size > time_payload_size) {
        std::memcpy(&ts.tv_sec, bytes, sizeof(ts.tv_sec));
        std::memcpy(&ts.tv_nsec, bytes + sizeof(ts.tv_sec), sizeof(ts.tv_nsec));
        return true;
    }

    return false;
}

bool get_now_time(timespec &ts) {
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return false;
    }

    return true;
}

[[nodiscard]] timespec timespec_diff(const timespec &start, const timespec &end) {
    timespec diff;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        diff.tv_sec = end.tv_sec - start.tv_sec - 1;
        diff.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        diff.tv_sec = end.tv_sec - start.tv_sec;
        diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    return diff;
}

bool get_mem_usage(std::size_t& mem_sizes) {
    bool is_found = false;
    std::ifstream file("/proc/self/status");  
    if (!file.is_open()) {  
        return is_found; 
    }  
    std::string line;  
    while (std::getline(file, line)) {   
        size_t pos = line.find("VmRSS:");  
        if (pos != std::string::npos) {  
            std::string numberStr = line.substr(pos + 6);  
            std::istringstream iss(numberStr);  
            if (iss >> mem_sizes) { 
                is_found = true;  
                break; 
            }  
        }
    }  
    return is_found; 
}

/*Data Handle */
bool handleDatas(std::size_t payload_size,const unsigned long average_throughput,const unsigned long average_latency,const double average_cpu_load,const double average_load_no_zero){
    std::ofstream outfile("data.txt", std::ios::app);
    if (!outfile.is_open()) {  
        return false;  
    }  
    std::ifstream infile("data.txt");  
    std::string line;  
    if (!std::getline(infile, line)) {
        outfile << "payload_size average_throughput average_latency average_cpu_load" << std::endl;  
    }  
    infile.close();
    outfile << payload_size << " " << average_throughput <<" "<< average_latency<<" "<<average_cpu_load<<" "<<average_load_no_zero<<std::endl;  
    outfile.close();
    return true;
}