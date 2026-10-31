// Strategy pattern for the late-penalty rule (FR-8). MarkingService depends only
// on ILatePenaltyStrategy, so a new rule is a new class (OCP, DIP).
#pragma once

namespace lms {

class ILatePenaltyStrategy {
public:
    virtual ~ILatePenaltyStrategy() = default;
    // Marks to deduct. The caller clamps the final mark at 0.
    virtual double calculatePenalty(int daysLate, double maxMark) const = 0;
};

// 10% of the maximum mark per started day for days 1-3; day 4 or more gives zero.
class StandardDeductionStrategy : public ILatePenaltyStrategy {
public:
    double calculatePenalty(int daysLate, double maxMark) const override {
        if (daysLate <= 0) return 0.0;
        if (daysLate >= 4) return maxMark;       // whole mark removed -> final mark 0
        return maxMark * 0.10 * daysLate;
    }
};

class NoPenaltyStrategy : public ILatePenaltyStrategy {
public:
    double calculatePenalty(int, double) const override { return 0.0; }
};

}  // namespace lms
