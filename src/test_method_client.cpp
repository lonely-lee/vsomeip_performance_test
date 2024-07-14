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

#if 0
using namespace std::chrono_literals;

class TestMethodClient {
public:
    TestMethodClient(protocol_e protocol, std::size_t payload_size, std::uint32_t cycle,std::uint32_t calls,std::uint32_t sliding) :
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
        number_of_calls_(calls),
        sliding_window_size_(sliding) {
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
            std::cout << "Protoc:"<<(protocol_ == protocol_e::PR_TCP ? "TCP" : "UDP" )
                <<", execute "<< number_of_test_ << "tests. Sent: " 
                << number_of_calls_
                << "request messages per test (all receive response)."
                <<"The byte size of both the request and response messages is "<<payload_size_<<" bytes. This caused: "
                << "average latency(every message send or receive)["
                << std::setfill('0') << std::dec
                << average_latency / 1000000
                << "."
                << std::setw(6) << average_latency % 1000000  
                << "s], average throughput["
                <<average_throughput<<"(Byte/s)]"
                << std::endl;

            
            handleDatas((protocol_ == protocol_e::PR_UDP),number_of_calls_,payload_size_,average_throughput,average_latency,sliding_window_size_);
        }

        std::cout << "Stopping..."<<std::endl;
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

                    for (uint32_t i = 0; i < number_of_calls_; i++)
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
                    std::cout<<"this Test finished."<<std::endl;
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
        std::cout<<"开始发送send_service_start_measuring:"<<std::endl;
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
    std::uint32_t cycle = 50; // Default: 1s
    uint32_t calls = 10;
    uint32_t sliding_window = 1;
    std::size_t payload_size = 1024;


    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string size_arg("--size");
    std::string cycle_arg("--cycle");
    std::string calls_arg("--calls");
    
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
        } else if (calls_arg == argv[i] && i+1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> calls;
        }
        i++;
    }

    TestMethodClient client(use_tcp ? protocol_e::PR_TCP : protocol_e::PR_UDP, payload_size, cycle,calls,sliding_window);
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

#elif 1


class cpu_load_test_client
{
public:
    cpu_load_test_client(protocol_e _protocol, std::uint32_t _number_of_calls,
                        std::uint32_t _payload_size, bool _call_service_sync,
                        bool _shutdown_service) :
            protocol_(_protocol),
            app_(vsomeip::runtime::get()->create_application()),
            request_(vsomeip::runtime::get()->create_request(protocol_ == protocol_e::PR_TCP)),
            call_service_sync_(_call_service_sync),
            shutdown_service_at_end_(_shutdown_service),
            sliding_window_size_(_number_of_calls),
            wait_for_availability_(true),
            is_available_(false),
            number_of_calls_(_number_of_calls),
            number_of_calls_current_(0),
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
        // shutdown the service
        if(shutdown_service_at_end_)
        {
            shutdown_service();
        }
        app_->clear_all_handler();

        if(latencys_.empty()){
            std::cout <<"This test have no data"<<std::endl;
        } else{
            const auto average_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0) / static_cast<uint64_t>(latencys_.size());
            const auto average_throughput = payload_size_ * (static_cast<uint64_t>(latencys_.size())) * 1000000 / std::accumulate(latencys_.begin(), latencys_.end(), 0);
            std::cout << "Protoc:"<<(protocol_ == protocol_e::PR_TCP ? "TCP" : "UDP" )
                <<", execute "<< number_of_calls_ << " tests. Sent: " 
                << number_of_calls_
                << " request messages per test(all receive response)."
                <<"The byte size of both the request and response messages is "<<payload_size_<<" bytes. This caused: "
                << "average latency(every message send or receive)["
                << std::setfill('0') << std::dec
                << average_latency / 1000000
                << "."
                << std::setw(6) << average_latency % 1000000  
                << "s], average throughput["
                <<average_throughput<<"(Byte/s)]"
                << std::endl;

            
            handleDatas((protocol_ == protocol_e::PR_UDP),number_of_calls_,payload_size_,average_throughput,average_latency,sliding_window_size_);
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
                << (_is_available ? "available." : "NOT available.")<<std::endl;

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

