#pragma once

#include <array>
#include <csignal>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

struct PendingSignal {
    int sig;
    siginfo_t info;
};

struct SignalQueue {
    void push(const PendingSignal& signal) {
        ASSERT_MSG(signal.sig >= 32 && signal.sig <= 64, "Signal outside of range: %d", signal.sig);

        int sig = signal.sig - 32 - 1;
        bool ok = signal_queues[sig].push(signal);
        if (!ok) {
            ERROR("Signal queue for realtime signal %d overflowed", signal.sig);
        }
        total_signals++;
    }

    PendingSignal pop() {
        for (size_t i = 0; i < signal_queues.size(); i++) {
            if (!signal_queues[i].empty()) {
                total_signals--;
                return signal_queues[i].pop();
            }
        }

        ERROR("Tried popping but there's no pending signals?");
        return {};
    }

    bool empty() {
        return total_signals == 0;
    }

private:
    struct SignalQueueSingle {
        bool push(const PendingSignal& signal) {
            if (index == data.size()) {
                return false;
            }

            data[index] = signal;
            index++;
            return true;
        }

        PendingSignal pop() {
            ASSERT_MSG((int)index - 1 >= 0, "Signal queue underflow somehow?");
            PendingSignal signal = data[index - 1];
            index--;
            return signal;
        }

        bool empty() {
            return index == 0;
        }

    private:
        size_t index = 0;
        std::array<PendingSignal, 10> data{};
    };

    size_t total_signals = 0;
    std::array<SignalQueueSingle, 32> signal_queues{};
};