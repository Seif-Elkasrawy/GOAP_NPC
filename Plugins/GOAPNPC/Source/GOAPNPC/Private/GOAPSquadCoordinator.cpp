#include "GOAPSquadCoordinator.h"
#include "GOAPController.h"
#include "Logging/LogMacros.h"
#include "DrawDebugHelpers.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY_STATIC(LogGOAPSquad, Log, All);

static TAutoConsoleVariable<bool> CVarGOAPLogSquad(
	TEXT("GOAP.LogSquad"),
	false,
	TEXT("If true, logs squad coordinator events: task assignment, completion, lost agents, give-up handbacks, failed tasks, retries and object broadcasts."),
	ECVF_Default
);

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

	for (const FSquadObjective& objective : objectives)
	{
		for (const FName& name : objective.requiredTasks)
		{
			if (!tasks.ContainsByPredicate([&name](const FSquadTask& t) { return t.taskName == name; }))
			{
				UE_LOG(LogGOAPSquad, Warning, TEXT("Objective '%s' lists task '%s', which does not exist"),
					*objective.objectiveName.ToString(), *name.ToString());
			}
		}
	}
}

void AGOAPSquadCoordinator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RetryFailedTasks();
	AssignTasks();
	CheckTaskCompletion();
	CheckObjectiveCompletion();
	BroadcastObjectState();

#if ENABLE_DRAW_DEBUG
	static const TConsoleVariableData<int32>* debugDrawVar =
		IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("GOAP.DebugDraw"));
	if (debugDrawVar != nullptr && debugDrawVar->GetValueOnGameThread() > 0)
	{
		drawDebug();
	}
#endif
}

void AGOAPSquadCoordinator::AddTask(const FSquadTask& task)
{
	tasks.Add(task);
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

		// Live squad members that have not given up on this task, busy or not.
		// A busy agent is still eligible (it may be free later), so this is
		// counted before the busy check.
		int32 eligibleAgents = 0;

		for (APawn* pawn : squadMembers)
		{
			if (pawn == nullptr)
				continue;

			AGOAPController* controller = Cast<AGOAPController>(pawn->GetController());
			if (controller == nullptr)
				continue; // Not yet possessed this frame, or not a GOAPController.

			if (task->excludedAgents.ContainsByPredicate([controller](const TWeakObjectPtr<AGOAPController>& w) { return w.Get() == controller; }))
				continue; // This agent already gave up on this task.

			++eligibleAgents;
			
			bool alreadyBusy = tasks.ContainsByPredicate([controller](const FSquadTask& t) { return t.assignedTo.Get() == controller; });
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

		if (best == nullptr && bEnablePreemption && eligibleAgents > 0)
		{
			FSquadTask* victimTask = nullptr;
			best = FindPreemptionVictim(*task, victimTask);
			if (best != nullptr)
			{
				bestDistSq = FVector::DistSquared(task->location, best->GetPawn()->GetActorLocation());
				UE_VLOG(this, LogGOAPSquad, Warning, TEXT("Preempted %s from task (prio %.0f) for task (prio %.0f)"),
					*best->GetName(), victimTask->priority, task->priority);

				if (CVarGOAPLogSquad.GetValueOnGameThread())
				{
					UE_LOG(LogGOAPSquad, Log, TEXT("Preempted agent %s from task (priority %.1f) for task (priority %.1f)"),
						*best->GetName(), victimTask->priority, task->priority);
				}
				victimTask->assignedTo.Reset();
				victimTask->status = ESquadTaskStatus::Unassigned;
			}
		}

		if (best != nullptr)
		{
			best->setGoal(task->goalAtoms);
			task->assignedTo = best;
			task->status = ESquadTaskStatus::Assigned;

			UE_VLOG(this, LogGOAPSquad, Log, TEXT("Assigned task (prio %.0f) to %s"), task->priority, *best->GetName());
			UE_VLOG_SEGMENT(this, LogGOAPSquad, Log, best->GetPawn()->GetActorLocation(), task->location,
				FColor::Cyan, TEXT("assign %s"), *best->GetName());
			
			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Assigned task (priority %.1f, location %s) to agent %s (dist %.1f)"),
					task->priority, *task->location.ToString(), *best->GetName(), FMath::Sqrt(bestDistSq));
			}
		}
		else if (eligibleAgents == 0 && !task->excludedAgents.IsEmpty())
		{
			// Somebody gave up on this task and nobody else is left to try it.
			task->status = ESquadTaskStatus::Failed;
			task->failedAtTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

			UE_VLOG(this, LogGOAPSquad, Error, TEXT("Task (priority %.1f, location %s) failed: every available agent has given up on it"),
					task->priority, *task->location.ToString());

			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Task (priority %.1f, location %s) failed: every available agent has given up on it"),
					task->priority, *task->location.ToString());
			}
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

		AGOAPController* agent = task.assignedTo.Get();
		if (agent == nullptr || agent->GetPawn() == nullptr)
		{

			UE_VLOG(this, LogGOAPSquad, Warning, TEXT("Task (priority %.1f, location %s) lost its agent - returning to Unassigned"),
					task.priority, *task.location.ToString());
			
			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Task (priority %.1f, location %s) lost its agent - returning to Unassigned"),
					task.priority, *task.location.ToString());
			}

			task.assignedTo.Reset();
			task.status = ESquadTaskStatus::Unassigned;
			continue;
		}

		// Completion is checked before the give-up flag: a task whose goal is
		// already satisfied is done, even if the failure counter also tripped.
		TArray<FAtom> currentAtoms = agent->getCurrentWorldStateAtoms();
		if (IsTaskSatisfied(task.goalAtoms, currentAtoms))
		{
			UE_VLOG(this, LogGOAPSquad, Log, TEXT("Task (priority %.1f, location %s) completed by agent %s"), task.priority, *task.location.ToString(), *agent->GetName());

			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Task (priority %.1f, location %s) completed by agent %s"),
					task.priority, *task.location.ToString(), *agent->GetName());
			}

			task.assignedTo.Reset();
			task.status = ESquadTaskStatus::Completed;
			continue;
		}

		if (agent->hasGivenUp())
		{
			UE_VLOG(this, LogGOAPSquad, Warning, TEXT("%s gave up on task (prio %.0f), handed back"), *agent->GetName(), task.priority);

			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Agent %s gave up on task (priority %.1f, location %s) - handing it back"),
					*agent->GetName(), task.priority, *task.location.ToString());
			}

			task.excludedAgents.AddUnique(agent);
			task.assignedTo.Reset();
			task.status = ESquadTaskStatus::Unassigned;
		}
	}
}

