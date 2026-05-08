#include "BitScheduler.hpp"
#include <algorithm>

void BitScheduler::Schedule(std::shared_ptr<BitTask> task) {
    task->Start();
    m_tasks.push_back(std::move(task));
}

void BitScheduler::Update(float dt) {
    for (auto& task : m_tasks) {
        if (!task->IsFinished()) {
            task->Update(dt);
        }
    }
    Prune();
}

bool BitScheduler::HasActiveTasks(uint32_t tagMask) const {
    for (const auto& task : m_tasks) {
        if (!task->IsFinished() && task->HasTag(tagMask)) return true;
    }
    return false;
}

void BitScheduler::FastForwardByTag(uint32_t tagMask) {
    for (auto& task : m_tasks) {
        if (!task->IsFinished() && task->HasTag(tagMask)) task->FastForward();
    }
    Prune();
}

void BitScheduler::FastForwardByOwner(const std::string& owner) {
    for (auto& task : m_tasks) {
        if (!task->IsFinished() && task->GetOwner() == owner) task->FastForward();
    }
    Prune();
}

void BitScheduler::CancelByTag(uint32_t tagMask) {
    for (auto& task : m_tasks) {
        if (!task->IsFinished() && task->HasTag(tagMask)) task->Cancel();
    }
    Prune();
}

void BitScheduler::CancelByOwner(const std::string& owner) {
    for (auto& task : m_tasks) {
        if (!task->IsFinished() && task->GetOwner() == owner) task->Cancel();
    }
    Prune();
}

void BitScheduler::CancelAll() {
    for (auto& task : m_tasks) task->Cancel();
    m_tasks.clear();
}

void BitScheduler::Prune() {
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
            [](const std::shared_ptr<BitTask>& t) { return t->IsFinished(); }),
        m_tasks.end()
    );
}
