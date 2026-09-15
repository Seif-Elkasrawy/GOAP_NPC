/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright � 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario S�nchez Blanco, Jos� Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#pragma once

#include "GOAPAction.h"
#include "SubgoalState.h"
#include "CoreMinimal.h"

/**
 * A single node in the A* search graph the planner explores. Represents a
 * possible state of the world, reached via a chosen action, together with
 * the subgoal that still needs to be satisfied from this point and the
 * costs (g, h, f) A* uses to order and evaluate the open list.
 */
class GOAPNPC_API GOAPNode
{
private:

	SubgoalState subgoalState;

	float h;

	float g;

	int parent;

	// Chosen action to reach this node.
	UGOAPAction* action;

public:

	/** Constructs a node with no action (used for the search root), zero g/h cost. */
	GOAPNode();

	/**
	 * Constructs a node reached via the given action. g is initialised to the
	 * action's own cost (accumulated with the parent's g later via setG); h
	 * starts at zero until set explicitly.
	 *
	 * @param a The action chosen to reach this node.
	 */
	GOAPNode(UGOAPAction* a);

	//OPERATORS 

	/**
	 * Checks whether this node and n represent the same graph node - i.e. the
	 * same action reached with the same remaining subgoal. Does not compare
	 * g, h, or parent, so two nodes with different costs can still be equal
	 * under this check if they represent the same underlying search state.
	 *
	 * @param n The node to compare against.
	 * @return True if both nodes share the same action and subgoal state.
	 */
	bool operator==(GOAPNode n);

	// GETS

	/** @return This node's heuristic cost estimate (remaining cost to the goal). */
	float getH();

	/** @return This node's accumulated path cost from the search root. */
	float getG();

	/** @return This node's total estimated cost (g + h), used to order the open-list heap. */
	float getF();

	/** @return The index of this node's parent within the planner's closed list, or -1 for the root. */
	int getParent();

	/** @return This node's subgoal state - what still needs to be satisfied from this node onward. */
	SubgoalState getSubgoalState();

	/** @return The action chosen to reach this node, or nullptr for the root. */
	UGOAPAction* getAction();

	// SETS

	/**
	 * Replaces this node's subgoal state.
	 *
	 * @param s The subgoal state to assign to this node.
	 */
	void setSubgoalState(SubgoalState s);

	/**
	 * Sets this node's heuristic cost estimate directly. The estimate itself
	 * is computed elsewhere (see GOAPPlanner::computeHeuristic) since it
	 * depends on planner-level data (the cheapest-cost-per-atom index) this
	 * node has no access to.
	 *
	 * @param value The heuristic cost estimate to assign.
	 */
	void setH(float value);

	/**
	 * Accumulates a parent's path cost into this node's own g, turning this
	 * node's g from "this action's own cost" into "total path cost from the
	 * search root through this node".
	 *
	 * @param p The parent node whose g should be added to this node's own.
	 */
	void setG(GOAPNode p);

	/**
	 * Sets this node's parent index within the planner's closed list, used
	 * to reconstruct the plan once the goal is reached.
	 *
	 * @param p Index into the planner's closed list, or -1 for the root.
	 */
	void setParent(int p);


};
