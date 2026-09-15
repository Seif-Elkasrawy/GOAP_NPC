/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright � 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario S�nchez Blanco, Jos� Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#pragma once

#include "GOAPWorldState.h"
#include "GOAPNode.h"
#include "GOAPAction.h"
#include "CoreMinimal.h"

/**
 * Generates the cheapest sequence of actions (a plan) that transforms
 * currentWorld into a state satisfying goal, using backward-chaining A*
 * search: nodes represent a shrinking subgoal (what still needs to become
 * true), and edges represent actions that can satisfy part of that subgoal.
 * Precomputes an effect index and a cheapest-cost-per-atom index from its
 * action set once, so that both candidate lookup (getAdjacent) and the
 * search heuristic (computeHeuristic) avoid scanning every registered
 * action on every node expansion.
 */
class GOAPNPC_API GOAPPlanner
{
private:

	// Not owned by this planner - set by whoever constructs it (see
	// AGOAPController::BeginPlay) and never deleted here.
	GOAPWorldState* currentWorld;

	// Not owned by this planner - see currentWorld.
	GOAPWorldState* goal;

	TArray<UGOAPAction*> actions;

	TArray<GOAPNode> openList;

	TArray<GOAPNode> closedList;

	int maxDepth;

	// Set at the end of every generatePlan() call to closedList.Num() -
	// the number of nodes genuinely expanded (lazy-deletion duplicates
	// popped from openList but skipped are not counted). Read via
	// getLastExpansionCount() for benchmarking/diagnostics.
	int lastExpansionCount = 0;

	// Precomputed: atom key ("name_T"/"name_F") -> actions whose effects
	// can satisfy it. Built once from `actions`, avoids scanning every
	// registered action (and calling its checkProceduralPrecondition)
	// on every node expansion.
	TMap<FString, TArray<UGOAPAction*>> effectIndex;

	// Precomputed alongside effectIndex: atom key -> cheapest cost among
	// actions that can produce it. Backs computeHeuristic's sum-of-
	// cheapest-producer-cost estimate.
	TMap<FString, float> cheapestCostIndex;

	/**
	 * Indexes a single action's effects into effectIndex and
	 * cheapestCostIndex. Called once per action during construction, and
	 * again from addAction when an action is registered afterward.
	 *
	 * @param action The action to index.
	 */
	void indexAction(UGOAPAction* action);

	/**
	 * Estimates the remaining cost to satisfy subgoal, as the sum of the
	 * cheapest known producing action's cost for every atom in subgoal
	 * that is not already true in currentWorld. Not strictly admissible
	 * (an action satisfying multiple unmet atoms at once has its cost
	 * counted once per atom), but a standard, informative relaxation.
	 * An atom with no known producing action contributes 0 rather
	 * than a penalty.
	 *
	 * @param subgoal The subgoal to estimate remaining cost for.
	 * @return The summed heuristic cost estimate.
	 */
	float computeHeuristic(SubgoalState subgoal);

public:

	GOAPPlanner();

	/** Does not delete currentWorld or goal - this planner never owns them. */
	~GOAPPlanner();

	/**
	 * Constructs a planner over a given world state, goal, and action set,
	 * building the effect and cheapest-cost indices from that action set
	 * immediately.
	 *
	 * @param c The agent's real current world state. Not owned - must
	 *        outlive this planner.
	 * @param g The goal state to plan toward. Not owned - must outlive
	 *        this planner.
	 * @param a The full set of actions available to the planner.
	 */
	GOAPPlanner(GOAPWorldState* c, GOAPWorldState* g, const TArray<UGOAPAction*>& a);

	/**
	 * Finds node's index within list, matched via GOAPNode::operator==
	 * (same action + subgoal state - g, h, and parent are not compared).
	 * Used in generatePlan() to detect stale duplicate entries popped from
	 * the open-list heap that were already expanded once before a cheaper
	 * path superseded them.
	 *
	 * @param node The node to search for.
	 * @param list The list to search within.
	 * @return The index of node within list, or -1 if not present.
	 */
	int getIndexInList(GOAPNode node, const TArray<GOAPNode>& list);

	/**
	 * Finds every action that could serve as a valid next step from current
	 * toward satisfying its subgoal: the action's effects must resolve at
	 * least one of the subgoal's atoms (via the precomputed effectIndex),
	 * it must not be the same action just used to reach current, and its
	 * checkProceduralPrecondition must currently pass. Each accepted
	 * action produces one adjacent node with a new subgoal - the current
	 * one, minus whatever atoms that action's effects satisfy, plus that
	 * action's own preconditions (filtered against currentWorld).
	 *
	 * @param current The node to find adjacents of.
	 * @param p The pawn the plan is being generated for - passed through
	 *        to each candidate action's checkProceduralPrecondition.
	 * @return Every node reachable from current via one valid action.
	 */
	TArray<GOAPNode> getAdjacent(GOAPNode current, APawn* p);

	/**
	 * Runs backward-chaining A* search from goal toward currentWorld,
	 * returning the resulting plan as an ordered array of actions (index 0
	 * is the next action to perform). The open list is a min-heap ordered
	 * by F score (see getAdjacent, computeHeuristic); duplicate entries
	 * are handled via lazy deletion rather than in-place cost updates,
	 * since mutating a heap element's cost in place would break heap
	 * ordering. Search stops once a node's subgoal is fully satisfied by
	 * currentWorld, the open list empties, or maxDepth is exceeded - in
	 * the latter two cases an empty array is returned and a warning is logged.
	 *
	 * @param p The pawn to plan for - passed through to
	 *        checkProceduralPrecondition during candidate evaluation.
	 * @return The resulting plan, ordered for execution, or an empty array
	 *         if no plan was found.
	 */
	TArray<UGOAPAction*> generatePlan(APawn* p);

	/**
	 * Registers an additional action with this planner after construction,
	 * indexing it into effectIndex and cheapestCostIndex immediately.
	 *
	 * @param a The action to register.
	 */
	void addAction(UGOAPAction* a);

	//GETS

	/** @return A copy of this planner's current goal state. */
	GOAPWorldState getGoal();

	/** @return A copy of this planner's current world state. */
	GOAPWorldState getCurrentWorld();

	/** @return The maximum number of nodes generatePlan() will expand before giving up. */
	int getMaxDepth();

	/** @return The number of nodes genuinely expanded during the most recent generatePlan() call. */
	int getLastExpansionCount();

	//SETS

	/**
	 * Replaces the goal this planner searches toward. Does not take
	 * ownership - g must outlive this planner.
	 *
	 * @param g The goal state to plan toward.
	 */
	void setGoal(GOAPWorldState* g);

	/**
	 * Replaces the world state this planner treats as the agent's real
	 * current state. Does not take ownership - w must outlive this planner.
	 *
	 * @param w The world state to treat as current.
	 */
	void setCurrentWorld(GOAPWorldState* w);

	/**
	 * Sets the maximum number of nodes generatePlan() will expand before
	 * giving up and returning an empty plan.
	 *
	 * @param md The maximum expansion count.
	 */
	void setMaxDepth(int md);

};