void AGOAPSquadCoordinator::CheckObjectiveCompletion()
{
	for (FSquadObjective& objective : objectives)
	{
		if (objective.status != ESquadObjectiveStatus::InProgress)
			continue;
		
		int32 completedCount = 0;
		bool permanentlyFailed = false;

		for (const FName& name : objective.requiredTasks)
		{
			const FSquadTask* task = tasks.FindByPredicate([&name](const FSquadTask& t) { return t.taskName == name; });
			if (task == nullptr)
				continue; // unknown name: never counts as completed
			
			if (task->status == ESquadTaskStatus::Completed)
				++completedCount;
			else if (task->status == ESquadTaskStatus::Failed && (failedTaskRetryCooldown <= 0.f || task->retryCount >= maxFailedTaskRetries))
				permanentlyFailed = true;
		}

		
		if (objective.completedTasks != completedCount)
		{
			UE_VLOG(this, LogGOAPSquad, Log, TEXT("Objective '%s' progress %d/%d"), *objective.objectiveName.ToString(), completedCount, objective.requiredTasks.Num());
		}

		objective.completedTasks = completedCount;

		if (objective.requiredTasks.Num() > 0 && completedCount == objective.requiredTasks.Num())
		{
			objective.status = ESquadObjectiveStatus::Completed;
			UE_VLOG(this, LogGOAPSquad, Log, TEXT("Objective '%s' COMPLETED (%d/%d tasks)"),
				*objective.objectiveName.ToString(), completedCount, objective.requiredTasks.Num());
			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Objective '%s' completed (%d/%d tasks)"),
					*objective.objectiveName.ToString(), completedCount, objective.requiredTasks.Num());
			}
			OnObjectiveCompleted.Broadcast(objective.objectiveName);
		}
		else if (permanentlyFailed)
		{
			objective.status = ESquadObjectiveStatus::Failed;
			UE_VLOG(this, LogGOAPSquad, Error, TEXT("Objective '%s' FAILED: a required task failed for good"),
				*objective.objectiveName.ToString());
			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Objective '%s' failed: a required task failed for good"),
					*objective.objectiveName.ToString());
			}
		}
	}
}

