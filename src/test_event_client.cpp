#include "common.hpp"

#include <iomanip>
#include <iostream>
#include <vsomeip/vsomeip.hpp>

class TestEventClient {
public:
    TestEventClient() :
        app_(vsomeip::runtime::get()->create_application()) {
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

private:
    void on_availability(vsomeip::service_t service, vsomeip::instance_t instance, bool is_available) {
        std::cout << "Service ["
                << std::setw(4) << std::setfill('0') << std::hex << service << "." << instance
                << "] is "
                << (is_available ? "available." : "NOT available.")
                << std::endl;
    }

    void on_message(const std::shared_ptr<vsomeip::message> &response) {
        timespec response_ts;
        timespec diff_ts;
        timespec now_ts;
        get_now_time(now_ts);
        bytes_to_timespec(response->get_payload()->get_data(), response->get_payload()->get_length(), response_ts);
        diff_ts = timespec_diff(response_ts, now_ts);
        std::cout << "Received a notification for Event ["
                << std::setfill('0') << std::hex
                << std::setw(4) << response->get_service() << "."
                << std::setw(4) << response->get_instance() << "."
                << std::setw(4) << response->get_method() << "] to Client/Session ["
                << std::setw(4) << response->get_client() << "/"
                << std::setw(4) << response->get_session()
                << "] latency ["
                << std::setfill('0') << std::dec
                << diff_ts.tv_sec
                << "."
                << std::setw(6) << diff_ts.tv_nsec / 1000
                << "]"
                << std::endl;
    }

    std::shared_ptr<vsomeip::application> app_;
};

int main(int argc, char** argv) {

    TestEventClient client;

    if (client.Init()) {
        client.Start();
        return 0;
    } else {
        return 1;
    }

    return 0;
}