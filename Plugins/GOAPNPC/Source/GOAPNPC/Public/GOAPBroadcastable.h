#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GOAPAction.h"          // for FAtom
#include "GOAPBroadcastable.generated.h"

/**
 * Implement this on ANY object - a door, a vault, a car, a switch,
 * anything a game built on top of this plugin invents - to let
 * AGOAPSquadCoordinator broadcast a fact about it into every squad
 * member's world state once some condition becomes true.
 *
 * The plugin only ever knows about this interface. It has no concept of
 * doors or any other concrete object; that belongs entirely to whatever
 * game-specific class implements it (see e.g. Source/TFG/GOAPKeyDoor for
 * this demo project's door). This is what keeps the coordinator reusable
 * across different games without a Broadcast<Thing>State function per object type.
 */
UINTERFACE(BlueprintType)
class GOAPNPC_API UGOAPBroadcastable : public UInterface
{
	GENERATED_BODY()
};

class GOAPNPC_API IGOAPBroadcastable
{
	GENERATED_BODY()

public:

	/**
	 * @return True once this object's own condition for being announced
	 * to the squad is met (e.g. a door's two sockets are both filled).
	 * Ground truth lives entirely in the implementing class - the
	 * coordinator only ever reads this, never sets it.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = GOAP)
	bool IsConditionMet() const;

	// UHT only generates the Execute_ dispatcher above - the virtual an
	// implementing class actually overrides has to be declared by hand:
	virtual bool IsConditionMet_Implementation() const { return false; }

	/**
	 * @return The atom the coordinator should push into every squad
	 * member's world state once IsConditionMet() is true. Naming (e.g.
	 * "doorAOpen" vs "doorBOpen" vs "vaultOpen") is entirely up to the
	 * implementing class, so multiple objects of the same or different
	 * types can each announce something distinct.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = GOAP)
	FAtom GetBroadcastAtom() const;
	virtual FAtom GetBroadcastAtom_Implementation() const { return FAtom(); }
};