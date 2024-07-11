#include "common.hpp"

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
#include <csignal>

class TestEventServer {
public:
    TestEventServer(protocol_e protocol, std::size_t payload_size, std::uint32_t cycle) :
        app_(vsomeip::runtime::get()->create_application()),
        protocol_(protocol),
        payload_size_(payload_size + time_payload_size),
        payload_(vsomeip::runtime::get()->create_payload()),
        number_of_send_(0),
        number_of_send_total_(0),
        number_of_test_(0),
        running_(true),
        is_offered_(false),
        cycle_(cycle),
        notify_thread_([this]{ notify(); }){
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
        {
            running_ = false;
            std::lock_guard<std::mutex> u_lcok(notify_mutex_);
            notify_cv_.notify_one();
            is_start_ = false;
        }
        if (std::this_thread::get_id() != notify_thread_.get_id()) {
            if (notify_thread_.joinable()) {
                notify_thread_.join();
            }
            else {
                notify_thread_.detach();
            }
        }
        {
            std::lock_guard<std::mutex> g_lcok(offer_mutex_);
            std::cout<< "Stopping server......" << std::endl;
            is_offered_ = true;
            app_->clear_all_handler();
            app_->stop_offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);          
        }

        if(latencys_.size() != 0){
            const auto average_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0) / static_cast<uint64_t>(latencys_.size());
            const auto average_throughput = payload_size_ * (static_cast<uint64_t>(latencys_.size())) * 1000000 / std::accumulate(latencys_.begin(), latencys_.end(), 0);
            std::cout << "Send: " << number_of_send_total_
                << " notification messages in total. This caused: latency(us/message)["
                << std::fixed << std::setprecision(2)
                << average_latency
                << "], throughput(bytes/s)["
                << average_throughput
                << "]." << std::endl;
        }

        app_->stop();
        std::cout<< "Server stopped." << std::endl;
    }

private:
    void notify() {
        while (running_) {
            std::unique_lock<std::mutex> u_lock(notify_mutex_);
            notify_cv_.wait(u_lock, [this]{ return is_offered_; });
            while (is_start_){
                app_->notify(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENT_ID, payload_);
                number_of_send_++;
                
                std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
            }
        }
    }

    void on_message_start_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        std::cout<< "Start measuring......" << std::endl;
        (void)_request;
        get_now_time(before_);

        std::lock_guard<std::mutex> g_lock(notify_mutex_);
        is_start_ = true;
        notify_cv_.notify_one();
    }

    void on_message_stop_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        is_start_ = false;
        get_now_time(after_);
        timespec diff_ts = timespec_diff(before_, after_);
        auto latency_us = ((diff_ts.tv_sec * 1000000000) + diff_ts.tv_nsec ) / (1000 * number_of_send_);//每条notification直接的时间间隔，包括notify的周期
        double throuput_bytes = payload_size_ * number_of_send_/(((diff_ts.tv_sec + diff_ts.tv_nsec / 1000000000.000)) - cycle_*number_of_send_);
        latencys_.push_back(latency_us);
        number_of_test_++;
        std::cout <<"The No."<<number_of_test_<< " send " << std::setw(4) << std::setfill('0')
        << number_of_send_ << " notification messages.cycle(ms)["
        <<cycle_<<"]. latency(us,Including cycles)["
        << std::fixed << std::setprecision(2)
        <<latency_us
        <<"]. throughput(bytes/s,Excluding cycles)["
        <<throuput_bytes<<"]"<<std::endl;
        std::cout << "The No."<<number_of_test_<<" testing has ended, and the next round of testing is about to begin......" << std::endl;
        number_of_send_total_ += number_of_send_;
        number_of_send_ = 0;
    }

    void on_message_shutdown(const std::shared_ptr<vsomeip::message>& _request){
        (void)_request;
        std::cout << "Shutdown method was called, going down now."<<std::endl;
        stop();
    }

    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::payload> payload_;
    protocol_e protocol_;
    std::size_t payload_size_;
    uint32_t number_of_send_,number_of_send_total_;

    bool is_start_;
    uint32_t number_of_test_;

    std::mutex offer_mutex_;
    std::mutex notify_mutex_;
     std::condition_variable notify_cv_;
    bool running_;
    bool is_offered_;
    std::uint32_t cycle_;

    std::thread notify_thread_;

    std::vector<uint64_t> latencys_;

    timespec before_,after_;
};


TestEventServer *its_sample_ptr(nullptr);
std::thread stop_thread;
void handle_signal(int _signal) {
    if (its_sample_ptr != nullptr &&
            (_signal == SIGINT || _signal == SIGTERM)) {
        stop_thread = std::thread([its_sample_ptr=its_sample_ptr]{
            its_sample_ptr->stop();
            std::cout<< "Server diaoyong stop." << std::endl;
        }); 
    }
}
int main(int argc, char** argv) {

    bool use_tcp = false;
    bool be_quiet = false;
    std::uint32_t cycle = 50; // Default: 1s
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
    its_sample_ptr = &server;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (server.Init()) {
        server.Start();
        if (stop_thread.joinable()) {
            stop_thread.join();
        } else{
            stop_thread.detach();
        }
        std::cout<< "return 0." << std::endl;
        return 0;
    } else {
        return 1;
    }
}