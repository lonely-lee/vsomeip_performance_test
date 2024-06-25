#include "common.hpp"

#include <cstddef>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <vsomeip/vsomeip.hpp>

class TestMethodServer {
public:
    TestMethodServer() :
        app_(vsomeip::runtime::get()->create_application()),
        blocked_(false),
        offer_([this]{ run(); }) {
    }

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

        return true;
    }

    void Start() {
        app_->start();
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
        get_now_time(now_ts);
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

        timespec_to_bytes(now_ts, response->get_payload()->get_data(), response->get_payload()->get_length());
        app_->send(response);
    }

    std::shared_ptr<vsomeip::application> app_;

    std::mutex mutex_;
    std::condition_variable cv_;
    bool blocked_;

    std::thread offer_;
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