void AGOAPSquadCoordinator::RetryFailedTasks()
{
	if (failedTaskRetryCooldown <= 0.f)
		return; // Retrying disabled: Failed tasks stay Failed.

	const double now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	for (FSquadTask& task : tasks)
	{
		if (task.status != ESquadTaskStatus::Failed)
			continue;

		if (task.retryCount >= maxFailedTaskRetries)
			continue; // Retry budget used up: stays Failed for good.

		if (now - task.failedAtTime >= failedTaskRetryCooldown)
		{
			++task.retryCount;
			task.excludedAgents.Empty();
			task.status = ESquadTaskStatus::Unassigned;

			UE_VLOG(this, LogGOAPSquad, Log, TEXT("Retrying failed task (priority %.1f, location %s), attempt %d of %d"), task.priority, *task.location.ToString(), task.retryCount, maxFailedTaskRetries);

			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Retrying failed task (priority %.1f, location %s), attempt %d of %d"),
					task.priority, *task.location.ToString(), task.retryCount, maxFailedTaskRetries);
			}
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

			UE_VLOG(this, LogGOAPSquad, Log, TEXT("Broadcasting atom '%s'=true from %s to %d squad member(s)"), *atoms[0].name, *asObject->GetName(), squadMembers.Num());

			if (CVarGOAPLogSquad.GetValueOnGameThread())
			{
				UE_LOG(LogGOAPSquad, Log, TEXT("Broadcasting atom '%s'=true from %s to %d squad member(s)"),
					*atoms[0].name, *asObject->GetName(), squadMembers.Num());
			}

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

AGOAPController* AGOAPSquadCoordinator::FindPreemptionVictim(const FSquadTask& newTask, FSquadTask*& outVictimTask)
{
	outVictimTask = nullptr;
	AGOAPController* victimAgent = nullptr;
	float lowestPriority = TNumericLimits<float>::Max();
	float bestDistSq = TNumericLimits<float>::Max();

	for (FSquadTask& candidate : tasks)
	{
		if (candidate.status != ESquadTaskStatus::Assigned)
			continue;
		if (candidate.priority + preemptionMargin > newTask.priority)
			continue; // not low enough to take an agent from

		AGOAPController* agent = candidate.assignedTo.Get();
		if (agent == nullptr || agent->GetPawn() == nullptr)
			continue;
		if (agent->hasGivenUp())
			continue; // a handback is already pending
		if (newTask.excludedAgents.ContainsByPredicate([agent](const TWeakObjectPtr<AGOAPController>& w) { return w.Get() == agent; }))
			continue;
		if (IsTaskSatisfied(candidate.goalAtoms, agent->getCurrentWorldStateAtoms()))
			continue; // already done, completion will be credited this tick

		const float distSq = FVector::DistSquared(newTask.location, agent->GetPawn()->GetActorLocation());
		if (candidate.priority < lowestPriority || (candidate.priority == lowestPriority && distSq < bestDistSq))
		{
			lowestPriority = candidate.priority;
			bestDistSq = distSq;
			victimAgent = agent;
			outVictimTask = &candidate;
		}
	}
	return victimAgent;
}

void AGOAPSquadCoordinator::drawDebug() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* world = GetWorld();
	if (world == nullptr)
		return;

	for (const FSquadTask& task : tasks)
	{
		FColor color = FColor::Yellow;
		const TCHAR* statusText = TEXT("UNASSIGNED");

		switch (task.status)
		{
		case ESquadTaskStatus::Assigned:  color = FColor::Cyan;  statusText = TEXT("ASSIGNED"); break;
		case ESquadTaskStatus::Completed: color = FColor::Green; statusText = TEXT("DONE");     break;
		case ESquadTaskStatus::Failed:    color = FColor::Red;   statusText = TEXT("FAILED");   break;
		default: break;
		}

		FString label = FString::Printf(TEXT("%s\nprio %.0f"), statusText, task.priority);

		const AGOAPController* agent = task.assignedTo.Get();
		if (agent != nullptr)
		{
			label += TEXT("\n-> ") + agent->GetName();
			if (const APawn* pawn = agent->GetPawn())
			{
				DrawDebugLine(world, pawn->GetActorLocation(), task.location, color, false, 0.f, 0, 1.f);
			}
		}

		DrawDebugSphere(world, task.location, 40.f, 12, color, false, 0.f, 0, 2.f);
		DrawDebugString(world, task.location + FVector(0.f, 0.f, 80.f), label, nullptr, color, 0.f, true);
	}

	if (GEngine)
	{
		for (int32 i = 0; i < objectives.Num(); ++i)
		{
			const FSquadObjective& obj = objectives[i];
			const FColor objColor = obj.status == ESquadObjectiveStatus::Completed ? FColor::Green :
				obj.status == ESquadObjectiveStatus::Failed ? FColor::Red : FColor::White;
			GEngine->AddOnScreenDebugMessage(2000 + i, 0.f, objColor,
				FString::Printf(TEXT("Objective %s: %d/%d tasks"), *obj.objectiveName.ToString(), obj.completedTasks, obj.requiredTasks.Num()));
		}
	}
#endif
}