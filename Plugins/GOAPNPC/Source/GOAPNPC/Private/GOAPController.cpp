/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright � 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario S�nchez Blanco, Jos� Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#include "GOAPController.h"

static TAutoConsoleVariable<bool> CVarGOAPLogStats(
	TEXT("GOAP.LogStats"),
	false,
	TEXT("If true, logs GOAP planner expansion count and timing on every generatePlan() call."),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarGOAPLogActions(
	TEXT("GOAP.LogActions"),
	false,
	TEXT("If true, logs the result of every doAction call (InProgress/Succeeded/Failed)."),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarGOAPEventDriven(
	TEXT("GOAP.EventDrivenReplanning"),
	true,
	TEXT("If true, agents replan only on events. If false, they replan on every executeGOAP call (continuous baseline)."),
	ECVF_Default
);

AGOAPController::AGOAPController() {}

AGOAPController::~AGOAPController()
{
	delete planner;
	planner = nullptr;
}

void AGOAPController::BeginPlay()
{
	// Loads actions.
	for (auto i = 0; i < actions.Num(); ++i)
	{
		if (actions[i] != NULL) {
			UGOAPAction* actionInstance = NewObject<UGOAPAction>(this, actions[i]);
			FString aux = actionInstance->GetName();
			auxActions.Push(actionInstance);
		}
	}

	// Loads Current World.
	for (FAtom atom : currentWorld)
		wsCurrentWorld.addAtom(atom.name, atom.value);

	// Loads Desired World.
	for (FAtom atom : desiredWorld)
		wsDesiredWorld.addAtom(atom.name, atom.value);

	// Loads actions' preconditions and effects.
	for (UGOAPAction* a : auxActions)
		a->create_P_E();

	// Loads Planner.
	planner = new GOAPPlanner(&wsCurrentWorld, &wsDesiredWorld, auxActions);
	planner->setMaxDepth(maxDepth);
	failureMemory.configure(failurePenalty, penaltyDecayRate);
	planner->setFailureMemory(&failureMemory);

	// Error messages.
	if (auxActions.Num() == 0)
		UE_LOG(LogTemp, Warning, TEXT("Actions not found in GOAPController."));

	if (wsCurrentWorld.isEmpty())
		UE_LOG(LogTemp, Warning, TEXT("Undefined Current World in GOAPController."));

	if (wsDesiredWorld.isEmpty())
		UE_LOG(LogTemp, Warning, TEXT("Undefined Desired World in GOAPController.")); // This is normal in the first execution, isn�t it?

	Super::BeginPlay();
}

void AGOAPController::OnPossess(APawn* pawn)
{
	Super::OnPossess(pawn);
}

void AGOAPController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

bool AGOAPController::executeGOAP()
{
	if (GetPawn() == nullptr)
	{
		return false;
	}

	if (bGaveUp) return false;

	// Replan only on an event: stale plan, no plan, or plan fully executed.
	const bool bNeedsPlan = shouldReplan();
	if (bNeedsPlan && !generatePlan())
	{
		return false;
	}

	if (planStep >= plan.Num())
	{
		return false; // No plan found, or the goal is already satisfied.
	}

	UGOAPAction* aux = plan[planStep];

	const EGOAPActionResult result = aux->doAction(GetPawn());
	lastActionResult = result;

	if (CVarGOAPLogActions.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Log, TEXT("GOAP action: %s (step %d/%d) by %s -> %s"),
			*aux->GetName(), planStep + 1, plan.Num(), *GetName(),
			*UEnum::GetValueAsString(result));
	}

	switch (result)
	{
	case EGOAPActionResult::Succeeded:
		// Effects are applied only on success, changing the current world state.
		wsCurrentWorld.joinWorldState(aux->getEffects());
		++planStep;
		break;
	case EGOAPActionResult::Failed:
	{
		const double now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		failureMemory.recordFailure(aux, now);
		++failuresThisGoal;

		if (failuresThisGoal >= maxFailuresPerGoal)
		{
			bGaveUp = true;
			if (CVarGOAPLogActions.GetValueOnGameThread())
			{
				UE_LOG(LogTemp, Log, TEXT("GOAP agent %s gave up on its goal after %d failures (last: %s)"),
					*GetName(), failuresThisGoal, *aux->GetName());
			}
		}
		else
		{
			markPlanDirty(TEXT("action failed"));
		}
		break;
	}
	
	case EGOAPActionResult::InProgress:
	default:
		break;
	}

	return true;
}

bool AGOAPController::generatePlan()
{
	if (planner == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("GOAPController: planner is null (BeginPlay may not have run yet on this controller)."));
		return false;
	}

	if (GetPawn() == nullptr) return false;
	
	if (auxActions.Num() > 0 && !wsCurrentWorld.isEmpty() && !wsDesiredWorld.isEmpty())
	{
		// Creates the cheapest plan of actions.
		double startTime = FPlatformTime::Seconds();
		plan = planner->generatePlan(GetPawn());
		planStep = 0;
		bPlanDirty = false;
		++planRequestCount;
		lastPlanTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		double elapsedMs = (FPlatformTime::Seconds() - startTime) * 1000.0;

		if (CVarGOAPLogStats.GetValueOnGameThread())
		{
			UE_LOG(LogTemp, Log, TEXT("GOAP benchmark: expansions=%d time=%.4fms"),
				planner->getLastExpansionCount(), elapsedMs);
		}

		if (debug && GEngine) debugInfo();

		return true;
	}

	return false;
}

