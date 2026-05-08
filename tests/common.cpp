#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include "felix86/common/global.hpp"
#include "felix86/hle/signals.hpp"

class Initializer : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const&) override {
        g_output_fd = STDOUT_FILENO;
        Config::initialize();
        initialize_globals();
        g_process_globals.initialize();
        Signals::initialize();

        // Set most configs to default except for rootfs path
        Config new_config{};
        new_config.rootfs_path = g_config.rootfs_path;
        new_config.protect_pages = false;
        g_config = new_config;
    }
};

CATCH_REGISTER_LISTENER(Initializer)
