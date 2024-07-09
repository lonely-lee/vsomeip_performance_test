#include "common.hpp"
#include "cpu_load_measurer.hpp"

#include <csignal>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <cmath>
#include <vsomeip/vsomeip.hpp>
#include <numeric>
#include <unistd.h>

class TestMethodServer {
public:
    TestMethodServer() :
        app_(vsomeip::runtime::get()->create_application()),
        blocked_(false),
        offer_([this]{ run(); }),
        cpu_load_measurer_(static_cast<std::uint32_t>(::getpid())),
        cpu_load_measurer_2(static_cast<std::uint32_t>(::getpid())),
        number_of_test_(0),
        number_of_received_messages_total_(0),
        payload_size_(0),
        number_of_received_messages_(0) {}

    bool Init() {
        cpu_load_measurer_2.start();
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state){
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                std::lock_guard<std::mutex> g_lock(mutex_);
                blocked_ = true;
                cv_.notify_one();
            }
        });

        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, TEST_METHOD_ID,
                std::bind(&TestMethodServer::on_message, this,std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, START_METHOD_ID,
                std::bind(&TestMethodServer::on_message_start_measuring, this,std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, STOP_METHOD_ID,
                std::bind(&TestMethodServer::on_message_stop_measuring, this,std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, SHUTDOWN_METHOD_ID,
                std::bind(&TestMethodServer::on_message_shutdown, this,std::placeholders::_1));



        return true;
    }

    void Start() {
        app_->start();
    }

    void stop()
    {
        std::lock_guard<std::mutex> g_lcok(mutex_);
        blocked_ = true;
        app_->clear_all_handler();
        app_->stop_offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);

        cv_.notify_one();
        if (offer_.joinable()) {
            offer_.join();
        } else {
            offer_.detach();
        }
        std::cout << "Stopping..."<<std::endl;
        app_->stop();
        cpu_load_measurer_2.stop();
        cpu_load_measurer_2.print_cpu_load();
    }

private:

    void run() {
        std::unique_lock<std::mutex> u_lock(mutex_);
        cv_.wait(u_lock, [this] { return blocked_; });
        app_->offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
    }

    void on_message(const std::shared_ptr<vsomeip::message> &request) {
        number_of_received_messages_++;
        number_of_received_messages_total_++;
        auto response = vsomeip::runtime::get()->create_response(request);
        if(payload_size_==0){
            std::memcpy(&payload_size_, request->get_payload()->get_data(), sizeof(std::size_t));
        }
        ByteVec payload_data(payload_size_);
        auto response_payload = vsomeip::runtime::get()->create_payload(payload_data);
        response->set_payload(response_payload);
        app_->send(response);
    }


    void on_message_start_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        cpu_load_measurer_.start();
        get_now_time(before_);
    }

    void on_message_stop_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        cpu_load_measurer_.stop();
        get_now_time(after_);
        timespec diff_ts = timespec_diff(before_, after_);
        auto latency_us = ((diff_ts.tv_sec * 1000000) + (diff_ts.tv_nsec / 1000)) / (2 * number_of_received_messages_);//请求到响应来回除以二，同时除以发送请求次数
        latencys_.push_back(latency_us);
        number_of_test_++;
        std::cout <<"The No."<<number_of_test_<< " Received " << std::setw(4) << std::setfill('0')
        << number_of_received_messages_ << " messages. CPU load(%)["
        << std::fixed << std::setprecision(2)
        << (std::isfinite(cpu_load_measurer_.get_cpu_load()) ? cpu_load_measurer_.get_cpu_load() : 0.0)
        <<"], latency(us)["
        <<latency_us
        <<"]"<<std::endl;
        cpu_loads_.push_back(std::isfinite(cpu_load_measurer_.get_cpu_load()) ? cpu_load_measurer_.get_cpu_load() : 0.0);
        number_of_received_messages_ = 0;
    }

    void on_message_shutdown(const std::shared_ptr<vsomeip::message>& _request){
        (void)_request;
        std::cout << "Shutdown method was called, going down now."<<std::endl;
        const auto average_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0) / static_cast<uint64_t>(latencys_.size());
        const auto average_throughput = payload_size_ * (static_cast<uint64_t>(latencys_.size())) * 1000000 / std::accumulate(latencys_.begin(), latencys_.end(), 0);
        const double average_load(std::accumulate(cpu_loads_.begin(), cpu_loads_.end(), 0.0) / static_cast<double>(cpu_loads_.size()));
        std::cout << "Received: " << number_of_received_messages_total_
            << " in total (excluding control messages). This caused: "
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

    std::mutex mutex_;
    std::condition_variable cv_;
    bool blocked_;

    std::thread offer_;
    cpu_load_measurer cpu_load_measurer_,cpu_load_measurer_2;
    std::vector<double> cpu_loads_;
    std::vector<uint64_t> latencys_;
    int32_t number_of_test_;
    int32_t number_of_received_messages_,number_of_received_messages_total_;
    timespec before_,after_;
    std::size_t payload_size_;
};

TestMethodServer *its_sample_ptr(nullptr);
std::thread stop_thread;
void handle_signal(int _signal) {
    std::cout << "Signal received, going down now."<<std::endl;
    if (its_sample_ptr != nullptr &&
            (_signal == SIGINT || _signal == SIGTERM)) {
        stop_thread = std::thread([its_sample_ptr=its_sample_ptr]{
            its_sample_ptr->stop();
        }); 
    }
}

int main(int argc, char** argv) {
    uint32_t rev;
    cpu_load_measurer cpu_load_measurer_3(static_cast<std::uint32_t>(::getpid()));
    std::thread measurement_thread(&cpu_load_measurer::start_period, &cpu_load_measurer_3, 1); // 假设sampler_period为5秒
    TestMethodServer server;

    its_sample_ptr = &server;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (server.Init()) {
        server.Start();
        if (stop_thread.joinable()) {
            stop_thread.join();
        }
        rev = 0;
    } else {
        rev = 1;
    }
    std::cout<<"程序总体统计的cpu负载率为:"<<std::endl;
    cpu_load_measurer_3.exit_measure();
    if (measurement_thread.joinable()) {  
            measurement_thread.join();  
    } 
    double res = cpu_load_measurer_3.get_cpu_load_period();
    std::cout<<"app average cpuload"<<res<<std::endl;
    return rev;
}