#pragma once
#include "CoreMinimal.h"

class UGOAPAction;

/** Per-agent record of recent action failures, turned into a cost penalty the planner adds. */
class GOAPNPC_API GOAPFailureMemory
{
public:
    /** Sets the penalty added per failure and how fast it decays. Call once before use. */
    void configure(float inPenaltyPerFailure, float inDecayPerSecond);

	/** Adds one failure for action at time now (stacks on any remaining penalty). */
    void recordFailure(UGOAPAction* action, double now);

    /** @return The action's current penalty after time decay, 0 if it never failed. */
    float getPenalty(UGOAPAction* action, double now) const;
    
	/** Forgets every failure. Keeps the configured tuning. */
    void clear();

private:
    struct FRecord { float penalty = 0.f; double lastUpdate = 0.0; };
    TMap<UGOAPAction*, FRecord> records;
    float penaltyPerFailure = 0.f;
    float decayPerSecond = 0.f;
};