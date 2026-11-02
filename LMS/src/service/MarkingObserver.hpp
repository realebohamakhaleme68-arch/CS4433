// Observer pattern: MarkingService (subject) notifies observers when a mark is
// saved; it knows only the interface, never the UI (FR-11).
#pragma once
#include <map>
#include <string>

namespace lms {

class IMarkingObserver {
public:
    virtual ~IMarkingObserver() = default;
    virtual void onMarkSaved(int departmentId, int submissionId) = 0;
};

class HoDNotifier : public IMarkingObserver {
public:
    void onMarkSaved(int departmentId, int submissionId) override {
        ++pending_[departmentId];
        lastMessage_ = "The Head of Department has been notified (submission #" + std::to_string(submissionId) +
                       " awaits moderation).";
    }
    int pendingFor(int departmentId) const {
        auto it = pending_.find(departmentId);
        return it == pending_.end() ? 0 : it->second;
    }
    void clear(int departmentId) { pending_[departmentId] = 0; }
    const std::string& lastMessage() const { return lastMessage_; }
private:
    std::map<int, int> pending_;
    std::string lastMessage_;
};

}  // namespace lms
