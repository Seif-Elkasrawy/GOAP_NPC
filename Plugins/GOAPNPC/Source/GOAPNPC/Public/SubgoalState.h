/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Backward-search subgoal representation.
 */
#pragma once

#include <map>
#include "CoreMinimal.h"
#include "GOAPWorldState.h"


/**
 * Holds the set of atoms that must still become true for a plan to be valid,
 * as regressed backward from the goal during backward-chaining search.
 * Unlike GOAPWorldState (a fixed set of facts - an action's preconditions/
 * effects, or the agent's real current world), a SubgoalState is expected to
 * shrink as actions are found that satisfy its atoms, and grow as those
 * actions' own preconditions are folded in. Composes a GOAPWorldState
 * internally to reuse its atom storage and comparison logic rather than
 * reimplementing it.
 */
class GOAPNPC_API SubgoalState
{
private:

	GOAPWorldState requirements;

public:

	SubgoalState();

	~SubgoalState();

	/**
	 * Seeds a subgoal state directly from the goal - used to build the root
	 * node's subgoal at the start of a backward search.
	 *
	 * @param goal The goal whose atoms become this subgoal's initial requirements.
	 */
	SubgoalState(const GOAPWorldState& goal);

	/**
	 * Checks whether this subgoal and s hold exactly the same requirement
	 * atoms with the same values.
	 *
	 * @param s The subgoal state to compare against.
	 * @return True if both subgoal states require exactly the same atoms.
	 */
	bool operator==(SubgoalState s);

	/**
	 * Folds an action's preconditions into this subgoal as new requirements,
	 * skipping any precondition atom that is already true in realWorld. This
	 * filter is what stops the subgoal from bloating with atoms that don't
	 * need any further resolving as the search regresses backward. Mutates
	 * this instance directly.
	 *
	 * @param preconditions The preconditions of the action being folded in.
	 * @param realWorld The agent's real current world state, used to filter
	 *        out precondition atoms that are already satisfied.
	 */
	void mergeUnsatisfiedRequirements(GOAPWorldState preconditions, GOAPWorldState realWorld);

	/**
	 * Removes a single requirement atom, once an action's effects have
	 * satisfied it. GOAPWorldState has no equivalent operation - this exists
	 * because backward search genuinely needed atom removal and
	 * GOAPWorldState was deliberately left untouched rather than extended.
	 *
	 * @param name The name of the requirement atom to remove.
	 */
	void removeAtom(FString name);

	/** @return This subgoal's full set of (name, value) requirement atoms. */
	const std::map<FString, bool>& getAtoms();

	/**
	 * Exposes the underlying GOAPWorldState so it can be passed into
	 * GOAPWorldState::isIncluded() for the search's termination check,
	 * without GOAPWorldState itself needing to know about SubgoalState.
	 *
	 * @return A copy of this subgoal's requirements as a GOAPWorldState.
	 */
	GOAPWorldState getWorldState() const;

	/** @return True if this subgoal has no remaining requirement atoms - i.e. everything it needed has been satisfied. */
	bool isEmpty();
};