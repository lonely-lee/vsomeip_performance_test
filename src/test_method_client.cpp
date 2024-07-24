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
#include <atomic>
using namespace std::chrono_literals;



class cpu_load_test_client
{
public:
    cpu_load_test_client(protocol_e _protocol, std::uint32_t number_of_request,std::uint32_t _number_of_test,
                        std::uint32_t _payload_size) :
            protocol_(_protocol),
            app_(vsomeip::runtime::get()->create_application()),
            request_(vsomeip::runtime::get()->create_request(protocol_ == protocol_e::PR_TCP)),
            sliding_window_size_(_number_of_test),
            wait_for_availability_(true),
            is_available_(false),
            is_ready_(false),
            is_over_(false),
            number_of_requests_(number_of_request),
            number_of_test_(_number_of_test),
            number_of_test_current_(0),
            number_of_sent_messages_(0),
            number_of_sent_messages_total_(0),
            number_of_acknowledged_messages_(0),
            payload_size_(_payload_size),
            wait_for_all_msg_acknowledged_(true),
            initialized_(false),
            sender_(std::bind(&cpu_load_test_client::run, this)) {
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application"<<std::endl;;
            return;
        }
        initialized_ = true;
        app_->register_state_handler(
                std::bind(&cpu_load_test_client::on_state, this,
                        std::placeholders::_1));

        app_->register_message_handler(vsomeip::ANY_SERVICE,
                vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
                std::bind(&cpu_load_test_client::on_message, this,
                        std::placeholders::_1));

        app_->register_availability_handler(TEST_SERVICE_ID,
                TEST_INSTANCE_ID,
                std::bind(&cpu_load_test_client::on_availability, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
        std::cout << "Starting..."<<std::endl;
        app_->start();
    }

    ~cpu_load_test_client() {
        {
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_for_availability_ = false;
            condition_.notify_one();
        }
        {
            std::lock_guard<std::mutex> its_lock(all_msg_acknowledged_mutex_);
            wait_for_all_msg_acknowledged_ = false;
            all_msg_acknowledged_cv_.notify_one();
        }
        sender_.join();
    }

private:
    void stop() {
        std::cout << "Stopping..."<<std::endl;
        shutdown_service();
        app_->clear_all_handler();

        std::cout<<">>>>>>>>>>>>>>>               TEST RESULT               <<<<<<<<<<<<<<<"<<std::endl;
        if(latencys_.empty()){
            std::cout <<"This test have no data"<<std::endl;
        } else{
            const unsigned long average_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0) / static_cast<uint64_t>(latencys_.size());
            unsigned long int average_throughput = payload_size_*1.000000 / average_latency*1000000;
            //std::cout<<"total_payload"<<total_payload<<std::endl;
            std::cout << "Protoc:"<<(protocol_ == protocol_e::PR_TCP ? "TCP" : "UDP" )
                <<", execute "<< number_of_test_ << " tests. Sent: " 
                << number_of_requests_
                << " request messages per test(all receive response)."<<std::endl;
            std::cout<<"The byte size of both the request and response messages is "<<payload_size_<<" bytes. "<<std::endl;
            std::cout<<"This caused: "
                << "average latency(every message send or receive)["
                << std::setfill('0') << std::dec
                << average_latency / 1000000
                << "."
                << std::setw(6) << average_latency % 1000000  
                << "s], average throughput["
                <<average_throughput<<"(Byte/s)]"
                << std::endl;

            
            handleDatas("./../result/method_client_data.txt",(protocol_ == protocol_e::PR_UDP),
                        number_of_requests_,number_of_test_,payload_size_,
                        average_throughput,average_latency);
        }
    }

    void on_state(vsomeip::state_type_e _state) {
        if(_state == vsomeip::state_type_e::ST_REGISTERED)
        {
            app_->request_service(TEST_SERVICE_ID,
                    TEST_INSTANCE_ID);
        }
    }

