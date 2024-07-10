#include "common.hpp"

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <numeric>
#include <iomanip>
#include <iostream>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <vector>
#include <vsomeip/vsomeip.hpp>
#include <sys/types.h>
#include <unistd.h>

using namespace std::chrono_literals;

class TestMethodClient {
public:
    TestMethodClient(protocol_e protocol, std::size_t payload_size, std::uint32_t cycle) :
        app_(vsomeip::runtime::get()->create_application()),
        protocol_(protocol),
        payload_size_(payload_size + time_payload_size),
        cycle_(cycle),
        request_(vsomeip::runtime::get()->create_request(protocol == protocol_e::PR_TCP)),
        running_(true),
        wait_for_availability_(true),
        wait_for_msg_acknowledged_(true),
        is_available_(false),
        number_of_test_(0),
        sender_(std::bind(&TestMethodClient::run, this)),
        number_of_acknowledged_messages_(0),
        number_of_calls_(1000),
        sliding_window_size_(5) {
    }

    bool Init() {
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }

        std::cout << "Client settings [protocol="
                  << (protocol_ == protocol_e::PR_TCP ? "TCP" : "UDP")
                  << " payload size="
                  << payload_size_ - time_payload_size
                  << " cycle="
                  << cycle_
                  << "ms]"
                  << std::endl;

        app_->register_state_handler([this](vsomeip::state_type_e state){
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->request_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
            }
        });

        app_->register_message_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_METHOD_ID, 
        [this](const std::shared_ptr<vsomeip::message> &response){
            on_message(response);
        });

        request_->set_service(TEST_SERVICE_ID);
        request_->set_instance(TEST_INSTANCE_ID);
        request_->set_method(TEST_METHOD_ID);

        ByteVec payload_data(payload_size_);
        std::memcpy(payload_data.data(), &payload_size_, sizeof(std::size_t));
        auto request_payload = vsomeip::runtime::get()->create_payload(payload_data);
        request_->set_payload(request_payload);

        app_->register_availability_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID,
        [this](vsomeip::service_t service, vsomeip::instance_t instance, bool is_available) {
            on_availability(service, instance, is_available);
        });

        return true;
    }

    void Start() {
        app_->start();
    }

    void Stop() {
        if (is_available_){//通知服务端测试结束
            std::cout<<"Stopping test"<<std::endl;
            request_->set_service(TEST_SERVICE_ID);
            request_->set_instance(TEST_INSTANCE_ID);
            request_->set_method(SHUTDOWN_METHOD_ID);
            app_->send(request_);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        running_ = false;
        {
            std::lock_guard<std::mutex> u_lcok(msg_acknowledged_mutex_);
            wait_for_msg_acknowledged_ = false;
            msg_acknowledged_cv_.notify_all();
        }

        {
            std::lock_guard<std::mutex> u_lcok(mutex_);
            wait_for_availability_ = false;
            is_available_ = false;
            cv_.notify_all();
        }
        if (sender_.joinable()) {
            sender_.join();
        } else {
            sender_.detach();
        }
        app_->clear_all_handler();
        app_->release_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);

        if(latencys_.empty()){
            std::cout <<"This test have no data"<<std::endl;
        } else{
            const auto average_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0) / static_cast<uint64_t>(latencys_.size());
            const auto average_throughput = payload_size_ * (static_cast<uint64_t>(latencys_.size())) * 1000000 / std::accumulate(latencys_.begin(), latencys_.end(), 0);
            std::cout << "Sent: " 
                << number_of_test_*number_of_calls_
                << "request messages in total (all receive response). This caused: "
                << "latency["
                << std::setfill('0') << std::dec
                << average_latency / 1000000
                << "."
                << std::setw(6) << average_latency % 1000000  
                << "s], throughput["
                <<average_throughput<<"(Byte/s)]"
                << std::endl;

            
            handleDatas(payload_size_,average_throughput,average_latency);
        }

        app_->stop();
    }

