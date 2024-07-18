#include "common.hpp"

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

class cpu_load_test_service
{
public:
    cpu_load_test_service(protocol_e _protocol) :
                    protocol_(_protocol),
                    app_(vsomeip::runtime::get()->create_application()),
                    is_registered_(false),
                    blocked_(false),
                    number_of_tests_(0),
                    number_of_received_messages_(0),
                    number_of_received_messages_total_(0),
                    payload_size_(0),
                    offer_thread_(std::bind(&cpu_load_test_service::run, this))
    {
    }

    ~cpu_load_test_service() {
        {
            std::lock_guard<std::mutex> its_lock(mutex_);
            blocked_ = true;
            condition_.notify_one();
        }
        offer_thread_.join();
    }

    bool init()
    {
        std::lock_guard<std::mutex> its_lock(mutex_);

        if (!app_->init()) {
            std::cerr << "Couldn't initialize application"<<std::endl;
            return false;
        }
        app_->register_message_handler(TEST_SERVICE_ID,
                TEST_INSTANCE_ID, TEST_METHOD_ID,
                std::bind(&cpu_load_test_service::on_message, this,
                        std::placeholders::_1));

        app_->register_message_handler(TEST_SERVICE_ID,
                TEST_INSTANCE_ID, SHUTDOWN_METHOD_ID,
                std::bind(&cpu_load_test_service::on_message_shutdown, this,
                        std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,
                TEST_INSTANCE_ID, START_METHOD_ID,
                std::bind(&cpu_load_test_service::on_message_start_measuring, this,
                        std::placeholders::_1));
        app_->register_message_handler(TEST_SERVICE_ID,
                TEST_INSTANCE_ID, STOP_METHOD_ID,
                std::bind(&cpu_load_test_service::on_message_stop_measuring, this,
                        std::placeholders::_1));
        app_->register_state_handler(
                std::bind(&cpu_load_test_service::on_state, this,
                        std::placeholders::_1));
        return true;
    }

    void start()
    {
        std::cout << "Starting..."<<std::endl;
        app_->start();
    }

    void stop()
    {
        std::cout << "Stopping..."<<std::endl;
        app_->stop_offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
        app_->clear_all_handler();
        app_->stop();
    }

    void on_state(vsomeip::state_type_e _state)
    {
        std::cout << "Application " << app_->get_name() << " is "
                << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." :
                        "deregistered.")<<std::endl;

        if(_state == vsomeip::state_type_e::ST_REGISTERED)
        {
            if(!is_registered_)
            {
                is_registered_ = true;
                std::lock_guard<std::mutex> its_lock(mutex_);
                blocked_ = true;
                // "start" the run method thread
                condition_.notify_one();
            }
        }
        else
        {
            is_registered_ = false;
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _request)
    {
        number_of_received_messages_++;
        // send response
        auto response = vsomeip::runtime::get()->create_response(_request);
        if(payload_size_==0){
            std::memcpy(&payload_size_, _request->get_payload()->get_data(), sizeof(std::size_t));
            payload_size_ = static_cast<uint32_t>(payload_size_);
        }
        ByteVec payload_data(payload_size_);
        auto response_payload = vsomeip::runtime::get()->create_payload(payload_data);
        response->set_payload(response_payload);
        app_->send(response);
    }

    void on_message_start_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        //std::cout<<"收到的请求为开始测试:"<<_request->get_method()<<std::endl;
        std::cout << "Start measuring!"<<std::endl;
        get_now_time(before_);
    }

    void on_message_stop_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        get_now_time(after_);
        timespec diff_ts = timespec_diff(before_, after_);
        auto latency_us = ((diff_ts.tv_sec * 1000000) + (diff_ts.tv_nsec / 1000)) / (2 * number_of_received_messages_);//请求到响应来回除以二，同时除以发送请求次数
        //std::cout<<"收到的请求为结束测试:"<<_request->get_method()<<std::endl;

        latencys_.push_back(latency_us);
        std::cout <<"The No."<<++number_of_tests_<< " Received " << std::setw(4) << std::setfill('0')
        << number_of_received_messages_ << " messages. latency(us)["
        <<latency_us
        <<"], Throughput(Bytes/s)["
        <<(payload_size_*1000000/latency_us)
        <<"]."<<std::endl;
        number_of_received_messages_total_ += number_of_received_messages_;
        number_of_received_messages_ = 0;
    }

    void on_message_shutdown(
            const std::shared_ptr<vsomeip::message>& _request){
        (void)_request;

        std::cout<<">>>>>>>>>>>>>>>               TEST RESULT               <<<<<<<<<<<<<<<"<<std::endl;
        if(latencys_.empty()){
            std::cout <<"This test have no data"<<std::endl;
        } else{
            const auto average_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0) / static_cast<uint64_t>(latencys_.size());
            const auto average_throughput = payload_size_*1.000000 / average_latency*1000000;
            std::cout << "Protoc:"<<"(protocol_ == protocol_e::PR_TCP ?)"
                <<", execute "<< number_of_tests_ << " tests. Receive: " 
                << number_of_received_messages_total_
                << " request messages per test(all send response)."<<std::endl;
            std::cout<<"The byte size of both the request and response messages is "<<payload_size_<<" bytes. "<<std::endl;
            std::cout<<"This caused: "
                << "average latency(every message send or receive)["
                << std::setfill('0') << std::dec
                << average_latency / 1000000
                << "."
                << std::setw(6) << average_latency % 1000000  
                << "s], average throughput["
                <<average_throughput<<"(Byte/s)]."
                << std::endl;
            handleDatas("method_server_data.txt",(protocol_ == protocol_e::PR_UDP),
                        number_of_received_messages_total_/number_of_tests_,number_of_tests_,
                        payload_size_,average_throughput,average_latency);
        }
        stop();
    }

    void run()
    {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_) {
            condition_.wait(its_lock);
        }

        app_->offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
    }

private:
    protocol_e protocol_;
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
    std::uint32_t number_of_received_messages_;
    std::uint32_t number_of_received_messages_total_;
    std::uint32_t number_of_tests_;
    std::thread offer_thread_;

    std::vector<uint64_t> latencys_;
    timespec before_,after_;
    std::size_t payload_size_;
};


// this variables are changed via cmdline parameters
static protocol_e protocol(protocol_e::PR_UNKNOWN);
int main(int argc, char** argv)
{
    int i = 0;
    while (i < argc) {
        if(std::string("--protocol") == std::string(argv[i])
        || std::string("-p") == std::string(argv[i])) {
            if(std::string("udp") == std::string(argv[i+1]) ||
                    std::string("UDP") == std::string(argv[i+1])) {
                protocol = protocol_e::PR_UDP;
                i++;
            } else if(std::string("tcp") == std::string(argv[i+1]) ||
                    std::string("TCP") == std::string(argv[i+1])) {
                protocol = protocol_e::PR_TCP;
                i++;
            }
        } else if(std::string("--help") == std::string(argv[i])
        || std::string("-h") == std::string(argv[i])) {
            std::cout << "Available options:" << std::endl;
            std::cout << "--protocol|-p: valid values TCP or UDP" << std::endl;
        }
        i++;
    }


    if(protocol == protocol_e::PR_UNKNOWN) {
        std::cerr << "Please specify valid protocol mode, see --help" << std::endl;
        return(EXIT_FAILURE);
    }

    cpu_load_test_service test_service(protocol);
    if (test_service.init()) {
        test_service.start();
    }
}