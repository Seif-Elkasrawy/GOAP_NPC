#pragma once

#include "CoreMinimal.h"
#include "GOAPAction.h"          // for FAtom
#include "SquadTask.generated.h"

class AGOAPController;

UENUM(BlueprintType)
enum class ESquadTaskStatus : uint8
{
	Unassigned,
	Assigned,
	Completed,
	Failed
};

USTRUCT(BlueprintType)
struct FSquadTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SquadTask)
	TArray<FAtom> goalAtoms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SquadTask)
	float priority = 0.f;

	/**
 * Where this task is actually happening in the world. Assignment
 * picks the nearest idle agent to THIS point, not to the coordinator -
 * a coordinator sitting far from both key locations should not bias
 * assignment toward whichever agent happens to be near it.
 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SquadTask)
	FVector location = FVector::ZeroVector;

	/** Runtime only - which agent currently owns this task, if any. */
	UPROPERTY()
	TWeakObjectPtr<AGOAPController> assignedTo;

	/** Runtime only - agents that gave up on this task and must not be handed it again. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AGOAPController>> excludedAgents;

	/** Runtime only - world time the task entered Failed, used for the optional retry cooldown. */
	UPROPERTY()
	double failedAtTime = 0.0;

		/** Runtime only - how many times this Failed task has been retried so far. */
	UPROPERTY()
	int32 retryCount = 0;

	UPROPERTY()
	ESquadTaskStatus status = ESquadTaskStatus::Unassigned;
};