private:
    void run() {
        timespec before,after,diff_ts;
        while (running_) {
            {
                std::unique_lock<std::mutex> u_lock(mutex_);
                cv_.wait(u_lock, [this]{return !wait_for_availability_; });
                if (is_available_) {
                    send_service_start_measuring(true);
                    get_now_time(before);

                    for (size_t i = 0; i < number_of_calls_; i++)
                    {
                        app_->send(request_);
                        if(((i + 1 )% sliding_window_size_ == 0) || ((i + 1 ) == number_of_calls_)) {//如果请求发送完毕或达到一次性最大请求数量，则等待响应全部接收完毕（或再进行下一次发送）
                            std::unique_lock<std::mutex> u_lock(msg_acknowledged_mutex_);
                            msg_acknowledged_cv_.wait(u_lock, [this]{ return !wait_for_msg_acknowledged_; });
                            wait_for_msg_acknowledged_ = true;
                        }
                        if(!running_){
                            break;
                        }
                    }

                    get_now_time(after);
                    send_service_start_measuring(false);
                    diff_ts = timespec_diff(before, after);
                    auto latency_us = ((diff_ts.tv_sec * 1000000) + (diff_ts.tv_nsec / 1000)) / (2 * number_of_calls_);//请求到响应来回除以二,同时除以发送请求次数
                    latencys_.push_back(latency_us);
                    
                    number_of_test_++;
                    std::cout <<"The No."<<number_of_test_<<" Test "<< "sent " << std::setw(4) << std::setfill('0')
                            << number_of_calls_ << "request messages(all receive response). latency(us)["
                            << std::fixed << std::setprecision(2)
                            <<latency_us
                            <<"]"
                            << std::endl;
                }

            }
            std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
        }
    }

    void on_availability(vsomeip::service_t service, vsomeip::instance_t instance, bool is_available) {
        std::cout << "Service ["
                << std::setw(4) << std::setfill('0') << std::hex << service << "." << instance
                << "] is "
                << (is_available ? "available." : "NOT available.")
                <<std::dec<< std::endl;

        if (TEST_SERVICE_ID == service && TEST_INSTANCE_ID == instance) {
            if (is_available_  && !is_available) {
                is_available_ = false;
                std::lock_guard<std::mutex> g_lcok(mutex_);
                wait_for_availability_ = true;
            } else if (is_available && !is_available_) {
                is_available_ = true;
                std::lock_guard<std::mutex> g_lcok(mutex_);
                wait_for_availability_ = false;
                cv_.notify_one();
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message> &response) {
        number_of_acknowledged_messages_++;
        if (is_available_ && (number_of_acknowledged_messages_==number_of_calls_)) {
            std::cout<<"Received all messages:"<<number_of_acknowledged_messages_<<std::endl;
            std::lock_guard< std::mutex > u_lock(msg_acknowledged_mutex_);
            wait_for_msg_acknowledged_ = false;
            msg_acknowledged_cv_.notify_one();
            number_of_acknowledged_messages_=0;
        } else if(number_of_acknowledged_messages_ % sliding_window_size_ ==0){
            std::cout<<"Received one message:"<<number_of_acknowledged_messages_<<std::endl;
            std::lock_guard< std::mutex > u_lock(msg_acknowledged_mutex_);
            wait_for_msg_acknowledged_ = false;
            msg_acknowledged_cv_.notify_one();
        }
    }

    void send_service_start_measuring(bool _start_measuring) {
        std::shared_ptr<vsomeip::message> m = vsomeip::runtime::get()->create_request(protocol_ == protocol_e::PR_TCP);
        m->set_service(TEST_SERVICE_ID);
        m->set_instance(TEST_INSTANCE_ID);
        _start_measuring ? m->set_method(START_METHOD_ID) : m->set_method(STOP_METHOD_ID);
        app_->send(m);
    }

    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> request_;
    protocol_e protocol_;
    std::size_t payload_size_;
    std::uint32_t cycle_;

    std::mutex mutex_;
    std::mutex msg_acknowledged_mutex_;
    std::condition_variable cv_;
    std::condition_variable msg_acknowledged_cv_;
    bool running_;
    bool wait_for_availability_;
    bool wait_for_msg_acknowledged_;
    bool is_available_;
    std::uint64_t number_of_test_;
    uint64_t number_of_acknowledged_messages_;
    std::vector<std::uint64_t> latencys_;
    std::vector<std::size_t> throughput_;
    uint32_t number_of_calls_;
    uint32_t sliding_window_size_;

    std::thread sender_;
};

TestMethodClient *its_sample_ptr(nullptr);
std::thread stop_thread;
void handle_signal(int _signal) {
    if (its_sample_ptr != nullptr &&
            (_signal == SIGINT || _signal == SIGTERM)) {
        stop_thread = std::thread([its_sample_ptr=its_sample_ptr]{
            its_sample_ptr->Stop();
        }); 
    }
}

int main(int argc, char** argv) {
    bool use_tcp = false;
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

    TestMethodClient client(use_tcp ? protocol_e::PR_TCP : protocol_e::PR_UDP, payload_size, cycle);
    its_sample_ptr = &client;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    if (client.Init()) {
        client.Start();
        if (stop_thread.joinable()) {
            stop_thread.join();
        }
        return 0;
    } else {
        return 1;
    }
}