    void on_availability(vsomeip::service_t _service,
                         vsomeip::instance_t _instance, bool _is_available) {
        std::cout << "Service [" << std::setw(4) << std::setfill('0')
                << std::hex << _service << "." << _instance << "] is "
                << (_is_available ? "available." : "NOT available.")<<std::dec<<std::endl;

        if (TEST_SERVICE_ID == _service
                && TEST_INSTANCE_ID == _instance) {
            if (is_available_ && !_is_available) {
                is_available_ = false;
            } else if (_is_available && !is_available_) {
                is_available_ = true;
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_for_availability_ = false;
                condition_.notify_one();
            }
        }
    }
    void on_message(const std::shared_ptr<vsomeip::message> &_response) {
        //std::cout<<"resp length:"<<_response->get_length()<<std::endl;
        if(_response->get_method() == START_METHOD_ID){
            std::cout<<"receive start response message"<<std::endl;
            std::lock_guard<std::mutex> lk1(all_msg_acknowledged_mutex_);
            is_ready_ = true;
            all_msg_acknowledged_cv_.notify_one();
            return;
        } else if(_response->get_method() == STOP_METHOD_ID){
            std::cout<<"receive stop response message"<<std::endl;
            std::lock_guard<std::mutex> lk1(all_msg_acknowledged_mutex_);
            is_over_ = true;
            all_msg_acknowledged_cv_.notify_one();
            return;
        }
        number_of_acknowledged_messages_++;
        {
            // We notify the sender thread only if all sent messages have been acknowledged
            if(number_of_acknowledged_messages_ == number_of_test_current_)
            {
                std::lock_guard<std::mutex> lk(all_msg_acknowledged_mutex_);
                number_of_acknowledged_messages_ = 0;
                wait_for_all_msg_acknowledged_ = false;
                all_msg_acknowledged_cv_.notify_one();
            }
            else if(number_of_acknowledged_messages_ % sliding_window_size_ == 0)
            {
                std::lock_guard<std::mutex> lk(all_msg_acknowledged_mutex_);
                wait_for_all_msg_acknowledged_ = false;
                all_msg_acknowledged_cv_.notify_one();
            }
        }
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (wait_for_availability_) {
            condition_.wait(its_lock);
        }

        request_->set_service(TEST_SERVICE_ID);
        request_->set_instance(TEST_INSTANCE_ID);
        request_->set_method(TEST_METHOD_ID);
        std::shared_ptr<vsomeip::payload> payload = vsomeip::runtime::get()->create_payload();
        std::vector<vsomeip::byte_t> payload_data(payload_size_);;
        std::memcpy(payload_data.data(), &payload_size_, sizeof(std::size_t));
        payload->set_data(payload_data);
        request_->set_payload(payload);

        // lock the mutex
        for(std::uint32_t i=1; i <= number_of_test_; i++) {
            number_of_test_current_ = i;
            sliding_window_size_ = 10;
            timespec before,after,diff_ts;
            std::cout<<"Testing begins!"<<std::endl;
            send_service_start_measuring(true);
            {
                // 等待服务端开始测试的响应.
                std::unique_lock<std::mutex> lk(all_msg_acknowledged_mutex_);
                all_msg_acknowledged_cv_.wait(lk, [&](){return is_ready_;});
                is_ready_ = false;
            }
            get_now_time(before);
            {
                // lk在发送请求完成后销毁
                std::unique_lock<std::mutex> lk(all_msg_acknowledged_mutex_);
                send_messages_async(lk, number_of_requests_);
            }
            get_now_time(after);
            send_service_start_measuring(false);
            {
                std::unique_lock<std::mutex> lk(all_msg_acknowledged_mutex_);
                all_msg_acknowledged_cv_.wait(lk, [&](){return is_over_;});
                is_over_ = false;
            }
            diff_ts = timespec_diff(before, after);
            auto latency_us = ((diff_ts.tv_sec * 1000000) + (diff_ts.tv_nsec / 1000)) / (2 * number_of_requests_);//请求到响应来回除以二,同时除以发送请求次数
            latencys_.push_back(latency_us);
            std::cout <<"The No."<<number_of_test_current_<<" Test "<< " sent " << std::setw(4) << std::setfill('0')
                << number_of_requests_ << " request messages(all receive response). latency(us)["
                << std::fixed << std::setprecision(2)
                <<latency_us
                <<"], Throughput(Bytes/s)["
                <<(payload_size_*1.000000/latency_us*1000000)
                <<"]."<<std::endl;
        }

        wait_for_availability_ = true;

        stop();
        sleep(1);//等待shutdown报文发送完毕
        if (initialized_) {
            app_->stop();
        }
    }

