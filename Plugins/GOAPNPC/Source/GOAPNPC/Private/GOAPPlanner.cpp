/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright © 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario Sánchez Blanco, José Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#include "GOAPPlanner.h"

GOAPPlanner::GOAPPlanner() {}

GOAPPlanner::~GOAPPlanner() {}

GOAPPlanner::GOAPPlanner(GOAPWorldState* c, GOAPWorldState* g, const TArray<UGOAPAction*>& a)
{
	currentWorld = c;
	goal = g;
	actions = a;
}

GOAPNode GOAPPlanner::lowestFinList(const TArray<GOAPNode>& opList)
{
	GOAPNode node;

	float minF = MAX_FLT;
	for (GOAPNode n : opList)
	{
		if ((n.getF()) < minF)
		{
			node = n;
			minF = n.getF();
		}
	}

	return node;
}
//
//bool containsNode(GOAPNode node, const TArray<GOAPNode>& list)
//{
//	bool contains = false;
//	for (GOAPNode n : list)
//	{
//		if (n == node)
//		{
//			contains = true;
//			break;
//		}
//	}
//	return contains;
//}

int getIndexInOpenList(GOAPNode node, const TArray<GOAPNode>& list)
{
	for (int i = 0; i < list.Num(); ++i)
	{
		if (list[i] == node)
			return i;
	}
	return -1;
}

TArray<GOAPNode> GOAPPlanner::getAdjacent(GOAPNode current, const TArray<UGOAPAction*>& vActions, APawn* p)
{
	TArray<GOAPNode> adjacentNodes;
	SubgoalState currentSubgoal = current.getSubgoalState();

	for (int i = 0; i < vActions.Num(); ++i)
	{
		UGOAPAction* action = vActions[i];

		// Checks if the action is the same as the current one. (This can be deleted if you want your AI to perform the same action consecutively).
		const bool bSameActionAsBefore = current.getAction() == action;
		if (bSameActionAsBefore)
			continue;
		// Checks the procedural precondition of the action.
		const bool bProceduralPreconditionFulfilled = action->checkProceduralPrecondition(p);
		if (!bProceduralPreconditionFulfilled)
			continue;

		SubgoalState newSubgoal = currentSubgoal;
		bool resolvedSomething = false;

		// Single pass: match action's effects against subgoal atoms,
		// subtract on match, and track whether this action qualifies at all.
		GOAPWorldState effects = action->getEffects();
		for (auto requirement : currentSubgoal.getAtoms()) 
		{
			auto effectAtoms = effects.getAtoms();
			auto it = effectAtoms.find(requirement.first);
			if (it != effectAtoms.end() && it->second == requirement.second)
			{
				newSubgoal.removeAtom(requirement.first);
				resolvedSomething = true;
			}
		}
		if (!resolvedSomething)
			continue; // doesn't resolve anything we still need

		newSubgoal.mergeUnsatisfiedRequirements(action->getPreconditions(), *currentWorld);

		GOAPNode adjacent(action); // constructor sets g = action's cost, h = 0
		adjacent.setSubgoalState(newSubgoal);
		adjacentNodes.Push(adjacent);

	}

	return adjacentNodes;
}

TArray<UGOAPAction*> GOAPPlanner::generatePlan(APawn* p)
{
	TArray<UGOAPAction*> sol;

	GOAPNode start;
	start.setSubgoalState(SubgoalState(*goal)); // goal seeds the root
	start.setParent(-1);

	GOAPNode last;
	openList.Empty();
	closedList.Empty();
	openList.Push(start);

	bool continues = true;
	bool goalReached = false;

	// Search and create the cheapest path between actions having into account their preconditions, effects and cost.
	while (continues)
	{
		GOAPNode current = lowestFinList(openList);
		openList.Remove(current);
		closedList.Push(current);
		int pos = closedList.Num() - 1;

		// Termination: does the real world already satisfy what this node still requires?
		if (currentWorld->isIncluded(current.getSubgoalState().getWorldState()))
		{
			last = current;
			continues = false;
			goalReached = true;
			break;
		}

		// Get adjacents of actual node.
		TArray<GOAPNode> adjacents = getAdjacent(current, actions, p);

		// Explore adjacent nodes.
		for (GOAPNode& adjacent : adjacents)
		{
			adjacent.setG(current);

			int existingIndex = getIndexInOpenList(adjacent, openList);

			// If the adjacent node isn't in the open list, it is added.
			if (existingIndex == -1)
			{
				adjacent.setParent(pos);
				adjacent.setH(*currentWorld);
				openList.Push(adjacent);
			}
			// If current path to adjacent node is cheaper than the previous one, the path changes. 
			else if (adjacent.getG() < openList[existingIndex].getG())
			{
				openList[existingIndex].setParent(pos);
				openList[existingIndex].setG(current);
			}
		}

		// If open list is empty or the algorithm reach the maximum depth, the plan stops.
		if (openList.Num() == 0 || closedList.Num() > getMaxDepth())
		{
			continues = false;
		}
	}

	// Reconstruction: last is closest to the real world (first executable
	// action), start is the goal (last thing accomplished) — last -> start
	// now produces correct execution order directly.
	if (goalReached)
	{
		GOAPNode planNode = last;
		while (!(planNode == start))
		{
			sol.Push(planNode.getAction());
			planNode = closedList[planNode.getParent()];
		}
	}

	return sol;
}

void GOAPPlanner::addAction(UGOAPAction* a)
{
	this->actions.Push(a);
}

GOAPWorldState GOAPPlanner::getGoal()
{
	return *goal;
}
void GOAPPlanner::setGoal(GOAPWorldState* g)
{
	this->goal = g;
}
GOAPWorldState GOAPPlanner::getCurrentWorld()
{
	return *currentWorld;
}
void GOAPPlanner::setCurrentWorld(GOAPWorldState* w)
{
	this->currentWorld = w;
}

int GOAPPlanner::getMaxDepth() {
	return maxDepth;
}

void GOAPPlanner::setMaxDepth(int md) {
	maxDepth = md;
}
