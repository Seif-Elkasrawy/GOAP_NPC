/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright � 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario S�nchez Blanco, Jos� Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#pragma once

#include <map>
#include "CoreMinimal.h"

/**
 * The state of the world (the "logic world" GOAP reasons over) is made up of atoms.
 * An atom is a predicate (a simple string name) paired with a boolean truth value.
 * GOAPWorldState exists to give the planner, actions, and world-state comparisons
 * (satisfaction checks, effect application) a single, uniform representation to
 * operate on, independent of how any individual atom is produced or consumed.
 */
class GOAPNPC_API GOAPWorldState
{
private:

	std::map<FString, bool> atoms;

public:

	/** Constructs an empty world state (no atoms). */
	GOAPWorldState();

	~GOAPWorldState();

	/**
	 * Constructs a world state from a pre-existing atom set.
	 *
	 * @param atoms The initial set of (name, value) atoms this world state holds.
	 */
	GOAPWorldState(const std::map<FString, bool>& atoms);

	/**
	 * Checks whether this world state and w hold exactly the same atoms with the
	 * same values - a full equality check, not a satisfaction/subset check (see
	 * isIncluded for that).
	 *
	 * @param w The world state to compare against.
	 * @return True if both world states contain the same atoms with the same values.
	 */
	bool operator==(GOAPWorldState w);

	/**
	 * Checks whether this world state satisfies every atom w requires - i.e. whether
	 * w's atoms are a subset of this world state's atoms with matching values. This
	 * world state may hold additional atoms not present in w; that does not affect
	 * the result. Used to test whether a goal or subgoal has been achieved by the
	 * current real world state.
	 *
	 * @param w The set of required atoms to check for satisfaction.
	 * @return True if every atom in w is present in this world state with a matching value.
	 */
	bool isIncluded(GOAPWorldState w);

	/** @return This world state's full set of (name, value) atoms. */
	const std::map<FString, bool>& getAtoms();

	/**
	 * Replaces this world state's entire atom set.
	 *
	 * @param atoms The atom set to replace the current one with.
	 */
	void setAtoms(const std::map<FString, bool>& atoms);

	/**
	 * Inserts an atom, or overwrites its value if an atom of the same name already exists.
	 *
	 * @param name The atom's predicate name.
	 * @param value The atom's truth value.
	 */
	void addAtom(FString name, bool value);

	/** Removes every atom, leaving this world state empty. */
	void cleanAtoms();

	/**
	 * Merges w's atoms into this world state: every atom in w is inserted, or
	 * overwrites this world state's existing atom of the same name if one exists.
	 * Atoms this world state holds that are absent from w are left untouched.
	 * Used to apply an action's effects onto the real current world state once
	 * the action has been performed.
	 *
	 * @param w The world state whose atoms should be merged into this one.
	 */
	void joinWorldState(GOAPWorldState w);

	/** @return True if this world state holds no atoms. */
	bool isEmpty();
};