    void send_messages_async(std::unique_lock<std::mutex>& lk, std::uint32_t _messages_to_send) {
        for (number_of_sent_messages_ = 0;
                number_of_sent_messages_ < _messages_to_send;
                number_of_sent_messages_++, number_of_sent_messages_total_++)
        {
            app_->send(request_);
            if((number_of_sent_messages_+1) % sliding_window_size_ == 0)
            {
                // wait until all send messages have been acknowledged
                while(wait_for_all_msg_acknowledged_) {
                    all_msg_acknowledged_cv_.wait(lk);
                }
                wait_for_all_msg_acknowledged_ = true;
            }
        }
    }

    void send_service_start_measuring(bool _start_measuring) {
        std::shared_ptr<vsomeip::message> m = vsomeip::runtime::get()->create_request(protocol_ == protocol_e::PR_TCP);
        m->set_service(TEST_SERVICE_ID);
        m->set_instance(TEST_INSTANCE_ID);
        _start_measuring ? m->set_method(START_METHOD_ID) : m->set_method(STOP_METHOD_ID);
        app_->send(m);
    }

    void shutdown_service() {
        request_->set_service(TEST_SERVICE_ID);
        request_->set_instance(TEST_INSTANCE_ID);
        request_->set_method(SHUTDOWN_METHOD_ID);
        request_->set_payload(nullptr);
        app_->send(request_);
    }

private:
    protocol_e protocol_;
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> request_;
    bool shutdown_service_at_end_;
    std::uint32_t sliding_window_size_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool wait_for_availability_;
    bool is_available_;
    bool is_ready_;
    bool is_over_;
    std::uint32_t number_of_test_;
    std::uint32_t number_of_test_current_;
    std::uint32_t number_of_sent_messages_;
    std::uint32_t number_of_sent_messages_total_;
    std::uint32_t number_of_acknowledged_messages_;
    std::uint32_t number_of_requests_;

    std::uint32_t payload_size_;

    bool wait_for_all_msg_acknowledged_;
    std::mutex all_msg_acknowledged_mutex_;
    std::condition_variable all_msg_acknowledged_cv_;
    std::atomic<bool> initialized_;
    std::thread sender_;

    std::vector<std::uint64_t> latencys_;
};


cpu_load_test_client *its_sample_ptr(nullptr);
std::thread stop_thread;
void handle_signal(int _signal) {
    if (its_sample_ptr != nullptr &&
            (_signal == SIGINT || _signal == SIGTERM)) {
        stop_thread = std::thread([its_sample_ptr=its_sample_ptr]{
            //its_sample_ptr->stop();
        }); 
    }
}

// this variables are changed via cmdline parameters
static protocol_e protocol(protocol_e::PR_UDP);
static std::uint32_t number_of_requests_(10);
static std::uint32_t number_of_test(0);
static std::uint32_t payload_size(40);

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
        } else if(std::string("--test") == std::string(argv[i])
        || std::string("-t") == std::string(argv[i])) {
            try {
                number_of_test = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid value for number of calls" << std::endl;
                return(EXIT_FAILURE);
            }
            i++;
        } else if(std::string("--request") == std::string(argv[i])
        || std::string("-r") == std::string(argv[i])) {
            try {
                number_of_requests_ = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid value for number of calls" << std::endl;
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
            std::cout << "--test|-t: number of message test to do" << std::endl;
            std::cout << "--request|-r: number of request to send" << std::endl;
            std::cout << "--payload-size|-pl: payload size in Bytes default: 40" << std::endl;
        }
        i++;
    }

    if(protocol == protocol_e::PR_UNKNOWN) {
        std::cerr << "Please specify valid protocol mode, see --help" << std::endl;
        return(EXIT_FAILURE);
    }
    if(!number_of_test) {
        std::cerr << "Please specify valid number of tests, see --help" << std::endl;
        return(EXIT_FAILURE);
    }

    cpu_load_test_client test_client_(protocol,number_of_requests_, number_of_test, payload_size);
    // its_sample_ptr = &test_client_;
    // signal(SIGINT, handle_signal);
    // signal(SIGTERM, handle_signal);
    // if (client.Init()) {
    //     client.Start();
    //     if (stop_thread.joinable()) {
    //         stop_thread.join();
    //     }
    //     return 0;
    // } else {
    //     return 1;
    // }
    return 0;
}