        number_of_acknowledged_messages_++;
        if(call_service_sync_)
        {
            // We notify the sender thread every time a message was acknowledged
            std::lock_guard<std::mutex> lk(all_msg_acknowledged_mutex_);
            wait_for_all_msg_acknowledged_ = false;
            all_msg_acknowledged_cv_.notify_one();
        }
        else
        {
            // We notify the sender thread only if all sent messages have been acknowledged
            if(number_of_acknowledged_messages_ == number_of_calls_current_)
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
        std::vector<vsomeip::byte_t> payload_data;
        payload_data.assign(payload_size_, load_test_data);
        payload->set_data(payload_data);
        request_->set_payload(payload);

        // lock the mutex
        for(std::uint32_t i=0; i <= number_of_calls_; i++) {
            number_of_calls_current_ = i;
            sliding_window_size_ = 1;
            std::unique_lock<std::mutex> lk(all_msg_acknowledged_mutex_);
            send_messages_async(lk, 10);
            sleep(1);//防止测试结束和测试开始的someip报文合并成一条报文，便于调试
            //call_service_sync_ ? send_messages_sync(lk, i) : send_messages_async(lk, i);
        }
        std::cout << "Sent: " << number_of_sent_messages_total_
            << " messages in total (excluding control messages). This caused: "
            <<std::endl;

        std::vector<double> results_no_zero;

        std::cout << "Sent: " << number_of_sent_messages_total_
            << " messages in total (excluding control messages). This caused: "
            <<std::endl;

        wait_for_availability_ = true;

        stop();
        sleep(1);//等待shutdown报文发送完毕
        if (initialized_) {
            app_->stop();
        }
    }


    void send_messages_sync(std::unique_lock<std::mutex>& lk, std::uint32_t _messages_to_send) {
        send_service_start_measuring(true);
        for (number_of_sent_messages_ = 0;
                number_of_sent_messages_ < _messages_to_send;
                number_of_sent_messages_++, number_of_sent_messages_total_++)
        {
            app_->send(request_);
            // wait until the send messages has been acknowledged
            while(wait_for_all_msg_acknowledged_) {
                all_msg_acknowledged_cv_.wait(lk);
            }
            wait_for_all_msg_acknowledged_ = true;
        }
        send_service_start_measuring(false);
        std::cout << "Synchronously sent " << std::setw(4) << std::setfill('0')
            << number_of_sent_messages_ << " messages. CPU load [%]: "
            <<std::endl;

    }

    void send_messages_async(std::unique_lock<std::mutex>& lk, std::uint32_t _messages_to_send) {
        timespec before,after,diff_ts;
        std::cout<<"Testing begins!"<<std::endl;
        send_service_start_measuring(true);
        get_now_time(before);
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
        get_now_time(after);
        send_service_start_measuring(false);
        diff_ts = timespec_diff(before, after);
        auto latency_us = ((diff_ts.tv_sec * 1000000) + (diff_ts.tv_nsec / 1000)) / (2 * number_of_calls_);//请求到响应来回除以二,同时除以发送请求次数
        latencys_.push_back(latency_us);

        // std::cout << "Asynchronously sent " << std::setw(4) << std::setfill('0')
        //     << number_of_sent_messages_ << " messages. CPU load [%]: "
        //     <<std::endl;
        std::cout <<"The No."<<"number_of_test_"<<" Test"<< " sent " << std::setw(4) << std::setfill('0')
                << number_of_calls_ << "request messages(all receive response). latency(us)["
                << std::fixed << std::setprecision(2)
                <<latency_us
                <<"]"
                << std::endl;
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
        app_->send(request_);
    }

private:
    protocol_e protocol_;
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> request_;
    bool call_service_sync_;
    bool shutdown_service_at_end_;
    std::uint32_t sliding_window_size_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool wait_for_availability_;
    bool is_available_;
    const std::uint32_t number_of_calls_;
    std::uint32_t number_of_calls_current_;
    std::uint32_t number_of_sent_messages_;
    std::uint32_t number_of_sent_messages_total_;
    std::uint32_t number_of_acknowledged_messages_;

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
static protocol_e protocol(protocol_e::PR_UNKNOWN);
static std::uint32_t number_of_calls(0);
static std::uint32_t payload_size(40);
static bool call_service_sync(true);
static bool shutdown_service(true);

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
        } else if(std::string("--calls") == std::string(argv[i])
        || std::string("-c") == std::string(argv[i])) {
            try {
                number_of_calls = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid value for number of calls" << std::endl;
                return(EXIT_FAILURE);
            }
            i++;
        } else if(std::string("--mode") == std::string(argv[i])
        || std::string("-m") == std::string(argv[i])) {
            if(std::string("sync") == std::string(argv[i+1]) ||
                    std::string("SYNC") == std::string(argv[i+1])) {
                call_service_sync = true;
                i++;
            } else if(std::string("async") == std::string(argv[i+1]) ||
                    std::string("ASYNC") == std::string(argv[i+1])) {
                call_service_sync = false;
                i++;
            }
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
            std::cout << "--calls|-c: number of message calls to do" << std::endl;
            std::cout << "--mode|-m: mode sync or async" << std::endl;
            std::cout << "--payload-size|-pl: payload size in Bytes default: 40" << std::endl;
        }
        i++;
    }

    if(protocol == protocol_e::PR_UNKNOWN) {
        std::cerr << "Please specify valid protocol mode, see --help" << std::endl;
        return(EXIT_FAILURE);
    }
    if(!number_of_calls) {
        std::cerr << "Please specify valid number of calls, see --help" << std::endl;
        return(EXIT_FAILURE);
    }

    cpu_load_test_client test_client_(protocol, number_of_calls, payload_size, call_service_sync, shutdown_service);
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
#endif