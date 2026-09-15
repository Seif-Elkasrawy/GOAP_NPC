/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright © 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario Sánchez Blanco, José Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#pragma once

#include "GOAPPlanner.h"
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

	//Debug info
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	bool debug;

	//Include controller's name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GOAP)
	bool controller;
	


private:

	GOAPPlanner* planner;

	UPROPERTY()
	TArray<UGOAPAction*> auxActions;

	UPROPERTY()
	TArray<UGOAPAction*> plan;

	GOAPWorldState wsCurrentWorld;

	GOAPWorldState wsDesiredWorld;

public:

	// Allocated in BeginPlay, never deleted - this class has no destructor
	// override, so this leaks one GOAPPlanner per controller instance.
	AGOAPController();

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
	 * Generates a plan (see generatePlan) and, if one exists, performs its
	 * first action via doAction. On success, applies that action's effects
	 * to wsCurrentWorld so the next plan is generated against the updated
	 * world state.
	 *
	 * @return True if an action was performed this call.
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
