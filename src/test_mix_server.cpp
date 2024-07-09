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
#include <cstring>

class TestMixServer {
public:
    TestMixServer(protocol_e protocol, std::size_t payload_size, std::uint32_t cycle) :
        app_(vsomeip::runtime::get()->create_application()),
        protocol_(protocol),
        request_payload_size_(payload_size + time_payload_size),
        notify_payload_size_(1000),
        number_of_send_(0),
        number_of_test_(0),
        number_of_send_total_(0),
        number_of_received_request_(0),
        number_of_received_request_total_ (0),
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
                  << request_payload_size_ - time_payload_size
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

        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, TEST_METHOD_ID,
                std::bind(&TestMixServer::on_message, this,std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, START_METHOD_ID,
                std::bind(&TestMixServer::on_message_start_measuring, this,std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, STOP_METHOD_ID,
                std::bind(&TestMixServer::on_message_stop_measuring, this,std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,TEST_INSTANCE_ID, SHUTDOWN_METHOD_ID,
                std::bind(&TestMixServer::on_message_shutdown, this,std::placeholders::_1));

        std::set<vsomeip::eventgroup_t> its_groups;
        its_groups.insert(TEST_EVENTGROUP_ID);
        app_->offer_event(
                TEST_SERVICE_ID,
                TEST_INSTANCE_ID,
                TEST_EVENT_ID,
                its_groups);
        return true;
    }

    void Start() {
        app_->start();
    }

    void stop(){
        std::cout<< "Stopping server......" << std::endl;
        blocked_ = true;
        app_->clear_all_handler();
        app_->stop_offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
        notify_cv_.notify_one();
        running_ = false;
        is_offered_ = true;
        is_start_ = false;
        if (notify_thread_.joinable()) {
            std::cout<< "Joining notify thread......" << std::endl;
            notify_thread_.join();
        } else {
            std::cout<< "Notify thread is not joinable......" << std::endl;
            notify_thread_.detach();
        }

        if(latencys_.size() != 0){
            const auto total_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0);
            uint32_t total_payload = request_payload_size_ * number_of_received_request_total_ + number_of_send_total_ * notify_payload_size_;
            double total_time = std::accumulate(latencys_.begin(), latencys_.end(), 0)/1000000.000 - cycle_/1000.000 * number_of_send_total_;
            double average_throughput = total_payload / total_time;
            
            std::cout << "request_payload_size_: " << request_payload_size_ << std::endl;
            std::cout << "number_of_received_request_total_: " << number_of_received_request_total_ << std::endl;
            std::cout << "number_of_send_total_: " << number_of_send_total_ << std::endl;
            std::cout << "notify_payload_size_: " << notify_payload_size_ << std::endl;
            std::cout << "total_time: " << total_time << " seconds" << std::endl;
            std::cout << "total_payload: " << total_payload << " bytes" << std::endl;
            std::cout << "cycle_: " << cycle_ << " milliseconds" << std::endl;
            std::cout << "average_throughput: " << average_throughput << " bytes/s" << std::endl;
            
            const double average_load(std::accumulate(cpu_loads_.begin(), cpu_loads_.end(), 0.0) / static_cast<double>(cpu_loads_.size()));
            std::cout << "Send " << number_of_send_total_
                << " notification messages, recevied and responded "
                << number_of_received_request_total_
                <<" requests in total. This caused: "
                << std::fixed << std::setprecision(2)
                << average_load << "% load in average (average of "
                << cpu_loads_.size() << " measurements), latency(us)["
                << total_latency
                << "], throughput(bytes/s)["
                << average_throughput
                << "]." << std::endl;
            std::cout << "目前该测试程序针对event的notify次数，不一定准确：该测试程序从收到开始测试的请求及开始统计notify函数的调用次数，"
                    <<"但每次调用notify函数时，不一定有客户端订阅event，即vsomeip底层并不一定发送notify报文，"
                    <<"notify的实际值小于等于统计值.建议以客户端的统计报文次数为准" << std::endl;
        }

        app_->stop();
    }

private:
    void notify() {
        ByteVec payload_data(notify_payload_size_);
        std::shared_ptr<vsomeip_v3::payload> notify_payload_ = vsomeip::runtime::get()->create_payload(payload_data);
        while (running_) {
            std::unique_lock<std::mutex> u_lock(notify_mutex_);
            notify_cv_.wait(u_lock, [this]{ return is_offered_; });
            while (is_start_){
                app_->notify(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENT_ID, notify_payload_);
                number_of_send_++;
                std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message> &request) {
        number_of_received_request_++;
        auto response = vsomeip::runtime::get()->create_response(request);
        if(request_payload_size_==0){
            std::memcpy(&request_payload_size_, request->get_payload()->get_data(), sizeof(std::size_t));
        }
        ByteVec payload_data(request_payload_size_);
        auto response_payload = vsomeip::runtime::get()->create_payload(payload_data);
        response->set_payload(response_payload);
        app_->send(response);
    }

    void on_message_start_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        std::cout<< "Start measuring......" << std::endl;
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
        number_of_send_total_ += number_of_send_;
        number_of_received_request_total_ += number_of_received_request_;
        timespec diff_ts = timespec_diff(before_, after_);
        auto latency_us = (diff_ts.tv_sec * 1000000) + (diff_ts.tv_nsec / 1000);//总的延迟
        latencys_.push_back(latency_us);
        number_of_test_++;
        std::cout <<"The No."<<number_of_test_<< " send " << std::setw(4) << std::setfill('0')
        << number_of_send_ << " notification messages.Received and Responded " << std::setw(4)<<std::setfill('0')
        <<number_of_received_request_<<" requests. CPU load(%)["
        << std::fixed << std::setprecision(2)
        << (std::isfinite(cpu_load_measurer_.get_cpu_load()) ? cpu_load_measurer_.get_cpu_load() : 0.0)
        <<"], latency(us)["
        <<latency_us
        <<"]"<<std::endl;
        std::cout << "The No."<<number_of_test_<<" testing has ended, and the next round of testing is about to begin......" << std::endl;
        cpu_loads_.push_back(std::isfinite(cpu_load_measurer_.get_cpu_load()) ? cpu_load_measurer_.get_cpu_load() : 0.0);
        number_of_send_ = 0;
        number_of_received_request_ = 0;
    }

    void on_message_shutdown(const std::shared_ptr<vsomeip::message>& _request){
        (void)_request;
        std::cout << "Shutdown method was called, going down now."<<std::endl;
        stop();
    }

    std::shared_ptr<vsomeip::application> app_;
    protocol_e protocol_;
    std::size_t request_payload_size_,notify_payload_size_;
    uint32_t number_of_send_,number_of_send_total_;
    uint32_t number_of_received_request_,number_of_received_request_total_;

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

    std::vector<double> cpu_loads_;
    std::vector<uint64_t> latencys_;

    timespec before_,after_;
};

int main(int argc, char** argv) {
    bool use_tcp = false;
    bool be_quiet = false;
    std::uint32_t cycle = 50; // Default: 0.05s
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

    TestMixServer server(use_tcp ? protocol_e::PR_TCP : protocol_e::PR_UDP, payload_size, cycle);

    if (server.Init()) {
        server.Start();
        return 0;
    } else {
        return 1;
    }

    return 0;
}