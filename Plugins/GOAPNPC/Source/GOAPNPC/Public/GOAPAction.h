/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright © 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario Sánchez Blanco, José Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#pragma once

#include "GOAPWorldState.h"
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/NoExportTypes.h"
#include "GOAPAction.generated.h"

/**
 * A single (name, value) atom, exposed as a Blueprint-editable struct so
 * designers can author an action's preconditions and effects directly in
 * the editor, without needing to construct a GOAPWorldState by hand.
 */
USTRUCT(BlueprintType, Blueprintable)
struct FAtom
{
	GENERATED_USTRUCT_BODY()

	/** The atom's predicate name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Atom)
	FString name;

	/** The atom's required or resulting truth value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Atom)
		bool value;

};

/**
 * Represents a single action - an edge in the planner's search graph. An
 * action is authored primarily in Blueprint: preconditions and effects are
 * configured as FAtom arrays, cost and target type are set directly, and
 * the two behaviours that genuinely need per-action, designer-authored
 * logic (checkProceduralPrecondition, doAction) are implemented as
 * Blueprint events with no native C++ body, so each action subclass fully
 * owns its own runtime behaviour.
 */
UCLASS(Blueprintable)
class GOAPNPC_API UGOAPAction : public UObject
{
	GENERATED_BODY()

public:

	/** This action's display/lookup name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Properties)
	FString name;

	// Cost of the action. The planner will take this into account when making the cheapest plan.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Properties)
	float cost;

	// Object or class type of actor this action's target should have. This can be None if your action doesn't need a target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Properties)
	TSubclassOf<AActor> targetsType;

	// Preconditions or requirements needed to perform the action.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WorldState)
	TArray<FAtom> preconditions;

	// Effects or postconditions caused by the action.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WorldState)
	TArray<FAtom> effects;


private:

	AActor* target;

	GOAPWorldState wsPreconditions;

	GOAPWorldState wsEffects;

public:

	UGOAPAction();

	/**
	 * Finds every actor in the world matching targetsType. Relies entirely
	 * on UGameplayStatics::GetAllActorsOfClass's own handling of a null or
	 * unset targetsType - this function performs no such check itself.
	 *
	 * @param p The pawn performing the search (used to access the world).
	 * @return Every actor of targetsType currently in the world.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPAction)
		TArray<AActor*> getTargetsList(APawn* p);

	/**
	 * Finds the nearest actor of targetsType to p, excluding p itself, and
	 * assigns it as this action's target via setTarget. Intended to replace
	 * the hand-rolled nearest-target search each action's
	 * checkProceduralPrecondition graph would otherwise need to author
	 * individually.
	 *
	 * @param p The pawn to measure distance from and to exclude as a candidate.
	 * @return True if a target was found and assigned; false if no valid candidate exists.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPAction)
		bool findClosestTarget(APawn* p);

	/**
	 * Optional, per-action check for whether this action can currently be
	 * performed, beyond its declared preconditions - e.g. finding and
	 * validating a target. Implemented entirely in Blueprint; there is no
	 * native C++ body, so every action subclass must provide its own graph.
	 *
	 * @param p The pawn attempting to perform this action.
	 * @return True if this action is currently valid to perform.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = GOAPAction)
		bool checkProceduralPrecondition(APawn* p);

	/**
	 * Performs the action's actual gameplay behaviour. Implemented entirely
	 * in Blueprint; there is no native C++ body. The planner does not call
	 * this directly - GOAPController invokes it once an action is chosen
	 * to execute, then applies the action's declared effects to the real
	 * world state if it succeeds.
	 *
	 * @param p The pawn performing the action.
	 * @return True if the action completed successfully.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = GOAPAction)
		bool doAction(APawn* p);

	/**
	 * Builds this action's internal GOAPWorldState representations
	 * (wsPreconditions, wsEffects) from the Blueprint-authored preconditions
	 * and effects arrays. Must be called once after an action instance is
	 * created and before it is used by the planner - see
	 * AGOAPController::BeginPlay for the call site. Logs a warning if
	 * targetsType is unset.
	 */
	void create_P_E();

	// COMPARATORS

	/**
	 * Checks whether this action and a are equivalent by value - same cost,
	 * same target, same preconditions, and same effects. Does not compare
	 * identity (two distinct instances with identical configuration compare equal).
	 *
	 * @param action The action to compare against.
	 * @return True if both actions are equivalent by value.
	 */
	bool operator==(UGOAPAction& action);

	/** @return True if this action and action are not equivalent by value (see operator==). */
	bool operator!=(UGOAPAction& action);

	// GETS

	/** @return This action's display/lookup name. */
	FString getName();

	/** @return This action's cost, as used by the planner's search and heuristic. */
	float getCost();

	/**
	 * Gets this action's currently assigned target actor - either the one
	 * chosen by findClosestTarget or set directly via setTarget.
	 *
	 * @return The assigned target actor, or nullptr if none has been set.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPAction)
		AActor* getTarget();

	/** @return This action's preconditions, as a GOAPWorldState built from the preconditions array. */
	GOAPWorldState getPreconditions();

	/** @return This action's effects, as a GOAPWorldState built from the effects array. */
	GOAPWorldState getEffects();

	// SETS

	/**
	 * Sets this action's display/lookup name.
	 *
	 * @param n The name to assign.
	 */
	void setName(FString n);

	/**
	 * Sets this action's cost.
	 *
	 * @param c The cost to assign.
	 */
	void setCost(float c);

		/**
	 * Assigns a specific target actor directly, bypassing findClosestTarget.
	 *
	 * @param t The actor to assign as this action's target.
	 */
	UFUNCTION(BlueprintCallable, Category = GOAPAction)
	void setTarget(AActor* t);

	/**
	 * Replaces this action's internal preconditions representation directly.
	 * Bypasses the Blueprint-authored preconditions array and create_P_E -
	 * used when preconditions need to be set programmatically.
	 *
	 * @param preconditionAtoms The preconditions to assign.
	 */
	void setPreconditions(GOAPWorldState preconditionAtoms);

	/**
	 * Replaces this action's internal effects representation directly.
	 * Bypasses the Blueprint-authored effects array and create_P_E -
	 * used when effects need to be set programmatically.
	 *
	 * @param effectAtoms The effects to assign.
	 */
	void setEffects(GOAPWorldState effectAtoms);
};
