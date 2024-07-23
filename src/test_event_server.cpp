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
    TestEventServer(protocol_e protocol,uint32_t number_of_clients, std::size_t payload_size, std::uint32_t cycle) :
        app_(vsomeip::runtime::get()->create_application()),
        number_of_clients_(number_of_clients),
        number_of_request_start_(0),
        number_of_request_stop_(0),
        protocol_(protocol),
        event_id_((protocol_ == protocol_e::PR_TCP) ? TEST_EVENT_TCP_ID : TEST_EVENT_UDP_ID ),
        payload_size_(payload_size + time_payload_size),
        payload_(vsomeip::runtime::get()->create_payload()),
        number_of_send_(0),
        number_of_test_(0),
        number_of_send_total_(0),
        running_(true),
        blocked_(false),
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
        std::cout<< "Offering event service:"<<event_id_ << std::endl;
        app_->offer_event(
                TEST_SERVICE_ID,
                TEST_INSTANCE_ID,
                event_id_,
                its_groups);

        ByteVec payload_data(payload_size_);
        payload_->set_data(payload_data);

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
            auto average_latency = (std::accumulate(latencys_.begin(), latencys_.end(), 0) - cycle_* (number_of_send_total_ - number_of_test_) * 1000) / static_cast<uint64_t>(latencys_.size());
            average_latency = average_latency*number_of_test_/number_of_send_total_;
            const auto average_throughput = payload_size_ * number_of_send_total_ * 1000000 / (std::accumulate(latencys_.begin(), latencys_.end(), 0) - 1000 * cycle_ * (number_of_send_total_-number_of_test_));
            
            std::cout<<"This test is over:"
                    << "Protoc:"<<(protocol_ == protocol_e::PR_TCP ? "TCP" : "UDP" )
                    <<", execute "<< number_of_test_ << " tests. Sent: " 
                    << number_of_send_total_/number_of_test_<< " notification messages per test(all receive response)."<<std::endl;
            std::cout<<"The byte size of notification messages is "<<payload_size_<<" bytes. "<<std::endl;
            std::cout<<"this cause: average_latency(us,except cycle)["
                    <<average_latency
                    <<"], average throughput(Bytes/s,except cycle)["
                    <<average_throughput
                    <<"]"<<std::endl;

                handleDatas("event_server_data.txt",(protocol_ == protocol_e::PR_UDP),cycle_,number_of_send_total_/number_of_test_,
                            number_of_test_,payload_size_,average_throughput,average_latency);
        } else{
            std::cout<< "No latency data was recorded." << std::endl;
        }

        app_->stop();
    }

private:
    void notify() {
        while (running_) {
            std::unique_lock<std::mutex> u_lock(notify_mutex_);
            notify_cv_.wait(u_lock, [this]{ return is_offered_; });
            while (is_start_){
                //std::cout<< "Send notification message......:"<<number_of_send_ << std::endl;
                app_->notify(TEST_SERVICE_ID, TEST_INSTANCE_ID, event_id_, payload_);
                number_of_send_++;
                
                std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
            }
        }
    }

    void on_message_start_measuring(const std::shared_ptr<vsomeip::message>& _request){
        std::cout<< "Start measuring......" << std::endl;
        //(void)_request;
        number_of_request_start_++;
        start_resps_.push_back(vsomeip::runtime::get()->create_response(_request));
        if(number_of_request_start_ == number_of_clients_){
            for(auto resp:start_resps_){
                app_->send(resp);
            }
            number_of_request_start_=0;
            start_resps_.clear();
            is_start_ = true;
            get_now_time(before_);

            std::lock_guard<std::mutex> g_lock(notify_mutex_);
            notify_cv_.notify_one();
        }
    }

    void on_message_stop_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        std::cout<< "Stop measuring......" << std::endl;
        //(void)_request;

        number_of_request_stop_++;
        stop_resps_.push_back(vsomeip::runtime::get()->create_response(_request));
        if(number_of_request_stop_ == number_of_clients_){
            is_start_ = false;
            get_now_time(after_);
            for(auto resp:stop_resps_){
                app_->send(resp);
            }
            number_of_request_stop_=0;
            stop_resps_.clear();

            if(number_of_send_ != 0){
                timespec diff_ts = timespec_diff(before_, after_);
                auto latency_us = (diff_ts.tv_sec * 1000000) + diff_ts.tv_nsec / 1000;//每条notification直接的时间间隔，包括notify的周期
                auto throuput_bytes = payload_size_ * number_of_send_ * 1000000 / (latency_us - cycle_*1000 * (number_of_send_ - 1));//每条notification直接的时间间隔，包括notify的周期
                latencys_.push_back(latency_us);
                number_of_test_++;
                std::cout <<"The No."<<number_of_test_<< " test send " << std::setw(4) << std::setfill('0')
                << number_of_send_ << " notification messages.cycle(ms)["
                <<cycle_<<"]. latency(us,except cycles)["
                << std::fixed << std::setprecision(2)
                <<((latency_us- cycle_ * (number_of_send_ - 1) * 1000)/number_of_send_)
                <<"]. throughput(bytes/s,Excluding cycles)["
                <<throuput_bytes<<"]."<<std::endl;
                std::cout << "The No."<<number_of_test_<<" testing has ended, and the next round of testing is about to begin......" << std::endl;
                number_of_send_total_ += number_of_send_;
                number_of_send_ = 0;            
            } else{
                std::cout<< "No notification messages was sended." << std::endl;
            }
        }
    }

    void on_message_shutdown(const std::shared_ptr<vsomeip::message>& _request){
        (void)_request;
        std::cout << "Shutdown method was called, going down now."<<std::endl;
        stop();
    }

    std::shared_ptr<vsomeip::application> app_;
    uint32_t number_of_clients_;//客户端数量
    uint32_t number_of_request_stop_;
    uint32_t number_of_request_start_;
    std::vector<std::shared_ptr<vsomeip::message>> stop_resps_;
    std::vector<std::shared_ptr<vsomeip::message>> start_resps_;

    std::shared_ptr<vsomeip::payload> payload_;
    protocol_e protocol_;
    vsomeip::event_t event_id_;//成员变量赋值依赖声明次序，不依赖初始化列表顺序
    std::size_t payload_size_;
    uint32_t number_of_send_,number_of_send_total_;

    bool is_start_;
    uint32_t number_of_test_;

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

