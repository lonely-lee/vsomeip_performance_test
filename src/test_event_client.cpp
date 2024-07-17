#include "common.hpp"

#include <condition_variable>
#include <csignal>
#include <unistd.h>
#include <cmath>
#include <thread>
#include <mutex>
#include <iomanip>
#include <iostream>
#include <vsomeip/vsomeip.hpp>
#include <numeric> // 包含accumulate的头文件
#include <atomic>

class TestEventClient {
public:
    TestEventClient(protocol_e _protocol,uint32_t number_of_notify, uint32_t test_total,uint32_t cycle) :
        protocol_(_protocol),
        app_(vsomeip::runtime::get()->create_application()),
        number_of_notify_(number_of_notify),
        number_of_test_(0),
        test_total_(test_total),
        number_of_notification_messages_(0),
        payload_size_(0),
        wait_for_msg_acknowledged_(true),
        wait_until_subscription_accepted_(true),
        cycle_(cycle),
        running_(true),
        sender_(std::bind(&TestEventClient::run, this)),
        initialized_(false) {
    }


    ~TestEventClient() {
        if(sender_.joinable()){
            sender_.join();
        }else {
            sender_.detach();
        }
    }

    bool Init() {
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }
        initialized_ = true;
        app_->register_state_handler([this](vsomeip::state_type_e state){
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->request_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
            }
        });

        app_->register_message_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENT_ID, 
        [this](const std::shared_ptr<vsomeip::message> &response){
            on_notification(response);
        });

        app_->register_availability_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID,
        [this](vsomeip::service_t service, vsomeip::instance_t instance, bool is_available) {
            on_availability(service, instance, is_available);
        });

        std::set<vsomeip::eventgroup_t> its_groups;
        its_groups.insert(TEST_EVENTGROUP_ID);
        app_->request_event(
                TEST_SERVICE_ID,
                TEST_INSTANCE_ID,
                TEST_EVENT_ID,
                its_groups);
        app_->register_subscription_status_handler(TEST_SERVICE_ID,
                TEST_INSTANCE_ID, TEST_EVENTGROUP_ID,
                TEST_EVENT_ID,
                std::bind(&TestEventClient::on_subscription_status_changed, this,
                          std::placeholders::_1, std::placeholders::_2,
                          std::placeholders::_3, std::placeholders::_4,
                          std::placeholders::_5));
        app_->subscribe(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENTGROUP_ID);

        return true;
    }

    void Start() {
        app_->start();
    }

    void stop() {
        std::cout<<"Stopping test"<<std::endl;
        if (is_available_){//通知服务端测试结束
            is_available_ = false;
            std::cout<<"Sending shutdown request"<<std::endl;
            std::shared_ptr<vsomeip::message> request_ = vsomeip::runtime::get()->create_request();
            request_->set_service(TEST_SERVICE_ID);
            request_->set_instance(TEST_INSTANCE_ID);
            request_->set_method(SHUTDOWN_METHOD_ID);
            app_->send(request_);
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
        running_ = false;

        app_->clear_all_handler();
        app_->unsubscribe(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENTGROUP_ID);
        app_->release_event(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENT_ID);
        app_->release_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);

        if(latencys_.size() != 0){
            uint64_t average_latency = (std::accumulate(latencys_.begin(), latencys_.end(), 0) - cycle_* test_total_*(number_of_notify_ - 1) * 1000) / static_cast<uint64_t>(latencys_.size());
            average_latency = average_latency/number_of_notify_;
            auto average_throughput = payload_size_ * number_of_notify_ * test_total_  * 1.000000 / (std::accumulate(latencys_.begin(), latencys_.end(), 0.0) - cycle_ * (number_of_notify_ - 1) * test_total_* 1000) * 1000000;
            std::cout<<"This test is over:"
                    << "Protoc:"<<(protocol_ == protocol_e::PR_TCP ? "TCP" : "UDP" )
                    <<", execute "<< test_total_ << " tests. Receive: " 
                    << number_of_notify_<< " notification messages per test(all receive response)."<<std::endl;
            std::cout<<"The byte size of notification messages is "<<payload_size_<<" bytes. "<<std::endl;
            std::cout<<"this cause: average_latency(us,except cycle)["
                    <<average_latency
                    <<"], average throughput(Bytes/s,except cycle)["
                    <<average_throughput
                    <<"]"<<std::endl;
            handleDatas("event_client_data.txt",(protocol_ == protocol_e::PR_UDP),cycle_,number_of_notify_,
                        number_of_test_,payload_size_,average_throughput,average_latency);
        } else{
            std::cout<<"No data to handle"<<std::endl;
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
                std::lock_guard<std::mutex> g_lcok(mutex_);
                is_available_ = false;
                wait_for_availability_ = true;
            } else if (is_available && !is_available_) {
                std::lock_guard<std::mutex> g_lcok(mutex_);
                is_available_ = true;
                wait_for_availability_ = false;
                cv_.notify_one();
            }
        }
    }

    void on_subscription_status_changed(const vsomeip::service_t _service,
                                        const vsomeip::instance_t _instance,
                                        const vsomeip::eventgroup_t _eventgroup,
                                        const vsomeip::event_t _event,
                                        const uint16_t error_code) {
        if (error_code == 0x0u) { // accepted
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_until_subscription_accepted_ = false;
            cv_.notify_one();
        }
    }

    void on_notification(const std::shared_ptr<vsomeip::message> &response) {
        number_of_notification_messages_++;
        if(payload_size_ == 0){
            payload_size_ = response->get_length();
            std::cout<<"payload_size_:"<<payload_size_<<std::endl;
        }
        //std::cout<<"The No."<<number_of_test_<<" test:"<<" Received  number of messages:"<<number_of_notification_messages_<<std::endl;
        if (is_available_ && (number_of_notification_messages_== number_of_notify_)) {
            std::lock_guard< std::mutex > u_lock(msg_acknowledged_mutex_);
            wait_for_msg_acknowledged_ = false;
            notification_cv_.notify_one();
            number_of_notification_messages_=0;
        }
    }

    void run() {
        timespec before,after,diff_ts;
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (wait_for_availability_) {
            cv_.wait(its_lock);
        }

        while (wait_until_subscription_accepted_) {
            if (std::cv_status::timeout == cv_.wait_for(its_lock, std::chrono::seconds(30))) {
                std::cout << "Subscription wasn't accepted in time!"<<std::endl;
                break;
            }
        }

        for(number_of_test_ = 1; number_of_test_ <= test_total_; ++number_of_test_){
            std::cout << "Service is send_service_start_measuring." << std::endl;
            send_service_start_measuring(true);
            get_now_time(before);
            {                    
                std::unique_lock<std::mutex> u_lock(msg_acknowledged_mutex_);
                notification_cv_.wait(u_lock, [this]{ return !wait_for_msg_acknowledged_; });
                wait_for_msg_acknowledged_ = true;
            }
            get_now_time(after);
            send_service_start_measuring(false);
            diff_ts = timespec_diff(before, after);
            uint64_t latency_us = diff_ts.tv_sec * 1000000 + diff_ts.tv_nsec / 1000;//测试过程总延迟
            latencys_.push_back(latency_us);
            //std::cout<<"latency_us:"<<latency_us-cycle_ * (number_of_notify_ - 1) * 1000<<std::endl;
            auto throughput = payload_size_ * number_of_notify_ * 1.000000 / (latency_us - cycle_*1000 * (number_of_notify_ - 1))*1000000;
            std::cout <<"The No."<<number_of_test_<<" Test "<< "receive " << std::setw(4) << std::setfill('0')
                    << number_of_notify_ << " notification messages."
                    <<" latency(us,except cycle)["
                    <<((latency_us - cycle_ * (number_of_notify_ - 1) * 1000)/number_of_notify_)
                    <<"], throughput(Bytes/s)["
                    <<throughput<<"]."
                    << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        stop();
        if (initialized_) {
            app_->stop();
        }
    }

    void send_service_start_measuring(bool _start_measuring) {
        std::shared_ptr<vsomeip::message> m = vsomeip::runtime::get()->create_request();
        m->set_service(TEST_SERVICE_ID);
        m->set_instance(TEST_INSTANCE_ID);
        _start_measuring ? m->set_method(START_METHOD_ID) : m->set_method(STOP_METHOD_ID);
        app_->send(m);
    }

private:
    protocol_e protocol_;
    std::shared_ptr<vsomeip::application> app_;
    std::mutex mutex_,msg_acknowledged_mutex_;
    std::condition_variable cv_,notification_cv_;
    bool running_;
    bool wait_for_availability_,wait_for_msg_acknowledged_;
    bool is_available_;
    bool wait_until_subscription_accepted_;

    uint32_t payload_size_;
    uint32_t test_total_;
    uint32_t number_of_test_;
    uint32_t number_of_notification_messages_;
    uint32_t number_of_notify_;
    std::vector<std::uint64_t> latencys_;
    uint32_t cycle_;
    std::thread sender_;

    std::atomic<bool> initialized_;
};

