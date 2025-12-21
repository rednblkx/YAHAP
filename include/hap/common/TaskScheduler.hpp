#pragma once

#include "hap/platform/System.hpp"
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace hap::common {

/**
 * @brief Central task scheduler for periodic and one-shot tasks.
 * 
 * Provides a non-blocking, cooperative task scheduling mechanism suitable
 * for embedded systems. Applications call tick() from their main loop.
 * 
 * Usage:
 * @code
 * TaskScheduler scheduler(&system);
 * 
 * // Schedule a periodic task every 1000ms
 * auto id = scheduler.schedule_periodic(1000, []() {
 *     check_session_timeouts();
 * });
 * 
 * // In main loop
 * while (running) {
 *     scheduler.tick(system.millis());
 *     // ... other work
 * }
 * @endcode
 */
class TaskScheduler {
public:
    using TaskId = uint32_t;
    using TaskCallback = std::function<void()>;
    
    static constexpr TaskId INVALID_TASK_ID = 0;

    explicit TaskScheduler(platform::System* system);
    ~TaskScheduler() = default;
    
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    TaskScheduler(TaskScheduler&&) = delete;
    TaskScheduler& operator=(TaskScheduler&&) = delete;
    
    /**
     * @brief Schedule a task to run periodically.
     * @param interval_ms Interval between executions in milliseconds.
     * @param callback Function to call on each execution.
     * @return Task ID for cancellation, or INVALID_TASK_ID on failure.
     */
    TaskId schedule_periodic(uint32_t interval_ms, TaskCallback callback);
    
    /**
     * @brief Schedule a task to run once after a delay.
     * @param delay_ms Delay in milliseconds before execution.
     * @param callback Function to call.
     * @return Task ID for cancellation, or INVALID_TASK_ID on failure.
     */
    TaskId schedule_once(uint32_t delay_ms, TaskCallback callback);
    
    /**
     * @brief Cancel a scheduled task.
     * @param id Task ID returned from schedule_periodic or schedule_once.
     * @return true if task was found and cancelled.
     */
    bool cancel(TaskId id);
    
    /**
     * @brief Cancel all scheduled tasks.
     */
    void cancel_all();
    
    /**
     * @brief Process pending tasks. Call this from your main loop.
     * @param current_time_ms Current time in milliseconds (from System::millis()).
     */
    void tick(uint64_t current_time_ms);
    
    /**
     * @brief Convenience overload that gets current time from system.
     */
    void tick();
    
    /**
     * @brief Get the number of currently scheduled tasks.
     */
    [[nodiscard]] size_t task_count() const { return tasks_.size(); }

private:
    struct ScheduledTask {
        TaskId id;
        TaskCallback callback;
        uint64_t next_run_ms;
        uint32_t interval_ms;
        bool cancelled = false;
    };
    
    platform::System* system_;
    std::vector<ScheduledTask> tasks_;
    TaskId next_id_ = 1;
    mutable std::mutex mutex_;
    
    void cleanup_cancelled_tasks();
};

} // namespace hap::common