static protocol_e protocol(protocol_e::PR_UDP);
static std::uint32_t cycle(50);
static std::uint32_t payload_size(40);
uint32_t number_of_clients(1);

int main(int argc, char** argv) {
    bool use_tcp = false;
    bool be_quiet = false;
    std::uint32_t cycle = 50; // Default: 1s
    std::size_t payload_size = 1024;

    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string size_arg("--size");
    std::string cycle_arg("--cycle");
    
    int i = 0;
    while (i < argc) {
        if(std::string("--protocol") == std::string(argv[i])
        || std::string("-p") == std::string(argv[i])) {
            if(std::string("udp") == std::string(argv[i+1]) ||
                    std::string("UDP") == std::string(argv[i+1])) {
                std::cout<< "UDP is using:." << std::endl;
                protocol = protocol_e::PR_UDP;
                i++;
            } else if(std::string("tcp") == std::string(argv[i+1]) ||
                    std::string("TCP") == std::string(argv[i+1])) {
                std::cout<< "TCP is using:." << std::endl;
                protocol = protocol_e::PR_TCP;
                i++;
            }
        } else if(std::string("--cycle") == std::string(argv[i])
        || std::string("-c") == std::string(argv[i])) {
            try {
                cycle = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid value for number of cycle" << std::endl;
                return(EXIT_FAILURE);
            }
            i++;
        } else if(std::string("--clients") == std::string(argv[i])
        || std::string("-cl") == std::string(argv[i])) {
            try {
                number_of_clients = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid value for number of clients" << std::endl;
                return(EXIT_FAILURE);
            }
            i++;
        } else if(std::string("--payload-size") == std::string(argv[i])
        || std::string("-pl") == std::string(argv[i])) {
            try {
                payload_size = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
                std::cout<<"payload_size:"<<payload_size << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid values for payload size" << std::endl;
                return(EXIT_FAILURE);
            }
            i++;
        } else if(std::string("--help") == std::string(argv[i])
        || std::string("-h") == std::string(argv[i])) {
            std::cout << "Available options:" << std::endl;
            std::cout << "--protocol|-p: valid values TCP or UDP" << std::endl;
            std::cout << "--cycle|-c: number of notify per test" << std::endl;
            std::cout << "--clients|-cl: number of clients" << std::endl;
            std::cout << "--payload-size|-pl: payload size in Bytes default: 40" << std::endl;
        }
        i++;
    }

    TestEventServer server(protocol,number_of_clients, payload_size, cycle);

    if (server.Init()) {
        server.Start();
        return 0;
    } else {
        return 1;
    }

    return 0;
}