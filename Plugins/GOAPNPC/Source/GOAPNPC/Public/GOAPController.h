/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright © 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario Sánchez Blanco, José Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#pragma once

#include "GOAPPlanner.h"
#include "GOAPFailureMemory.h"
#include "CoreMinimal.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "GOAPController.generated.h"

 /**
  * AIController that owns a GOAPPlanner, the agent's current and desired
  * world states, and the set of actions it can perform - the Blueprint-
  * facing entry point that ties GOAPWorldState, UGOAPAction, and
  * GOAPPlanner together into a usable AI controller. Current and desired
  * world state are private per instance; a shared/global current world
  * across agents would need to be built on top of this rather than
  * assumed by it.
  */
UCLASS()
class GOAPNPC_API AGOAPController : public AAIController
{
	GENERATED_BODY()

public:

	// State of the current world.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	TArray<FAtom> currentWorld;

	// State of the world in wich the goal has been achieved.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	TArray<FAtom> desiredWorld;

	// List of actions AI can do.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	TArray<TSubclassOf<UGOAPAction>> actions;

	// Maximum algorithm depth.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	int maxDepth = 100;

	// Seconds to wait before searching again after a search found no plan. 0 = retry on every executeGOAP call.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	float noPlanRetryInterval = 1.0f;

	// Tunables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	float failurePenalty = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	float penaltyDecayRate = 0.5f;   // penalty units per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	int32 maxFailuresPerGoal = 6;

	// Private state
	GOAPFailureMemory failureMemory;
	int32 failuresThisGoal = 0;
	bool bGaveUp = false;

	//Debug info
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	bool debug;

	//Include controller's name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	bool controller;



private:

	// Allocated in BeginPlay, deleted in ~AGOAPController.
	GOAPPlanner* planner = nullptr;

	UPROPERTY()
	TArray<UGOAPAction*> auxActions;

	UPROPERTY()
	TArray<UGOAPAction*> plan;

	GOAPWorldState wsCurrentWorld;

	GOAPWorldState wsDesiredWorld;

	// Index into plan of the step executeGOAP will run next.
	int32 planStep = 0;

	// True when something invalidated the cached plan; starts true so the first call plans.
	bool bPlanDirty = true;

	// Number of generatePlan runs on this controller, see getPlanRequestCount().
	int32 planRequestCount = 0;

	// World time of the most recent planner run, used for noPlanRetryInterval.
	double lastPlanTimeSeconds = 0.0;

	/** Decides whether executeGOAP must run the planner this call (see the table in 5.3b). */
	bool shouldReplan() const;

	/** Sets bPlanDirty; logs the reason when GOAP.LogActions is enabled. */
	void markPlanDirty(const TCHAR* reason);

	// Result of the most recent doAction call, see getLastActionResult().
	EGOAPActionResult lastActionResult = EGOAPActionResult::InProgress;

public:

	AGOAPController();
	virtual ~AGOAPController() override;
	/**
	 * Instantiates this controller's actions from actions, loads
	 * currentWorld/desiredWorld into wsCurrentWorld/wsDesiredWorld, and
	 * constructs the GOAPPlanner. desiredWorld may legitimately be empty
	 * here if the goal is meant to be assigned dynamically afterward via
	 * setGoal/updateGoal, rather than authored on this property directly -
	 * the resulting warning is informational, not necessarily an error.
	 */
	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* pawn) override;

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Executes one step of the cached plan. Plans first only if the plan
	 * is stale (see invalidatePlan), empty, or fully executed; otherwise
	 * keeps the existing plan. Calls doAction on plan[planStep]:
	 * Succeeded applies the action's effects and advances to the next step,
	 * InProgress changes nothing, Failed is recorded in lastActionResult and
	 * marks the plan stale so the next call replans.
	 *
	 * @return True if an action was performed this call (whatever its result).
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	bool executeGOAP();

	/**
	 * Runs the planner (see GOAPPlanner::generatePlan) against this
	 * controller's current world, goal, and actions, storing the result in
	 * plan. Does nothing and returns false if the planner hasn't been
	 * constructed yet (BeginPlay hasn't run), or if actions/current
	 * world/goal aren't all set up. Logs benchmark stats if GOAP.LogStats
	 * is enabled, and shows on-screen plan info if debug is set.
	 *
	 * @return True if a plan was generated (even if it's empty - see getPlan).
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	bool generatePlan();

	/** @return The most recently generated plan, ordered for execution. */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	TArray<UGOAPAction*> getPlan();

	/**
	 * Marks the cached plan as stale, so the next executeGOAP call
	 * generates a new one. Called internally on goal changes, world-state
	 * changes and action failures; also callable from Blueprint (e.g. from
	 * an interactive-showcase event) to force a replan.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	void invalidatePlan();

	/** @return How many times this controller has run the planner (for benchmarking plan requests). */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	int32 getPlanRequestCount() const;

	/**
	* @return The result of the most recent doAction call made by
	* executeGOAP. InProgress until the first action has run.
	*/
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	EGOAPActionResult getLastActionResult() const;

	/**
	 * @return True if the agent used up its failure budget on the current
	 * goal (maxFailuresPerGoal) and stopped executing. Cleared by setGoal.
	 * The squad coordinator reads this to hand the task to another agent.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	bool hasGivenUp() const;

	/**
	 * Replaces this controller's goal entirely with newGoal.
	 *
	 * @param newGoal The atoms defining the new goal state.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	void setGoal(const TArray<FAtom>& newGoal);

	/**
	 * Adds atoms to the current goal, or overwrites their values if atoms
	 * of the same name already exist. Unlike setGoal, does not clear
	 * existing goal atoms first.
	 *
	 * @param atoms The atoms to add or overwrite in the goal.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	void updateGoal(const TArray<FAtom>& atoms);

	/**
	 * Replaces this controller's current world state entirely with
	 * newCurrentWorld.
	 *
	 * @param newCurrentWorld The atoms defining the new current world state.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	void setCurrentWorld(const TArray<FAtom>& newCurrentWorld);

	/**
	 * Adds atoms to the current world state, or overwrites their values if
	 * atoms of the same name already exist. Unlike setCurrentWorld, does
	 * not clear existing atoms first.
	 *
	 * @param atoms The atoms to add or overwrite in the current world state.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	void updateCurrentWorld(const TArray<FAtom>& atoms);

	/** @return This controller's current world state, as an array of atoms. */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	TArray<FAtom> getCurrentWorldStateAtoms();

	/** @return This controller's goal state, as an array of atoms. */
	UFUNCTION(BlueprintCallable, Category = GOAPController)
	TArray<FAtom> getDesiredWorldStateAtoms();

private:
	/** Shows the current plan on-screen via GEngine::AddOnScreenDebugMessage, when debug is set. */
	void debugInfo();
};
