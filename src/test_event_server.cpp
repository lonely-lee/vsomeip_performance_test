#include "common.hpp"
#include "cpu_load_measurer.hpp"

#include <chrono>
#include <sstream>
#include <iostream>
#include <condition_variable>
#include <memory>
#include <thread>
#include <mutex>
#include <vsomeip/vsomeip.hpp>
#include <unistd.h>
#include <cmath>
#include <numeric>
#include <iomanip>

class TestEventServer {
public:
    TestEventServer(protocol_e protocol, std::size_t payload_size, std::uint32_t cycle) :
        app_(vsomeip::runtime::get()->create_application()),
        protocol_(protocol),
        payload_size_(payload_size + time_payload_size),
        payload_(vsomeip::runtime::get()->create_payload()),
        running_(true),
        blocked_(false),
        is_offered_(false),
        cycle_(cycle),
        notify_thread_([this]{ notify(); }),
        cpu_load_measurer_(static_cast<std::uint32_t>(::getpid())) {
    }

    bool Init() {
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }

        std::cout << "Server settings [protocol="
                  << (protocol_ == protocol_e::PR_TCP ? "TCP" : "UDP")
                  << " payload size="
                  << payload_size_ - time_payload_size
                  << " cycle="
                  << cycle_
                  << "ms]"
                  << std::endl;

        app_->register_state_handler([this](vsomeip::state_type_e state){
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                std::lock_guard<std::mutex> g_lock(offer_mutex_);
                blocked_ = true;
                app_->offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
                is_offered_ = true;
            }
        });
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, START_METHOD_ID,
                std::bind(&TestEventServer::on_message_start_measuring, this,std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, STOP_METHOD_ID,
                std::bind(&TestEventServer::on_message_stop_measuring, this,std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, SHUTDOWN_METHOD_ID,
                std::bind(&TestEventServer::on_message_shutdown, this,std::placeholders::_1));

        std::set<vsomeip::eventgroup_t> its_groups;
        its_groups.insert(TEST_EVENTGROUP_ID);
        app_->offer_event(
                TEST_SERVICE_ID,
                TEST_INSTANCE_ID,
                TEST_EVENT_ID,
                its_groups);

        ByteVec payload_data(payload_size_);
        payload_->set_data(payload_data);

        return true;
    }

    void Start() {
        app_->start();
    }

    void stop(){
        blocked_ = true;
        app_->clear_all_handler();
        app_->stop_offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
        notify_cv_.notify_one();
        if (notify_thread_.joinable()) {
            notify_thread_.join();
        } else {
            notify_thread_.detach();
        }
        app_->stop();
    }

private:
    void notify() {
        timespec ts;
        while (running_) {
            std::unique_lock<std::mutex> u_lock(notify_mutex_);
            notify_cv_.wait(u_lock, [this]{ return (is_offered_&&is_start_); });
            while (is_start_)
            {
                get_now_time(ts);
                timespec_to_bytes(ts, payload_->get_data(), payload_->get_length());
                app_->notify(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENT_ID, payload_);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
            }
        }
    }

    void on_message_start_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        is_start_ = true;
        cpu_load_measurer_.start();
        get_now_time(before_);

        std::lock_guard<std::mutex> g_lock(notify_mutex_);
        notify_cv_.notify_one();
    }

    void on_message_stop_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        is_start_ = false;
        cpu_load_measurer_.stop();
        get_now_time(after_);
        timespec diff_ts = timespec_diff(before_, after_);
        auto latency_us = ((diff_ts.tv_sec * 1000000) + (diff_ts.tv_nsec / 1000)) / (2 * number_of_send_);//请求到响应来回除以二，同时除以发送请求次数
        latencys_.push_back(latency_us);
        number_of_test_++;
        std::cout <<"The No."<<number_of_test_<< " Received " << std::setw(4) << std::setfill('0')
        << number_of_send_ << " messages. CPU load(%)["
        << std::fixed << std::setprecision(2)
        << (std::isfinite(cpu_load_measurer_.get_cpu_load()) ? cpu_load_measurer_.get_cpu_load() : 0.0)
        <<"], latency(us)["
        <<latency_us
        <<"]"<<std::endl;
        cpu_loads_.push_back(std::isfinite(cpu_load_measurer_.get_cpu_load()) ? cpu_load_measurer_.get_cpu_load() : 0.0);
        number_of_send_ = 0;
    }

    void on_message_shutdown(const std::shared_ptr<vsomeip::message>& _request){
        (void)_request;
        std::cout << "Shutdown method was called, going down now."<<std::endl;
        const auto average_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0) / static_cast<uint64_t>(latencys_.size());
        const auto average_throughput = payload_size_ * (static_cast<uint64_t>(latencys_.size())) * 1000000 / std::accumulate(latencys_.begin(), latencys_.end(), 0);
        const double average_load(std::accumulate(cpu_loads_.begin(), cpu_loads_.end(), 0.0) / static_cast<double>(cpu_loads_.size()));
        std::cout << "Send: " << number_of_send_total_
            << " notification messages in total. This caused: "
            << std::fixed << std::setprecision(2)
            << average_load << "% load in average (average of "
            << cpu_loads_.size() << " measurements), latency(us)["
            << average_latency
            << "], throughput(bytes/s)["
            << average_throughput
            << "]." << std::endl;

        stop();
    }

    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::payload> payload_;
    protocol_e protocol_;
    std::size_t payload_size_;
    uint32_t number_of_send_,number_of_send_total_;

    bool is_start_;
    uint32_t number_of_test_;
    cpu_load_measurer cpu_load_measurer_;

    std::mutex offer_mutex_;
    std::condition_variable offer_cv_;
    std::mutex notify_mutex_;
     std::condition_variable notify_cv_;
    bool running_;
    bool blocked_;
    bool is_offered_;
    std::uint32_t cycle_;

    std::thread notify_thread_;
    std::thread offer_thread_;

    std::vector<double> cpu_loads_;
    std::vector<uint64_t> latencys_;

    timespec before_,after_;
};

int main(int argc, char** argv) {
    bool use_tcp = false;
    bool be_quiet = false;
    std::uint32_t cycle = 1000; // Default: 1s
    std::size_t payload_size = 1024;

    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string size_arg("--size");
    std::string cycle_arg("--cycle");
    
    int i = 1;
    while (i < argc) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
        } else if (udp_enable == argv[i]) {
            use_tcp = false;
        } else if (size_arg == argv[i] && i+1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> payload_size;
        } else if (cycle_arg == argv[i] && i+1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> cycle;
        }
        i++;
    }

    TestEventServer server(use_tcp ? protocol_e::PR_TCP : protocol_e::PR_UDP, payload_size, cycle);

    if (server.Init()) {
        server.Start();
        return 0;
    } else {
        return 1;
    }

    return 0;
}