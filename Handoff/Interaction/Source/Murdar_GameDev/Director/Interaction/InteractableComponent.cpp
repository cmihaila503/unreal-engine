#include "Director/Interaction/InteractableComponent.h"

#include "Director/Interaction/InteractionSubsystem.h"
#include "Director/GameEventSubsystem.h"
#include "Director/NarrativeStateSubsystem.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractableComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UInteractionSubsystem* Sub = UInteractionSubsystem::Get(this)) { Sub->Register(this); }
	if (Verb.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Interactable on %s has no Verb: the prompt will be empty"), *GetNameSafe(GetOwner()));
	}
}

void UInteractableComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UInteractionSubsystem* Sub = UInteractionSubsystem::Get(this)) { Sub->Unregister(this); }
	Super::EndPlay(Reason);
}

bool UInteractableComponent::IsAvailable() const
{
	if (!bEnabled || (bOnce && bUsed)) { return false; }
	if (RequiredFacts.IsEmpty() && ForbiddenFacts.IsEmpty()) { return true; }
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	return State && State->Check(RequiredFacts, ForbiddenFacts);
}

FVector UInteractableComponent::GetInteractionPoint() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform().TransformPosition(LocalPoint) : FVector::ZeroVector;
}

bool UInteractableComponent::Use(APawn* User)
{
	if (!IsAvailable()) { return false; }
	bUsed = true;

	if (!FactsOnUse.IsEmpty())
	{
		if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this))
		{
			for (const FGameplayTag& Fact : FactsOnUse) { State->SetFact(Fact); }
		}
	}

	// Blueprint first (the door opens), then the world hears about it (a mission completes, a dialogue starts).
	OnInteracted.Broadcast(this, User);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		static const FGameplayTag EventInteract = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Interact")), false);
		Bus->Publish(FGameEvent(EventInteract, GetOwner(), GetInteractionPoint(), 0.f, InteractionTag));
	}
	return true;
}
