/**
	GOAP NPC: Goal-Oriented Action Planning for Non-Player Characters
	Copyright � 2022 Narratech Laboratories

	Authors: Diego Romero-Hombrebueno Santos, Mario S�nchez Blanco, Jos� Manuel Sierra Ramos, Daniel Gil Aguilar and Federico Peinado
	Website: https://narratech.com/project/goap-npc/
 */
#include "GOAPNode.h"

GOAPNode::GOAPNode()
{
	action = NULL;
	g = 0;
	h = 0;
	parent = -1;
}

GOAPNode::GOAPNode(UGOAPAction* a)
{
	action = a;
	if (a != NULL)g = a->getCost();
	else g = 0;
	h = 0;
	parent = -1;
}

GOAPNode::GOAPNode(UGOAPAction* a, float cost)
{
	action = a;
	g = cost;
	h = 0;
	parent = -1;
}

bool GOAPNode::operator==(const GOAPNode& n) const
{
	return action == n.getAction() && subgoalState == (n.getSubgoalState());
}


const SubgoalState& GOAPNode::getSubgoalState() const
{
	return subgoalState;
}

float GOAPNode::getH() const
{
	return h;
}

float GOAPNode::getG() const
{
	return g;
}

float GOAPNode::getF() const
{
	return g + h;
}

int GOAPNode::getParent() const
{
	return parent;
}

UGOAPAction* GOAPNode::getAction() const
{
	return action;
}

void GOAPNode::setSubgoalState(const SubgoalState& s)
{
	this->subgoalState = s;
}

void GOAPNode::setH(float value)
{
	this->h = value;
}

void GOAPNode::setG(const GOAPNode& p)
{
	this->g += p.getG();
}

void GOAPNode::setParent(int p)
{
	this->parent = p;
}
