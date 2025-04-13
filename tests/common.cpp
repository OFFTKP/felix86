#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include "felix86/common/global.hpp"
#include "felix86/hle/signals.hpp"

class Initializer : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const&) override {
        g_output_fd = STDOUT_FILENO;
        g_testing = true;
        Config::initialize();
        initialize_globals();
        g_process_globals.initialize();
        initialize_extensions();
        Signals::initialize();
        g_config.protect_pages = false;
    }
};

CATCH_REGISTER_LISTENER(Initializer)
