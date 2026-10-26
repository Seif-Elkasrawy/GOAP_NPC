#include "GOAPFailureMemory.h"

void GOAPFailureMemory::configure(float inPenaltyPerFailure, float inDecayPerSecond)
{
    penaltyPerFailure = inPenaltyPerFailure;
    decayPerSecond = inDecayPerSecond;
}

float GOAPFailureMemory::getPenalty(UGOAPAction* action, double now) const
{
    const FRecord* record = records.Find(action);
    if (record == nullptr)
    {
        return 0.f;
    }
    return FMath::Max(0.f, record->penalty - decayPerSecond * static_cast<float>(now - record->lastUpdate));
}

void GOAPFailureMemory::recordFailure(UGOAPAction* action, double now)
{
    // Decay is applied lazily on read, so fold it in here before stacking.
    FRecord& record = records.FindOrAdd(action);
    record.penalty = getPenalty(action, now);
    record.penalty += penaltyPerFailure;
    record.lastUpdate = now;
}

void GOAPFailureMemory::clear()
{
    records.Empty();
}