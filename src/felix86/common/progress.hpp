#pragma once

#include <string>
#include <indicators/block_progress_bar.hpp>

using namespace indicators;

struct ProgressBar {
    ProgressBar(const std::string& task, size_t total)
        : task(task), bar{option::BarWidth{80}, option::ForegroundColor{Color::white}, option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
                          option::MaxProgress{total}} {}

    void tick() {
        bar.tick();
    }

    void completed() {
        bar.mark_as_completed();
    }

private:
    std::string task;
    BlockProgressBar bar;
};