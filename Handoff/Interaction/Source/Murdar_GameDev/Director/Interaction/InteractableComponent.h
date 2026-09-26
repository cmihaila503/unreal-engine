// Put on any actor to make it usable with the "E": a door, a phone, a man at a kiosk, a stash under a floorboard.
// Nothing here knows about missions, dialogue or shops: using it publishes Event.Interact (Source = the owner,
// Payload = InteractionTag) and those systems listen. Conditions are facts, so what can be used follows the story.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InteractableComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteracted, UInteractableComponent*, Interactable, APawn*, User);

UCLASS(ClassGroup = (Murdar), meta = (BlueprintSpawnableComponent))
class MURDAR_GAMEDEV_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractableComponent();

	/** The prompt, one short verb phrase in Romanian: „Sună”, „Deschide”, „Vorbește cu Nea Gică”. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Verb;

	/** Published as the Payload of Event.Interact. Missions complete on it, dialogue starts on it. Interact.* */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (Categories = "Interact"))
	FGameplayTag InteractionTag;

	/** Author weight when two things are close together: 2 = a phone beats the bench next to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.1", ClampMax = "5"))
	float Priority = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (Units = "cm", ClampMin = "30", ClampMax = "500"))
	float RangeCm = 150.f;

	/** How far off the player's facing it can be and still get the "E". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (Units = "deg", ClampMin = "5", ClampMax = "180"))
	float MaxAngleDeg = 60.f;

	/** Where the player "reaches" (a handle, a receiver), relative to the owner. Zero = the owner's origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (MakeEditWidget))
	FVector LocalPoint = FVector::ZeroVector;

	/** Usable only when all of these facts are set... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions", meta = (Categories = "Fact,Zone,Mission"))
	FGameplayTagContainer RequiredFacts;

	/** ...and none of these. Put a FactsOnUse fact here too for a saved once-only thing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions", meta = (Categories = "Fact,Zone,Mission"))
	FGameplayTagContainer ForbiddenFacts;

	/** Once per level load (not saved). For "once ever", use FactsOnUse + ForbiddenFacts instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	bool bOnce = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	bool bEnabled = true;

	/** Set on the narrative state when used (saved). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome", meta = (Categories = "Fact"))
	FGameplayTagContainer FactsOnUse;

	/** Blueprint hook for things that do something themselves (a door that opens). */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteracted OnInteracted;

	/** Conditions only (enabled, once, facts) — not distance or facing. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsAvailable() const;

	FVector GetInteractionPoint() const;

	/** Called by UInteractionSubsystem::InteractFocused. Returns false if it was no longer available. */
	bool Use(APawn* User);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool bUsed = false;
};
