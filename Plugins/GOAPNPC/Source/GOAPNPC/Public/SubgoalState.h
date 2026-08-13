/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Backward-search subgoal representation.

	A SubgoalState holds the set of atoms that must still become true
	for a plan to be valid, as regressed backward from the goal.
	Unlike GOAPWorldState (a fixed set of facts — an action's
	preconditions/effects, or the agent's real current world), a
	SubgoalState is expected to shrink as actions are found that
	satisfy its atoms, and grow as those actions' own preconditions
	are folded in.

	Composes a GOAPWorldState internally to reuse its atom storage
	and comparison logic, rather than reimplementing it.
 */
#pragma once

#include <map>
#include "CoreMinimal.h"
#include "GOAPWorldState.h"

class GOAPNPC_API SubgoalState
{
private:

	GOAPWorldState requirements;

public:

	SubgoalState();

	~SubgoalState();

	// Seeds a subgoal state from the goal — used for the search root.
	SubgoalState(const GOAPWorldState& goal);

	bool operator==(SubgoalState s);

	// Every atom in 'preconditions' is added as a new requirement,
	// UNLESS it's already true in realWorld — this is the filter
	// that stops the subgoal state from bloating with atoms that
	// don't need any further resolving. Mutates this instance directly.
	void mergeUnsatisfiedRequirements(GOAPWorldState preconditions, GOAPWorldState realWorld);

	// Removes an atom once an action's effects have satisfied it.
	// GOAPWorldState has no equivalent — this is the operation that
	// was genuinely missing and had to be added for backward search.
	void removeAtom(FString name);

	void set(FString name, bool value);

	const std::map<FString, bool>& getAtoms();

	// Exposes the underlying GOAPWorldState so it can be passed into
	// GOAPWorldState::isIncluded() at the termination check, without
	// GOAPWorldState itself needing to know about SubgoalState.
	GOAPWorldState getWorldState() const;

	bool isEmpty();
};