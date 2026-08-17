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

	void indexAction(UGOAPAction* action);

public:

	GOAPPlanner();

	~GOAPPlanner();

	GOAPPlanner(GOAPWorldState* c, GOAPWorldState* g, const TArray<UGOAPAction*>& a);

	// Get the node with lowest F's value. 
	// F = G (real cost at this state) + H (estimated cost from this state).
	GOAPNode lowestFinList(const TArray<GOAPNode>& opList);

	int getIndexInOpenList(GOAPNode node, const TArray<GOAPNode>& list);

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
