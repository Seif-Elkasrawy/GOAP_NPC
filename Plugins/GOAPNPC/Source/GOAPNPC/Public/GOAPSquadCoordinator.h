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

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	AGOAPSquadCoordinator();

private:

	/** Objects already announced this run, so each is broadcast to the squad exactly once. */
	UPROPERTY()
	TArray<UObject*> announcedObjects;

	/**
	 * Greedily assigns unclaimed tasks, highest priority first, to the
	 * nearest available (currently unassigned) squad member. Not an
	 * optimal matcher - ties and near-ties are broken arbitrarily by
	 * iteration order, not globally optimised.
	 */
	void AssignTasks();

	/**
	 * Checks each assigned task's goalAtoms against its agent's current
	 * world state (via getCurrentWorldStateAtoms). A task whose atoms are
	 * all satisfied is marked complete and its agent freed for reassignment.
	 */
	void CheckTaskCompletion();

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
};