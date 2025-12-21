#include "hap/common/TaskScheduler.hpp"
#include <algorithm>

namespace hap::common {

TaskScheduler::TaskScheduler(platform::System* system)
    : system_(system) {}

TaskScheduler::TaskId TaskScheduler::schedule_periodic(uint32_t interval_ms, TaskCallback callback) {
    if (!callback || interval_ms == 0) {
        return INVALID_TASK_ID;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    TaskId id = next_id_++;
    if (next_id_ == INVALID_TASK_ID) {
        next_id_ = 1;
    }
    
    uint64_t now = system_ ? system_->millis() : 0;
    
    tasks_.push_back(ScheduledTask{
        .id = id,
        .callback = std::move(callback),
        .next_run_ms = now + interval_ms,
        .interval_ms = interval_ms,
        .cancelled = false
    });
    
    return id;
}

TaskScheduler::TaskId TaskScheduler::schedule_once(uint32_t delay_ms, TaskCallback callback) {
    if (!callback) {
        return INVALID_TASK_ID;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    TaskId id = next_id_++;
    if (next_id_ == INVALID_TASK_ID) {
        next_id_ = 1;
    }
    
    uint64_t now = system_ ? system_->millis() : 0;
    
    tasks_.push_back(ScheduledTask{
        .id = id,
        .callback = std::move(callback),
        .next_run_ms = now + delay_ms,
        .interval_ms = 0,
        .cancelled = false
    });
    
    return id;
}

bool TaskScheduler::cancel(TaskId id) {
    if (id == INVALID_TASK_ID) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        if (task.id == id && !task.cancelled) {
            task.cancelled = true;
            return true;
        }
    }
    return false;
}

void TaskScheduler::cancel_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        task.cancelled = true;
    }
}

void TaskScheduler::tick(uint64_t current_time_ms) {
    std::vector<std::pair<TaskCallback, size_t>> to_execute;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < tasks_.size(); ++i) {
            auto& task = tasks_[i];
            if (task.cancelled) continue;
            
            if (current_time_ms >= task.next_run_ms) {
                if (task.callback) {
                    to_execute.emplace_back(task.callback, i);
                }
                
                if (task.interval_ms > 0) {
                    task.next_run_ms = current_time_ms + task.interval_ms;
                } else {
                    task.cancelled = true;
                }
            }
        }
    }
    
    for (auto& [callback, index] : to_execute) {
        callback();
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cleanup_cancelled_tasks();
    }
}

void TaskScheduler::tick() {
    if (system_) {
        tick(system_->millis());
    }
}

void TaskScheduler::cleanup_cancelled_tasks() {
    tasks_.erase(
        std::remove_if(tasks_.begin(), tasks_.end(),
            [](const ScheduledTask& task) { return task.cancelled; }),
        tasks_.end()
    );
}

} // namespace hap::common

