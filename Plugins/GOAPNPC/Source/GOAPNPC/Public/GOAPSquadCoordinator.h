#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SquadTask.h"
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
	TArray<AGOAPController*> squadMembers;

	virtual void Tick(float DeltaSeconds) override;

private:

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

	/** @return True if every atom in goalAtoms is present with a matching value in currentAtoms. */
	bool IsTaskSatisfied(const TArray<FAtom>& goalAtoms, const TArray<FAtom>& currentAtoms) const;
};