// src/Pipelines/pipeline_scheduler.h — Background cron trigger scheduler

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace mnemo::interpreters {
class Context;
}

namespace mnemo::pipelines {

class PipelineScheduler {
public:
    using RunCallback = std::function<void(std::string_view pipeline_name,
                                           std::string_view trigger_name)>;

    static auto instance() -> PipelineScheduler&;

    void set_run_callback(RunCallback callback);
    void start(std::chrono::seconds tick_interval = std::chrono::seconds{5});
    void stop();
    [[nodiscard]] auto is_running() const -> bool;
    void tick();

private:
    PipelineScheduler() = default;

    std::atomic<bool> running_{false};
    std::chrono::seconds tick_interval_{5};
    std::thread tick_thread_;
    RunCallback run_callback_;
};

} // namespace mnemo::pipelines