// TestEventClient *its_sample_ptr(nullptr);
// std::thread stop_thread;
// void handle_signal(int _signal) {
//     if (its_sample_ptr != nullptr &&
//             (_signal == SIGINT || _signal == SIGTERM)) {
//         stop_thread = std::thread([its_sample_ptr=its_sample_ptr]{
//             its_sample_ptr->stop();
//         }); 
//     }
// }


// this variables are changed via cmdline parameters
static protocol_e protocol(protocol_e::PR_UDP);
static std::uint32_t number_of_notify(6);
static std::uint32_t number_of_test(3);
static std::uint32_t cycle(50);
int main(int argc, char** argv) {

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
        } else if(std::string("--notify") == std::string(argv[i])
        || std::string("-n") == std::string(argv[i])) {
            try {
                number_of_notify = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid value for number of notify" << std::endl;
                return(EXIT_FAILURE);
            }
            i++;
        } else if(std::string("--test") == std::string(argv[i])
        || std::string("-t") == std::string(argv[i])) {
            try {
                number_of_test = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid value for test" << std::endl;
                return(EXIT_FAILURE);
            }
            i++;
        } else if(std::string("--cycle") == std::string(argv[i])
        || std::string("-c") == std::string(argv[i])) {
            try {
                cycle = static_cast<std::uint32_t>(std::stoul(std::string(argv[i+1]), nullptr, 10));
                std::cout<<"cycle:"<<cycle << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "Please specify a valid values for cycle" << std::endl;
                return(EXIT_FAILURE);
            }
            i++;
        } else if(std::string("--help") == std::string(argv[i])
        || std::string("-h") == std::string(argv[i])) {
            std::cout << "Available options:" << std::endl;
            std::cout << "--protocol|-p: valid values TCP or UDP" << std::endl;
            std::cout << "--notify|-n: number of message notify from server" << std::endl;
            std::cout << "--test|-t: number of test" << std::endl;
            std::cout << "--cycle|-c: cycle notify" << std::endl;
        }
        i++;
    }

    TestEventClient client(protocol, number_of_notify, number_of_test, cycle);
    // its_sample_ptr = &client;
    // signal(SIGINT, handle_signal);
    // signal(SIGTERM, handle_signal);

    if (client.Init()) {
        client.Start();

        // if (stop_thread.joinable()) {
        //     stop_thread.join();
        // } else{
        //     stop_thread.detach();
        // }
        return 0;
    } else {
        return 1;
    }
}