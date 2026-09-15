#include "SubgoalState.h"

SubgoalState::SubgoalState() {}

SubgoalState::~SubgoalState() {}

SubgoalState::SubgoalState(const GOAPWorldState& goal)
{
	requirements = goal;
}

bool SubgoalState::operator==(SubgoalState s)
{
	return requirements == s.getWorldState();
}

void SubgoalState::mergeUnsatisfiedRequirements(GOAPWorldState preconditions, GOAPWorldState realWorld)
{
	for (auto requirement : preconditions.getAtoms())
	{
		auto realAtoms = realWorld.getAtoms();
		auto it = realAtoms.find(requirement.first);

		// Only add if NOT already true in the real world.
		if (it == realAtoms.end() || it->second != requirement.second)
		{
			requirements.addAtom(requirement.first, requirement.second);
		}
	}
}

void SubgoalState::removeAtom(FString name)
{
	// GOAPWorldState has no removal method, so rebuild its atom map
	// without this key, then write it back via setAtoms().
	std::map<FString, bool> atoms = requirements.getAtoms();
	atoms.erase(name);
	requirements.setAtoms(atoms);
}

const std::map<FString, bool>& SubgoalState::getAtoms()
{
	return requirements.getAtoms();
}

GOAPWorldState SubgoalState::getWorldState() const
{
	return requirements;
}

bool SubgoalState::isEmpty()
{
	return requirements.isEmpty();
}