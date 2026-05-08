#ifndef BIT_TASK_HPP
#define BIT_TASK_HPP

#include <functional>
#include <memory>
#include <queue>
#include <vector>
#include <string>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// BitTaskTag: Bitmask for task categorization and Wait system queries
// ─────────────────────────────────────────────────────────────────────────────
enum BitTaskTag : uint32_t {
    TAG_NONE     = 0,
    TAG_MOVE     = 1 << 0,
    TAG_FADE     = 1 << 1,
    TAG_SHAKE    = 1 << 2,
    TAG_UI       = 1 << 3,
    TAG_AUDIO    = 1 << 4,
    TAG_DELAY    = 1 << 5,
    TAG_TIMELINE = 1 << 6,
    TAG_ALL      = 0xFFFFFFFF,
};

// ─────────────────────────────────────────────────────────────────────────────
// Easing Functions
// ─────────────────────────────────────────────────────────────────────────────
namespace BitEase {
    inline float Linear(float t)    { return t; }
    inline float CubicOut(float t)  { return 1.0f - powf(1.0f - t, 3.0f); }
    inline float CubicIn(float t)   { return t * t * t; }
    inline float CubicInOut(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BitTask: Base class for all scheduled async operations
// ─────────────────────────────────────────────────────────────────────────────
class BitTask {
public:
    virtual ~BitTask() = default;

    virtual void Start() {}
    virtual void Update(float dt) = 0;
    virtual void FastForward() { m_finished = true; }
    virtual void Cancel()      { m_cancelled = true; m_finished = true; }
    virtual float GetProgress() const { return m_finished ? 1.0f : 0.0f; }

    bool IsFinished()  const { return m_finished; }
    bool IsCancelled() const { return m_cancelled; }

    uint32_t    GetTags()  const { return m_tags; }
    std::string GetOwner() const { return m_owner; }

    void AddTag(BitTaskTag tag) { m_tags |= tag; }
    void SetOwner(const std::string& owner) { m_owner = owner; }
    bool HasTag(uint32_t mask) const { return (m_tags & mask) != 0; }

protected:
    bool     m_finished   = false;
    bool     m_cancelled  = false;
    uint32_t m_tags       = TAG_NONE;
    std::string m_owner   = "";
};

// ─────────────────────────────────────────────────────────────────────────────
// LerpTask: Interpolates a float value over time with an easing function
// The updater callback allows this to drive any engine value (position, alpha, etc)
// ─────────────────────────────────────────────────────────────────────────────
class LerpTask : public BitTask {
public:
    using Updater = std::function<void(float)>;
    using Easer   = std::function<float(float)>;

    LerpTask(float duration, float from, float to, Updater updater, Easer easer = BitEase::Linear)
        : m_duration(duration), m_from(from), m_to(to), m_updater(updater), m_easer(easer) {}

    void Start() override {
        if (m_updater) m_updater(m_from);
    }

    void Update(float dt) override {
        if (m_finished) return;
        m_elapsed += dt;
        float t = (m_duration > 0.0f) ? std::min(1.0f, m_elapsed / m_duration) : 1.0f;
        float eased = m_easer(t);
        if (m_updater) m_updater(m_from + (m_to - m_from) * eased);
        if (t >= 1.0f) {
            if (m_updater) m_updater(m_to); // guarantee exact final value
            m_finished = true;
        }
    }

    void FastForward() override {
        if (m_updater) m_updater(m_to);
        m_finished = true;
    }

    float GetProgress() const override {
        if (m_finished || m_duration <= 0.0f) return 1.0f;
        return std::min(1.0f, m_elapsed / m_duration);
    }

private:
    float   m_duration, m_from, m_to, m_elapsed = 0.0f;
    Updater m_updater;
    Easer   m_easer;
};

// ─────────────────────────────────────────────────────────────────────────────
// DelayTask: Waits for a set duration and finishes.
// ─────────────────────────────────────────────────────────────────────────────
class DelayTask : public BitTask {
public:
    explicit DelayTask(float duration) : m_duration(duration) {
        AddTag(TAG_DELAY);
    }

    void Update(float dt) override {
        m_elapsed += dt;
        if (m_elapsed >= m_duration) m_finished = true;
    }

    void FastForward() override { m_finished = true; }

    float GetProgress() const override {
        if (m_finished || m_duration <= 0.0f) return 1.0f;
        return std::min(1.0f, m_elapsed / m_duration);
    }

private:
    float m_duration, m_elapsed = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// EventTask: Fires a callback immediately on the next Update tick.
// Zero-duration, used inside SequenceTasks to trigger side effects.
// ─────────────────────────────────────────────────────────────────────────────
class EventTask : public BitTask {
public:
    using Callback = std::function<void()>;
    explicit EventTask(Callback cb) : m_callback(cb) {}

    void Update(float dt) override {
        (void)dt;
        if (m_callback) m_callback();
        m_finished = true;
    }

private:
    Callback m_callback;
};

// ─────────────────────────────────────────────────────────────────────────────
// SequenceTask: Runs a queue of tasks one-after-another.
// Tags are the union of all child task tags.
// ─────────────────────────────────────────────────────────────────────────────
class SequenceTask : public BitTask {
public:
    void Add(std::shared_ptr<BitTask> task) {
        m_tags |= task->GetTags();
        m_tasks.push(task);
    }

    void Start() override {
        if (!m_tasks.empty()) m_tasks.front()->Start();
    }

    void Update(float dt) override {
        while (!m_tasks.empty()) {
            auto& current = m_tasks.front();
            if (!current->IsFinished()) {
                current->Update(dt);
                return;
            }
            m_tasks.pop();
            if (!m_tasks.empty()) m_tasks.front()->Start();
        }
        m_finished = true;
    }

    void FastForward() override {
        while (!m_tasks.empty()) {
            m_tasks.front()->FastForward();
            m_tasks.pop();
        }
        m_finished = true;
    }

    float GetProgress() const override {
        if (m_finished || m_tasks.empty()) return 1.0f;
        return m_tasks.front()->GetProgress();
    }

private:
    std::queue<std::shared_ptr<BitTask>> m_tasks;
};

// ─────────────────────────────────────────────────────────────────────────────
// ParallelTask: Runs multiple tasks simultaneously.
// Finishes when ALL child tasks are done.
// ─────────────────────────────────────────────────────────────────────────────
class ParallelTask : public BitTask {
public:
    void Add(std::shared_ptr<BitTask> task) {
        m_tags |= task->GetTags();
        m_tasks.push_back(task);
    }

    void Start() override {
        for (auto& t : m_tasks) t->Start();
    }

    void Update(float dt) override {
        bool allDone = true;
        for (auto& t : m_tasks) {
            if (!t->IsFinished()) {
                t->Update(dt);
                if (!t->IsFinished()) allDone = false;
            }
        }
        if (allDone) m_finished = true;
    }

    void FastForward() override {
        for (auto& t : m_tasks) t->FastForward();
        m_finished = true;
    }

    float GetProgress() const override {
        if (m_finished || m_tasks.empty()) return 1.0f;
        float totalProgress = 0.0f;
        for (const auto& t : m_tasks) totalProgress += t->GetProgress();
        return totalProgress / m_tasks.size();
    }

private:
    std::vector<std::shared_ptr<BitTask>> m_tasks;
};

#endif // BIT_TASK_HPP
