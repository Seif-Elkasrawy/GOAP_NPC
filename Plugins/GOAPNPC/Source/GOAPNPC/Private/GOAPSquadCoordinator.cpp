#include "GOAPSquadCoordinator.h"
#include "GOAPController.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogGOAPSquad, Log, All);

AGOAPSquadCoordinator::AGOAPSquadCoordinator()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGOAPSquadCoordinator::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogGOAPSquad, Warning, TEXT("Coordinator config: %d task(s), %d squad member(s), %d broadcast object(s)"),
		tasks.Num(), squadMembers.Num(), broadcastObjects.Num());

	for (int32 i = 0; i < squadMembers.Num(); ++i)
	{
		UE_LOG(LogGOAPSquad, Warning, TEXT("  squadMembers[%d] = %s"),
			i, squadMembers[i] != nullptr ? *squadMembers[i]->GetName() : TEXT("NULL"));
	}

	for (int32 i = 0; i < broadcastObjects.Num(); ++i)
	{
		UE_LOG(LogGOAPSquad, Warning, TEXT("  broadcastObjects[%d] = %s"),
			i, broadcastObjects[i] != nullptr ? *broadcastObjects[i]->GetName() : TEXT("NULL"));
	}
}

void AGOAPSquadCoordinator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AssignTasks();
	CheckTaskCompletion();
	BroadcastObjectState();
}

void AGOAPSquadCoordinator::AssignTasks()
{
	// Highest priority first.
	TArray<FSquadTask*> unclaimed;
	for (FSquadTask& task : tasks)
	{
		if (task.status == ESquadTaskStatus::Unassigned)
			unclaimed.Add(&task);
	}
	unclaimed.Sort([](const FSquadTask& A, const FSquadTask& B) { return A.priority > B.priority; });

	for (FSquadTask* task : unclaimed)
	{
		AGOAPController* best = nullptr;
		float bestDistSq = TNumericLimits<float>::Max();

		for (APawn* pawn : squadMembers)
		{
			if (pawn == nullptr)
				continue;

			AGOAPController* controller = Cast<AGOAPController>(pawn->GetController());
			if (controller == nullptr)
				continue; // Not yet possessed this frame, or not a GOAPController.

			bool alreadyBusy = tasks.ContainsByPredicate([controller](const FSquadTask& t) { return t.assignedTo == controller; });
			if (alreadyBusy)
				continue;

			// Distance to the TASK's location, not to this coordinator -
			// a coordinator sitting far from both key locations should
			// never bias assignment toward whichever agent is near it.
			float distSq = FVector::DistSquared(task->location, pawn->GetActorLocation());
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				best = controller;
			}
		}

		if (best != nullptr)
		{
			best->setGoal(task->goalAtoms);
			task->assignedTo = best;
			task->status = ESquadTaskStatus::Assigned;

			UE_LOG(LogGOAPSquad, Warning, TEXT("Assigned task (priority %.1f, location %s) to agent %s (dist %.1f)"),
				task->priority, *task->location.ToString(), *best->GetName(), FMath::Sqrt(bestDistSq));
		}
		else
		{
			UE_LOG(LogGOAPSquad, Verbose, TEXT("No available agent for task (priority %.1f, location %s) this tick"),
				task->priority, *task->location.ToString());
		}
	}
}

void AGOAPSquadCoordinator::CheckTaskCompletion()
{
	for (FSquadTask& task : tasks)
	{
		if (task.status != ESquadTaskStatus::Assigned)
			continue;

		if (task.assignedTo == nullptr || task.assignedTo->GetPawn() == nullptr)
		{
			UE_LOG(LogGOAPSquad, Warning, TEXT("Task (priority %.1f, location %s) lost its agent - returning to Unassigned"),
				task.priority, *task.location.ToString());

			task.assignedTo = nullptr;
			task.status = ESquadTaskStatus::Unassigned;
			continue;
		}

		TArray<FAtom> currentAtoms = task.assignedTo->getCurrentWorldStateAtoms();
		if (IsTaskSatisfied(task.goalAtoms, currentAtoms))
		{
			UE_LOG(LogGOAPSquad, Warning, TEXT("Task (priority %.1f, location %s) completed by agent %s"),
				task.priority, *task.location.ToString(), *task.assignedTo->GetName());

			task.assignedTo = nullptr;
			task.status = ESquadTaskStatus::Completed;
		}
	}
}

void AGOAPSquadCoordinator::BroadcastObjectState()
{
	for (AActor* asObject : broadcastObjects)
	{
		if (asObject == nullptr || announcedObjects.Contains(asObject))
			continue;

		if (!asObject->GetClass()->ImplementsInterface(UGOAPBroadcastable::StaticClass()))
		{
			UE_LOG(LogGOAPSquad, Error, TEXT("%s is in broadcastObjects but does not implement IGOAPBroadcastable - skipping"),
				*asObject->GetName());
			announcedObjects.Add(asObject); // Don't re-log this every tick.
			continue;
		}

		if (IGOAPBroadcastable::Execute_IsConditionMet(asObject))
		{
			TArray<FAtom> atoms;
			atoms.Add(IGOAPBroadcastable::Execute_GetBroadcastAtom(asObject));

			UE_LOG(LogGOAPSquad, Warning, TEXT("Broadcasting atom '%s'=true from %s to %d squad member(s)"),
				*atoms[0].name, *asObject->GetName(), squadMembers.Num());

			for (APawn* pawn : squadMembers)
			{
				AGOAPController* controller = pawn != nullptr ? Cast<AGOAPController>(pawn->GetController()) : nullptr;
				if (controller != nullptr)
					controller->updateCurrentWorld(atoms);
			}

			announcedObjects.Add(asObject);
		}
	}
}

bool AGOAPSquadCoordinator::IsTaskSatisfied(const TArray<FAtom>& goalAtoms, const TArray<FAtom>& currentAtoms) const
{
	for (const FAtom& goal : goalAtoms)
	{
		const FAtom* match = currentAtoms.FindByPredicate([&goal](const FAtom& a) { return a.name == goal.name; });
		if (match == nullptr || match->value != goal.value)
			return false;
	}
	return true;
}