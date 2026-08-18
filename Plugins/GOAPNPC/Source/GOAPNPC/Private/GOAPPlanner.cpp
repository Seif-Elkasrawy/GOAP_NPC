/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright � 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario S�nchez Blanco, Jos� Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#include "GOAPPlanner.h"

GOAPPlanner::GOAPPlanner() {}

GOAPPlanner::~GOAPPlanner() {}

static FString MakeEffectKey(const FString& name, bool value)
{
	return name + (value ? TEXT("_T") : TEXT("_F"));
}

void GOAPPlanner::indexAction(UGOAPAction* action)
{
	GOAPWorldState effects = action->getEffects();
	for (auto& effectAtom : effects.getAtoms())
	{
		FString key = MakeEffectKey(effectAtom.first, effectAtom.second);
		effectIndex.FindOrAdd(key).AddUnique(action);

		float cost = action->getCost();
		float* existing = cheapestCostIndex.Find(key);
		if (existing == nullptr || cost < *existing)
		{
			cheapestCostIndex.Add(key, cost);
		}
	}
}

GOAPPlanner::GOAPPlanner(GOAPWorldState* c, GOAPWorldState* g, const TArray<UGOAPAction*>& a)
{
	currentWorld = c;
	goal = g;
	actions = a;

	effectIndex.Empty();
	for (UGOAPAction* action : actions)
	{
		indexAction(action);
	}
}

int GOAPPlanner::getIndexInList(GOAPNode node, const TArray<GOAPNode>& list)
{
	for (int i = 0; i < list.Num(); ++i)
	{
		if (list[i] == node)
			return i;
	}
	return -1;
}

float GOAPPlanner::computeHeuristic(SubgoalState subgoal)
{
	float cost = 0.f;
	for (auto& requirement : subgoal.getAtoms())
	{
		auto it = currentWorld->getAtoms().find(requirement.first);
		bool mismatched = (it == currentWorld->getAtoms().end() || it->second != requirement.second);
		if (!mismatched)
			continue;

		if (const float* cheapest = cheapestCostIndex.Find(MakeEffectKey(requirement.first, requirement.second)))
		{
			cost += *cheapest;
		}
		// No known action can produce this atom: contributes 0 rather than
		// a penalty. Fine while every atom in this demo is reachable; if a
		// no-plan-found case ever needs diagnosing, this is a spot worth
		// revisiting (ties into the live debug view in Tier 1C).
	}
	return cost;
}

TArray<GOAPNode> GOAPPlanner::getAdjacent(GOAPNode current, APawn* p)
{
	TArray<GOAPNode> adjacentNodes;
	SubgoalState currentSubgoal = current.getSubgoalState();

	// Only actions whose effects can satisfy at least one atom this
	// subgoal still needs are candidates - looked up via the precomputed
	// index instead of scanning every registered action.
	TArray<UGOAPAction*> candidates;
	for (auto& requirement : currentSubgoal.getAtoms())
	{
		if (const TArray<UGOAPAction*>* found = effectIndex.Find(MakeEffectKey(requirement.first, requirement.second)))
		{
			for (UGOAPAction* action : *found)
			{
				candidates.AddUnique(action);
			}
		}
	}
	for (UGOAPAction* action : candidates)
	{

		// Checks if the action is the same as the current one. (This can be deleted if you want your AI to perform the same action consecutively).
		const bool bSameActionAsBefore = current.getAction() == action;
		if (bSameActionAsBefore)
			continue;

		// Checks the procedural precondition of the action.
		const bool bProceduralPreconditionFulfilled = action->checkProceduralPrecondition(p);
		if (!bProceduralPreconditionFulfilled)
			continue;

		SubgoalState newSubgoal = currentSubgoal;

		// Membership in `candidates` already guarantees this action
		// resolves at least one atom, so there's no separate
		// "resolvedSomething" check needed here anymore - the index
		// enforces that invariant by construction.
		GOAPWorldState effects = action->getEffects();
		for (auto requirement : currentSubgoal.getAtoms())
		{
			auto effectAtoms = effects.getAtoms();
			auto it = effectAtoms.find(requirement.first);
			if (it != effectAtoms.end() && it->second == requirement.second)
			{
				newSubgoal.removeAtom(requirement.first);
			}
		}

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

	// Min-heap by F score. Takes nodes by value (not const&) because
	// GOAPNode's getters aren't const-qualified - same workaround the
	// old lowestFinList used by iterating "GOAPNode n : opList" by value.
	auto FComparator = [](GOAPNode A, GOAPNode B) { return A.getF() < B.getF(); };

	GOAPNode start;
	start.setSubgoalState(SubgoalState(*goal)); // goal seeds the root
	start.setParent(-1);

	GOAPNode last;
	openList.Empty();
	closedList.Empty();
	openList.HeapPush(start, FComparator);

	bool continues = true;
	bool goalReached = false;

	// Search and create the cheapest path between actions having into account their preconditions, effects and cost.
	while (continues)
	{
		GOAPNode current;
		openList.HeapPop(current, FComparator);

		// Lazy deletion: the heap always pops the lowest-F entry first,
		// so the first time a given (action, subgoalState) is popped it
		// is guaranteed to be its cheapest instance. A later, more
		// expensive duplicate of the same node may still be sitting in
		// the heap from before a cheaper path superseded it - skip it
		// rather than expanding it a second time.
		if (getIndexInList(current, closedList) != -1)
			continue;

		closedList.Push(current);
		int pos = closedList.Num() - 1;

		SubgoalState currentSubgoalForLog = current.getSubgoalState();
		FString subgoalStr;
		for (auto atom : currentSubgoalForLog.getAtoms())
		{
			subgoalStr += atom.first;
			subgoalStr += TEXT("=");
			subgoalStr += atom.second ? TEXT("T") : TEXT("F");
			subgoalStr += TEXT(" ");
		}

		FString actionName = current.getAction() ? current.getAction()->GetName() : TEXT("start");

		UE_LOG(LogTemp, Log, TEXT("GOAP node: action=%s subgoal={ %s} g=%.1f h=%.1f f=%.1f"),
			*actionName, *subgoalStr, current.getG(), current.getH(), current.getF());

		// Termination: does the real world already satisfy what this node still requires?
		if (currentWorld->isIncluded(current.getSubgoalState().getWorldState()))
		{
			last = current;
			continues = false;
			goalReached = true;
			break;
		}

		// Get adjacents of actual node.
		TArray<GOAPNode> adjacents = getAdjacent(current, p);

		// Explore adjacent nodes.
		for (GOAPNode& adjacent : adjacents)
		{
			adjacent.setG(current);
			adjacent.setParent(pos);
			adjacent.setH(computeHeuristic(adjacent.getSubgoalState()));
			openList.HeapPush(adjacent, FComparator);
		}

		// If open list is empty or the algorithm reach the maximum depth, the plan stops.
		if (openList.Num() == 0 || closedList.Num() > getMaxDepth())
		{
			continues = false;
		}
	}

	// Reconstruction: last is closest to the real world (first executable
	// action), start is the goal (last thing accomplished) last -> start
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

	if (!goalReached)
	{
		UE_LOG(LogTemp, Warning, TEXT("GOAPPlanner: no plan found (openList empty or maxDepth reached)."));
	}

	lastExpansionCount = closedList.Num();

	return sol;
}

void GOAPPlanner::addAction(UGOAPAction* a)
{
	this->actions.Push(a);
	indexAction(a);
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

int GOAPPlanner::getLastExpansionCount()
{
	return lastExpansionCount;
}

void GOAPPlanner::setMaxDepth(int md) {
	maxDepth = md;
}
