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

class TestEventClient {
public:
    TestEventClient() :
        app_(vsomeip::runtime::get()->create_application()),
        number_of_demand_(100),
        number_of_test_(1),
        number_of_notification_messages_(0),
        cycle_(500),
        sender_(std::bind(&TestEventClient::run, this)) {
    }

    bool Init() {
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state){
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->request_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
            }
        });

        app_->register_message_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENT_ID, 
        [this](const std::shared_ptr<vsomeip::message> &response){
            on_message(response);
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
        app_->subscribe(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENTGROUP_ID);

        return true;
    }

    void Start() {
        app_->start();
    }

    void stop() {
        std::cout<<"Stopping test"<<std::endl;
        if (is_available_){//通知服务端测试结束
            std::cout<<"Sending shutdown request"<<std::endl;
            std::shared_ptr<vsomeip::message> request_ = vsomeip::runtime::get()->create_request();
            request_->set_service(TEST_SERVICE_ID);
            request_->set_instance(TEST_INSTANCE_ID);
            request_->set_method(SHUTDOWN_METHOD_ID);
            app_->send(request_);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        running_ = false;
        is_available_ = false;
        wait_for_availability_ = false;
        wait_for_msg_acknowledged_ = false;

        app_->clear_all_handler();
        app_->unsubscribe(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENTGROUP_ID);
        app_->release_event(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENT_ID);
        app_->release_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);

        cv_.notify_one();
        notification_cv_.notify_one();
        if (std::this_thread::get_id() != sender_.get_id()) {
            std::cout<<"Waiting for sender to join"<<std::endl;
            if (sender_.joinable()) {
                std::cout<<"Joining sender"<<std::endl;
                sender_.join();
            }
        } else {
            std::cout<<"Sender is running on the same thread as this"<<std::endl;
            sender_.detach();
        }
        std::cout<<"Test over"<<std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        app_->stop();

        auto average_latency = std::accumulate(latencys_.begin(), latencys_.end(), 0.0) / static_cast<double>(latencys_.size());
        std::cout<<"This test is over:"
                <<"average_latency["
                <<average_latency
                <<"us]"<<std::endl;
    }

private:
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
        number_of_notification_messages_++;
        std::cout<<"The No."<<number_of_test_<<" test:"<<" Received  number of messages:"<<number_of_notification_messages_<<std::endl;
        if (is_available_ && (number_of_notification_messages_== number_of_demand_)) {
            std::cout<<"The No."<<number_of_test_<<" test over, The next round of testing is about to begin!!!!!!"<<std::endl;
            std::lock_guard< std::mutex > u_lock(msg_acknowledged_mutex_);
            wait_for_msg_acknowledged_ = false;
            notification_cv_.notify_one();
            number_of_notification_messages_=0;
        }
    }

    void run() {
        timespec before,after,diff_ts;
        while (running_) {
            {
                std::unique_lock<std::mutex> u_lock(mutex_);
                cv_.wait(u_lock, [this]{return !wait_for_availability_; });
                std::cout << "Service is running_." << std::endl;
                if (is_available_) {
                    std::cout << "Service is send_service_start_measuring." << std::endl;
                    send_service_start_measuring(true);
                    get_now_time(before);
                    std::unique_lock<std::mutex> u_lock(msg_acknowledged_mutex_);
                    notification_cv_.wait(u_lock, [this]{ return !wait_for_msg_acknowledged_; });
                    wait_for_msg_acknowledged_ = true;

                    get_now_time(after);
                    send_service_start_measuring(false);
                    diff_ts = timespec_diff(before, after);
                    auto latency_us = ((diff_ts.tv_sec * 1000000) + (diff_ts.tv_nsec / 1000)) / (2 * number_of_demand_);//请求到响应来回除以二,同时除以发送请求次数
                    latencys_.push_back(latency_us);
                    
                    std::cout <<"The No."<<number_of_test_<<" Test "<< "receive " << std::setw(4) << std::setfill('0')
                            << number_of_demand_ << " notification messages."
                            <<" latency(us)["
                            <<latency_us
                            <<"]"
                            << std::endl;
                    number_of_test_++;
                }

            }
            std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
        }
    }

    void send_service_start_measuring(bool _start_measuring) {
        std::shared_ptr<vsomeip::message> m = vsomeip::runtime::get()->create_request();
        m->set_service(TEST_SERVICE_ID);
        m->set_instance(TEST_INSTANCE_ID);
        _start_measuring ? m->set_method(START_METHOD_ID) : m->set_method(STOP_METHOD_ID);
        app_->send(m);
    }

    std::shared_ptr<vsomeip::application> app_;
    std::mutex mutex_,msg_acknowledged_mutex_;
    std::condition_variable cv_,notification_cv_;
    bool running_;
    bool wait_for_availability_,wait_for_msg_acknowledged_;
    bool is_available_;

    uint32_t number_of_test_;
    uint32_t number_of_notification_messages_;
    uint32_t number_of_demand_;
    std::vector<std::uint64_t> latencys_;
    uint32_t cycle_;
    std::thread sender_;
};

TestEventClient *its_sample_ptr(nullptr);
std::thread stop_thread;
void handle_signal(int _signal) {
    if (its_sample_ptr != nullptr &&
            (_signal == SIGINT || _signal == SIGTERM)) {
        stop_thread = std::thread([its_sample_ptr=its_sample_ptr]{
            its_sample_ptr->stop();
        }); 
    }
}

int main(int argc, char** argv) {

    TestEventClient client;
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