TArray<UGOAPAction*> AGOAPController::getPlan()
{
	return plan;
}

void AGOAPController::invalidatePlan()
{
	markPlanDirty(TEXT("manual"));
}

int32 AGOAPController::getPlanRequestCount() const
{
	return planRequestCount;
}

void AGOAPController::markPlanDirty(const TCHAR* reason)
{
	bPlanDirty = true;

	if (CVarGOAPLogActions.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Log, TEXT("GOAP replan requested for %s: %s"), *GetName(), reason);
	}
}

bool AGOAPController::shouldReplan() const
{
	// Baseline mode, no planner yet, or something invalidated the plan.
	if (!CVarGOAPEventDriven.GetValueOnGameThread() || planner == nullptr || bPlanDirty)
	{
		return true;
	}

	// Mid-plan: keep executing it.
	if (planStep < plan.Num())
	{
		return false;
	}

	// Plan fully executed: search once to see whether the goal is now satisfied.
	if (plan.Num() > 0)
	{
		return true;
	}

	// Empty plan, goal already satisfied: nothing can change until an event marks the plan dirty.
	if (!planner->didLastPlanFail())
	{
		return false;
	}

	// Empty plan, no plan found: procedural preconditions can start passing without any atom
	// changing, so retry, but slower than the executeGOAP call rate.
	const UWorld* world = GetWorld();
	return world == nullptr || (world->GetTimeSeconds() - lastPlanTimeSeconds) >= noPlanRetryInterval;
}

EGOAPActionResult AGOAPController::getLastActionResult() const
{
	return lastActionResult;
}

bool AGOAPController::hasGivenUp() const
{
	return bGaveUp;
}

void AGOAPController::setGoal(const TArray<FAtom>& newGoal)
{
	wsDesiredWorld.cleanAtoms();
	lastActionResult = EGOAPActionResult::InProgress; // A new goal clears the previous task's result.
	failureMemory.clear();
	failuresThisGoal = 0;
	bGaveUp = false;
	updateGoal(newGoal);
	markPlanDirty(TEXT("goal set"));
}

void AGOAPController::updateGoal(const TArray<FAtom>& atoms)
{
	for (FAtom atom : atoms)
		wsDesiredWorld.addAtom(atom.name, atom.value);

	markPlanDirty(TEXT("goal updated"));
}

void AGOAPController::setCurrentWorld(const TArray<FAtom>& newCurrentWorld)
{
	wsCurrentWorld.cleanAtoms();
	updateCurrentWorld(newCurrentWorld);
	markPlanDirty(TEXT("world replaced"));
}

void AGOAPController::updateCurrentWorld(const TArray<FAtom>& atoms)
{
	// Only a real change invalidates the plan: a broadcast that repeats a
	// value the agent already believes must not force a replan.
	bool bChanged = false;
	const auto& existing = wsCurrentWorld.getAtoms();
	for (const FAtom& atom : atoms)
	{
		auto it = existing.find(atom.name);
		if (it == existing.end() || it->second != atom.value)
		{
			bChanged = true;
		}
		wsCurrentWorld.addAtom(atom.name, atom.value);
	}

	if (bChanged)
	{
		markPlanDirty(TEXT("world changed"));
	}
}

TArray<FAtom> AGOAPController::getCurrentWorldStateAtoms()
{
	TArray<FAtom> worldStateAtoms;
	for (auto atoms : wsCurrentWorld.getAtoms())
	{
		worldStateAtoms.Add({ atoms.first, atoms.second });
	}

	return worldStateAtoms;
}

TArray<FAtom> AGOAPController::getDesiredWorldStateAtoms()
{
	TArray<FAtom> worldStateAtoms;
	for (auto atoms : wsDesiredWorld.getAtoms())
	{
		worldStateAtoms.Add({ atoms.first, atoms.second });
	}

	return worldStateAtoms;
}

void AGOAPController::debugInfo()
{
	FString name = GetPawn()->GetName();

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Plan generated by: " + name), false);
	if (controller)GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("controller: " + this->GetName()), false);
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("\n"), false);

	for (int i = 0; i < plan.Num(); i++) {
		if (i != 0) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("--->"), false);
		FString action = plan[i]->GetName();
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("" + action), false);
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("\n"), false);
}
