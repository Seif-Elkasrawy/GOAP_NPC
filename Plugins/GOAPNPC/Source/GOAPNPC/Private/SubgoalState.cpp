#include "SubgoalState.h"

SubgoalState::SubgoalState() {}

SubgoalState::~SubgoalState() {}

SubgoalState::SubgoalState(const GOAPWorldState& goal)
{
	requirements = goal;
}

bool SubgoalState::operator==(const SubgoalState& s) const
{
	return requirements == s.getWorldState();
}

void SubgoalState::mergeUnsatisfiedRequirements(const GOAPWorldState& preconditions, const GOAPWorldState& realWorld)
{
	const auto& realAtoms = realWorld.getAtoms();
	for (const auto& requirement : preconditions.getAtoms())
	{
		auto it = realAtoms.find(requirement.first);

		// Only add if NOT already true in the real world.
		if (it == realAtoms.end() || it->second != requirement.second)
		{
			requirements.addAtom(requirement.first, requirement.second);
		}
	}
}

void SubgoalState::removeAtom(const FString& name)
{
	// GOAPWorldState has no removal method, so rebuild its atom map
	// without this key, then write it back via setAtoms().
	std::map<FString, bool> atoms = requirements.getAtoms();
	atoms.erase(name);
	requirements.setAtoms(atoms);
}

const std::map<FString, bool>& SubgoalState::getAtoms() const
{
	return requirements.getAtoms();
}

const GOAPWorldState& SubgoalState::getWorldState() const
{
	return requirements;
}

bool SubgoalState::isEmpty() const
{
	return requirements.isEmpty();
}