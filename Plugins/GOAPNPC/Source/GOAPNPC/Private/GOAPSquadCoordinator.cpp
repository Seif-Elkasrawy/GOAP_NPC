#include "GOAPSquadCoordinator.h"
#include "GOAPController.h"

void AGOAPSquadCoordinator::AssignTasks()
{
	// Highest priority first.
	TArray<FSquadTask*> unclaimed;
	for (FSquadTask& task : tasks)
	{
		if (task.status == ESquadTaskStatus::Unassigned)
			unclaimed.Add(&task);
	}
   unclaimed.Sort([](const FSquadTask* A, const FSquadTask* B) { return A->priority > B->priority; });

	for (FSquadTask* task : unclaimed)
	{
		AGOAPController* best = nullptr;
		float bestDistSq = TNumericLimits<float>::Max();

		for (AGOAPController* member : squadMembers)
		{
			bool alreadyBusy = tasks.ContainsByPredicate([member](const FSquadTask& t) { return t.assignedTo == member; });
			if (alreadyBusy || member->GetPawn() == nullptr)
				continue;

			float distSq = FVector::DistSquared(GetActorLocation(), member->GetPawn()->GetActorLocation());
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				best = member;
			}
		}

		if (best != nullptr)
		{
			best->setGoal(task->goalAtoms);
			task->assignedTo = best;
			task->status = ESquadTaskStatus::Assigned;
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

            task.assignedTo = nullptr;
            task.status = ESquadTaskStatus::Unassigned;
            continue;
        }

        TArray<FAtom> currentAtoms = task.assignedTo->getCurrentWorldStateAtoms();
        if (IsTaskSatisfied(task.goalAtoms, currentAtoms))
        {
            task.assignedTo = nullptr;
            task.status = ESquadTaskStatus::Completed;
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