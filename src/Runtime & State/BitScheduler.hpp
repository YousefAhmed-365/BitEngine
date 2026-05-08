#ifndef BIT_SCHEDULER_HPP
#define BIT_SCHEDULER_HPP

#include "BitTask.hpp"
#include <vector>
#include <memory>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// BitScheduler: Central coordinator for all async/time-based engine tasks.
//
// Responsibilities:
//   - Maintains the list of active BitTasks.
//   - Drives all task updates once per frame via Update(dt).
//   - Provides query methods for the Wait system (HasActiveTasks by tag).
//   - Provides batch cancellation/fast-forward by tag or owner ID.
//
// Usage:
//   m_scheduler.Schedule(std::make_shared<LerpTask>(...));
//   if (m_scheduler.HasActiveTasks(TAG_MOVE)) { /* VM is waiting */ }
// ─────────────────────────────────────────────────────────────────────────────
class BitScheduler {
public:
    // Schedule a new task. It will be started immediately and updated each frame.
    void Schedule(std::shared_ptr<BitTask> task);

    // Advance all tasks by one frame.
    void Update(float dt);

    // ── Query ──────────────────────────────────────────────────────────────
    // Returns true if there are any active (non-finished) tasks matching the tag mask.
    bool HasActiveTasks(uint32_t tagMask) const;

    // Returns the number of currently active tasks.
    size_t ActiveCount() const { return m_tasks.size(); }

    // ── Batch Operations ───────────────────────────────────────────────────
    // Instantly complete all tasks matching the tag mask (drives them to final value).
    void FastForwardByTag(uint32_t tagMask);

    // Instantly complete all tasks owned by a given entity ID.
    void FastForwardByOwner(const std::string& owner);

    // Cancel (abort) all tasks matching the tag mask without applying final values.
    void CancelByTag(uint32_t tagMask);

    // Cancel all tasks belonging to a given owner.
    void CancelByOwner(const std::string& owner);

    // Cancel everything.
    void CancelAll();

    // Expose task list for debugger visualization (read-only).
    const std::vector<std::shared_ptr<BitTask>>& GetTasks() const { return m_tasks; }

private:
    std::vector<std::shared_ptr<BitTask>> m_tasks;

    void Prune(); // Remove finished/cancelled tasks from the list.
};

#endif // BIT_SCHEDULER_HPP
