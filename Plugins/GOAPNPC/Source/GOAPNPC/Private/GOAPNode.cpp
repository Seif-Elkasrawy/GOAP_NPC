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
}

GOAPNode::GOAPNode(UGOAPAction* a)
{
	action = a;
	if (a != NULL)g = a->getCost();
	else g = 0;
	h = 0;
}

bool GOAPNode::operator==(GOAPNode n)
{
	return action == n.getAction() && subgoalState == (n.getSubgoalState());
}


SubgoalState GOAPNode::getSubgoalState()
{
	return subgoalState;
}

float GOAPNode::getH()
{
	return h;
}

float GOAPNode::getG()
{
	return g;
}

float GOAPNode::getF()
{
	return g + h;
}

int GOAPNode::getParent()
{
	return parent;
}

UGOAPAction* GOAPNode::getAction()
{
	return action;
}

void GOAPNode::setSubgoalState(SubgoalState s)
{
	this->subgoalState = s;
}

void GOAPNode::setH(float value)
{
	this->h = value;
}

void GOAPNode::setG(GOAPNode p)
{
	this->g += p.getG();
}

void GOAPNode::setParent(int p)
{
	this->parent = p;
}
