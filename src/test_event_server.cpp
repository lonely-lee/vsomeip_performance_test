#include "common.hpp"

#include <chrono>
#include <sstream>
#include <iostream>
#include <condition_variable>
#include <memory>
#include <thread>
#include <mutex>
#include <vsomeip/vsomeip.hpp>

class TestEventServer {
public:
    TestEventServer(protocol_e protocol, std::size_t payload_size, std::uint32_t cycle) :
        app_(vsomeip::runtime::get()->create_application()),
        protocol_(protocol),
        payload_size_(payload_size + time_payload_size),
        payload_(vsomeip::runtime::get()->create_payload()),
        running_(true),
        blocked_(false),
        is_offered_(false),
        cycle_(cycle),
        offer_thread_([this]{ offer(); }),
        notify_thread_([this]{ notify(); }) {
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
                offer_cv_.notify_one();
            }
        });

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

private:
    void offer() {
        {
            std::unique_lock<std::mutex> u_lock(offer_mutex_);
            offer_cv_.wait(u_lock, [this] { return blocked_; });
        }
        {
            std::lock_guard<std::mutex> g_lock(notify_mutex_);
            app_->offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
            is_offered_ = true;
            notify_cv_.notify_one();
        }
    }

    void notify() {
        {
            std::unique_lock<std::mutex> u_lock(notify_mutex_);
            notify_cv_.wait(u_lock, [this]{ return is_offered_; });
        }

        timespec ts;
        while (running_) {
            get_now_time(ts);
            timespec_to_bytes(ts, payload_->get_data(), payload_->get_length());
            app_->notify(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENT_ID, payload_);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
        }
    }

    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::payload> payload_;
    protocol_e protocol_;
    std::size_t payload_size_;

    std::mutex offer_mutex_;
    std::condition_variable offer_cv_;
    std::mutex notify_mutex_;
     std::condition_variable notify_cv_;
    bool running_;
    bool blocked_;
    bool is_offered_;
    std::uint32_t cycle_;

    std::thread notify_thread_;
    std::thread offer_thread_;
};

int main(int argc, char** argv) {
    bool use_tcp = false;
    bool be_quiet = false;
    std::uint32_t cycle = 1000; // Default: 1s
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

    if (server.Init()) {
        server.Start();
        return 0;
    } else {
        return 1;
    }

    return 0;
}