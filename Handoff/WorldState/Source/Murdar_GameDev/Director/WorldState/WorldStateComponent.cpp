#include "Director/WorldState/WorldStateComponent.h"

#include "Director/WorldState/WorldStateSubsystem.h"

#include "GameFramework/Actor.h"

UWorldStateComponent::UWorldStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FName UWorldStateComponent::GetStableId() const
{
	const AActor* Owner = GetOwner();
	// RF_WasLoaded: the actor came from the level file, so its name is the same next session.
	return Owner && Owner->HasAnyFlags(RF_WasLoaded) ? Owner->GetFName() : NAME_None;
}

void UWorldStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetStableId().IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("WorldState on %s: not a placed actor, nothing will be saved"), *GetNameSafe(GetOwner()));
		return;
	}
	if (UWorldStateSubsystem* Sub = UWorldStateSubsystem::Get(this)) { Sub->Register(this); }
}

void UWorldStateComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorldStateSubsystem* Sub = UWorldStateSubsystem::Get(this))
	{
		// Destroyed in play (not the level unloading): remember it now, the actor won't be there at save time.
		if (Reason == EEndPlayReason::Destroyed && bSaveDestroyed && !GetStableId().IsNone())
		{
			Sub->NotifyDestroyed(this);
		}
		Sub->Unregister(this);
	}
	Super::EndPlay(Reason);
}
