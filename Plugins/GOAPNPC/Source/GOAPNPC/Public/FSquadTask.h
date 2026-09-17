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
	Completed
};

USTRUCT(BlueprintType)
struct FSquadTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SquadTask)
	TArray<FAtom> goalAtoms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SquadTask)
	float priority = 0.f;

	/** Runtime only - which agent currently owns this task, if any. */
	UPROPERTY()
	AGOAPController* assignedTo = nullptr;

	UPROPERTY()
	ESquadTaskStatus status = ESquadTaskStatus::Unassigned;
};