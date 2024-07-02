#include "common.hpp"
#include "cpu_load_measurer.hpp"

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
        cpu_load_measurer_(static_cast<std::uint32_t>(::getpid())) {}

    bool Init() {
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

        app_->register_message_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_METHOD_ID, 
        [this](const std::shared_ptr<vsomeip::message> &request){
            on_message(request);
        });
       app_->register_message_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID, STOP_METHOD_ID, 
        [this](const std::shared_ptr<vsomeip::message> &request){
            on_message_stop_measuring(request);
        });
       app_->register_message_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID, START_METHOD_ID, 
        [this](const std::shared_ptr<vsomeip::message> &request){
            on_message_start_measuring(request);
        });

        return true;
    }

    void Start() {
        app_->start();
    }

    void stop()
    {
        std::cout << "Stopping...";
        app_->stop_offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
        app_->clear_all_handler();
        app_->stop();
    }

private:

    void run() {
        std::unique_lock<std::mutex> u_lock(mutex_);
        cv_.wait(u_lock, [this] { return blocked_; });
        app_->offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
    }

    void on_message(const std::shared_ptr<vsomeip::message> &request) {
        timespec request_ts;
        timespec diff_ts;
        timespec now_ts;
        
        (now_ts);
        bytes_to_timespec(request->get_payload()->get_data(), request->get_payload()->get_length(), request_ts);
        diff_ts = timespec_diff(request_ts, now_ts);

        std::cout << "Received a message with Client/Session ["
		        << std::setfill('0') << std::hex
		        << std::setw(4) << request->get_client() 
                << "/"
		        << std::setw(4) << request->get_session() 
                << "] latency ["
                << std::setfill('0') << std::dec
                << diff_ts.tv_sec
                << "."
                << std::setw(6) << diff_ts.tv_nsec / 1000
                << "]"
		        << std::endl;

        auto response = vsomeip::runtime::get()->create_response(request);
        std::size_t payload_size;
        std::memcpy(&payload_size, request->get_payload()->get_data() + sizeof(timespec), sizeof(std::size_t));
        ByteVec payload_data(payload_size);
        auto response_payload = vsomeip::runtime::get()->create_payload(payload_data);
        response->set_payload(response_payload);
        app_->send(response);
    }


    void on_message_start_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        cpu_load_measurer_.start();
    }

    void on_message_stop_measuring(const std::shared_ptr<vsomeip::message>& _request)
    {
        (void)_request;
        cpu_load_measurer_.stop();
        std::cout << "Received " << std::setw(4) << std::setfill('0')
        << number_of_received_messages_ << " messages. CPU load [%]: "
        << std::fixed << std::setprecision(2)
        << (std::isfinite(cpu_load_measurer_.get_cpu_load()) ? cpu_load_measurer_.get_cpu_load() : 0.0);
        cpu_loads_.push_back(std::isfinite(cpu_load_measurer_.get_cpu_load()) ? cpu_load_measurer_.get_cpu_load() : 0.0);
        number_of_received_messages_ = 0;
    }

    void on_message_shutdown(const std::shared_ptr<vsomeip::message>& _request){
        (void)_request;
        std::cout << "Shutdown method was called, going down now.";
        const double average_load(std::accumulate(cpu_loads_.begin(), cpu_loads_.end(), 0.0) / static_cast<double>(cpu_loads_.size()));
        std::cout << "Received: " << number_of_received_messages_total_
            << " in total (excluding control messages). This caused: "
            << std::fixed << std::setprecision(2)
            << average_load << "% load in average (average of "
            << cpu_loads_.size() << " measurements).";

        std::vector<double> results_no_zero;
        for(const auto &v : cpu_loads_) {
            if(v > 0.0) {
                results_no_zero.push_back(v);
            }
        }
        const double average_load_no_zero(std::accumulate(results_no_zero.begin(), results_no_zero.end(), 0.0) / static_cast<double>(results_no_zero.size()));
        std::cout << "Sent: " << number_of_received_messages_total_
            << " messages in total (excluding control messages). This caused: "
            << std::fixed << std::setprecision(2)
            << average_load_no_zero << "% load in average, if measured cpu load "
            << "was greater zero (average of "
            << results_no_zero.size() << " measurements).";
        stop();
    }

    std::shared_ptr<vsomeip::application> app_;

    std::mutex mutex_;
    std::condition_variable cv_;
    bool blocked_;

    std::thread offer_;
    cpu_load_measurer cpu_load_measurer_;
    std::vector<double> cpu_loads_;
    int32_t number_of_received_messages_,number_of_received_messages_total_;
};

int main(int argc, char** argv) {
    TestMethodServer server;

    if (server.Init()) {
        server.Start();
        return 0;
    } else {
        return 1;
    }

    return 0;
}