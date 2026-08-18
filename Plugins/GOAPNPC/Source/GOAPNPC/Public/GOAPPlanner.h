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
  * The planner uses A* algorithm, classic pathfinding method, to generate the cheapest plan (sequence of actions).
  * Nodes represent possible states of the world and edges represent actions used as transitions between those states.
  */
class GOAPNPC_API GOAPPlanner
{
private:

	GOAPWorldState* currentWorld;

	GOAPWorldState* goal;

	TArray<UGOAPAction*> actions;

	TArray<GOAPNode> openList;

	TArray<GOAPNode> closedList;

	int maxDepth;

	GOAPWorldState* lastWorld = nullptr;
	TArray<UGOAPAction*> lastPlan;

	// Precomputed: atom key ("name_T"/"name_F") -> actions whose effects
	// can satisfy it. Built once from `actions`, avoids scanning every
	// registered action (and calling its checkProceduralPrecondition)
	// on every node expansion.
	TMap<FString, TArray<UGOAPAction*>> effectIndex;

	// Precomputed alongside effectIndex: atom key -> cheapest cost among
	// actions that can produce it. Backs computeHeuristic's sum-of-
	// cheapest-producer-cost estimate.
	TMap<FString, float> cheapestCostIndex;

	void indexAction(UGOAPAction* action);

	// Sum, over subgoal's atoms not already true in currentWorld, of the
	// cheapest known cost to produce each one. not strictly admissible
	// (an action satisfying multiple unmet atoms at once gets its cost
	// counted once per atom), but a standard, informative relaxation.
	float computeHeuristic(SubgoalState subgoal);

public:

	GOAPPlanner();

	~GOAPPlanner();

	GOAPPlanner(GOAPWorldState* c, GOAPWorldState* g, const TArray<UGOAPAction*>& a);

	// lowestFinList REMOVED - openList is now a min-heap by F score
	// (see generatePlan), popped directly via TArray::HeapPop.

	// Returns the index of node within list (matched via GOAPNode's
	// operator==, i.e. same action + subgoalState), or -1 if absent.
	// Used in generatePlan() to detect stale duplicate entries popped
	// from the open-list heap that were already expanded once before
	// a cheaper path superseded them.

	int getIndexInList(GOAPNode node, const TArray<GOAPNode>& list);

	// Returns the nodes adjacent to the current one.
	TArray<GOAPNode> getAdjacent(GOAPNode current, APawn* p);

	// A* algorithm.
	TArray<UGOAPAction*> generatePlan(APawn* p);

	void addAction(UGOAPAction* a);

	//GETS

	GOAPWorldState getGoal();

	GOAPWorldState getCurrentWorld();

	int getMaxDepth();

	//SETS

	void setGoal(GOAPWorldState* g);

	void setCurrentWorld(GOAPWorldState* w);

	void setMaxDepth(int md);

};
