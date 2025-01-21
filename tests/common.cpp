#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include "felix86/common/global.hpp"
#include "felix86/hle/signals.hpp"

class Initializer : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const&) override {
        g_testing = true;
        initialize_globals();
        initialize_extensions();
        Signals::initialize();
    }
};

CATCH_REGISTER_LISTENER(Initializer)
