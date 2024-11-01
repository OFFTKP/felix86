#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include "felix86/common/global.hpp"

class Initializer : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const&) override {
        initialize_globals();
        initialize_extensions();
    }
};

CATCH_REGISTER_LISTENER(Initializer)
