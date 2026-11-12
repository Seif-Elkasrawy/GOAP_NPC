#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SquadTask.h"
#include "GOAPBroadcastable.h"
#include "GOAPSquadCoordinator.generated.h"

class AGOAPController;

/**
 * Owns a designer-authored list of tasks and a squad of agents, and
 * assigns unclaimed tasks to available agents via a greedy nearest-agent
 * match. Assignment happens purely through each agent's existing
 * GOAPController::setGoal - the coordinator has no other hook into an
 * agent's behaviour and needs none.
 */
UCLASS()
class GOAPNPC_API AGOAPSquadCoordinator : public AActor
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Squad)
	TArray<FSquadTask> tasks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Squad)
	TArray<APawn*> squadMembers;

	/**
	 * Any objects the coordinator should watch and, once each reports
	 * IsConditionMet(), announce to the whole squad. Fully generic - a
	 * door, a vault, a car, anything implementing IGOAPBroadcastable.
	 * The plugin puts no constraint on what these are.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Squad)
	TArray<AActor*> broadcastObjects;
	
	/** Seconds after which a Failed task is retried (excluded agents cleared). 0 = never retry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Squad)
	float failedTaskRetryCooldown = 0.f;

	/**
	 * Maximum number of times a Failed task is retried. Only applies when
	 * failedTaskRetryCooldown is greater than 0. Once reached, the task
	 * stays Failed for good.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Squad)
	int32 maxFailedTaskRetries = 3;

	/** If true, a task with no free agent may take one from a lower-priority task. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Squad)
	bool bEnablePreemption = false;

	/** A task may only preempt a task whose priority is at least this much lower. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Squad)
	float preemptionMargin = 1.f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = Squad)
	void AddTask(const FSquadTask& task);

	AGOAPSquadCoordinator();

private:

	/** Objects already announced this run, so each is broadcast to the squad exactly once. */
	UPROPERTY()
	TArray<UObject*> announcedObjects;

	/**
	 * Greedily assigns unclaimed tasks, highest priority first, to the
	 * nearest available (currently unassigned) squad member, skipping
	 * agents in the task's excludedAgents. A task that has excluded agents
	 * and no eligible agent left (every live squad member gave up on it)
	 * is marked Failed. Not an optimal matcher - ties and near-ties are
	 * broken arbitrarily by iteration order, not globally optimised.
	 */
	void AssignTasks();

	/**
	 * Checks each assigned task against its agent. A task whose agent was
	 * lost is returned to Unassigned. A task whose goalAtoms are all
	 * satisfied in the agent's world state (via getCurrentWorldStateAtoms)
	 * is marked complete and its agent freed. A task whose agent reports
	 * hasGivenUp() is handed back: it returns to Unassigned and that agent
	 * is added to its excludedAgents so it is not handed straight back.
	 */
	void CheckTaskCompletion();

	/**
	 * Retries Failed tasks once failedTaskRetryCooldown has passed since
	 * they failed: clears their excludedAgents, increments retryCount and
	 * returns them to Unassigned. Does nothing when the cooldown is 0, and
	 * leaves a task Failed once it has used maxFailedTaskRetries.
	 */
	void RetryFailedTasks();

	/**
	 * For each entry in broadcastObjects not yet announced: if
	 * IsConditionMet() is true, pushes GetBroadcastAtom() into every
	 * squad member's world state via updateCurrentWorld, and marks it
	 * announced. Purely a belief broadcast - reads the object, never
	 * writes to it, and has no idea what kind of object it actually is.
	 */
	void BroadcastObjectState();

	/** @return True if every atom in goalAtoms is present with a matching value in currentAtoms. */
	bool IsTaskSatisfied(const TArray<FAtom>& goalAtoms, const TArray<FAtom>& currentAtoms) const;

	/**
	 * Finds an agent to take from a lower-priority task for newTask. Candidates are
	 * agents on Assigned tasks at least preemptionMargin lower in priority, excluding
	 * agents that already gave up, agents whose goal is already satisfied, and agents
	 * excluded from newTask. Picks the lowest priority first, then the nearest to
	 * newTask's location.
	 *
	 * @param newTask The task that needs an agent.
	 * @param outVictimTask Set to the victim's task, or nullptr if none was found.
	 * @return The agent to take, or nullptr if there is no valid victim.
	 */
	AGOAPController* FindPreemptionVictim(const FSquadTask& newTask, FSquadTask*& outVictimTask);

	void drawDebug() const;
};