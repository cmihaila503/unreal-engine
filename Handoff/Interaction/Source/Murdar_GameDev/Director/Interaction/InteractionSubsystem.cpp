#include "Director/Interaction/InteractionSubsystem.h"

#include "Director/Interaction/InteractableComponent.h"
#include "Director/Interaction/InteractionRules.h"
#include "Director/Interaction/InteractionSettings.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UInteractionSubsystem* UInteractionSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UInteractionSubsystem>() : nullptr;
}

bool UInteractionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UInteractionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const float Hz = GetDefault<UInteractionSettings>()->ScanHz;
	InWorld.GetTimerManager().SetTimer(ScanTimer, this, &UInteractionSubsystem::Scan, 1.f / FMath::Max(Hz, 1.f), true);
}

void UInteractionSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	Registered.Reset();
	Focused.Reset();
	Super::Deinitialize();
}

void UInteractionSubsystem::Register(UInteractableComponent* Interactable)
{
	if (Interactable) { Registered.AddUnique(Interactable); }
}

void UInteractionSubsystem::Unregister(UInteractableComponent* Interactable)
{
	Registered.RemoveSingleSwap(Interactable);
	if (Focused.Get() == Interactable) { Focused.Reset(); }
}

void UInteractionSubsystem::Scan()
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	// On foot only: a car is not an ACharacter, so driving clears the focus.
	const ACharacter* Player = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Player || IsBlocked())
	{
		Focused.Reset();
		return;
	}

	const UInteractionSettings* S = GetDefault<UInteractionSettings>();
	const FVector Feet = Player->GetActorLocation();
	const FVector Forward = S->bUseCameraFacing ? PC->GetControlRotation().Vector() : Player->GetActorForwardVector();
	const float BroadSq = FMath::Square(S->BroadRangeCm);

	std::vector<MurdarInteract::FCandidate> Candidates;
	TArray<UInteractableComponent*, TInlineAllocator<16>> Components;
	int CurrentId = -1;

	Registered.RemoveAllSwap([](const TWeakObjectPtr<UInteractableComponent>& W) { return !W.IsValid(); });
	for (const TWeakObjectPtr<UInteractableComponent>& Weak : Registered)
	{
		UInteractableComponent* I = Weak.Get();
		const FVector Point = I->GetInteractionPoint();
		const FVector To = Point - Feet;
		if (To.SizeSquared() > BroadSq) { continue; }

		MurdarInteract::FCandidate C;
		C.Id = Components.Num();
		// Height is ignored for distance up to a step or a counter: a phone on a wall at chest height is "right here".
		C.DistanceCm = FVector(To.X, To.Y, FMath::Max(0.f, FMath::Abs(To.Z) - 90.f)).Size();
		C.AngleDeg = MurdarInteract::FacingAngleDeg(Forward.X, Forward.Y, To.X, To.Y);
		C.RangeCm = I->RangeCm;
		C.MaxAngleDeg = I->MaxAngleDeg;
		C.Priority = I->Priority;
		C.bAvailable = I->IsAvailable();
		if (I == Focused.Get()) { CurrentId = C.Id; }
		Candidates.push_back(C);
		Components.Add(I);
	}

	// Best first; a blocked winner is taken out and the pick runs again (bounded number of traces).
	const FVector Eyes = Player->GetPawnViewLocation();
	for (int32 Attempt = 0; Attempt < S->MaxTracesPerScan; ++Attempt)
	{
		const int Best = MurdarInteract::PickBest(Candidates, CurrentId, S->Stickiness);
		if (Best < 0)
		{
			Focused.Reset();
			return;
		}
		UInteractableComponent* Winner = Components[Candidates[Best].Id];
		if (!S->bRequireLineOfSight || HasLineOfSight(Player, Eyes, Winner))
		{
			Focused = Winner;
			return;
		}
		Candidates[Best].bAvailable = false;
	}
	Focused.Reset();
}

bool UInteractionSubsystem::HasLineOfSight(const APawn* Pawn, const FVector& EyeLocation, const UInteractableComponent* Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MurdarInteractSight), false, Pawn);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, Target->GetInteractionPoint(), GetDefault<UInteractionSettings>()->SightChannel, Params))
	{
		return true;
	}
	// Hitting the thing itself (its mesh) counts as seeing it.
	return Hit.GetActor() == Target->GetOwner();
}

bool UInteractionSubsystem::InteractFocused(APawn* User)
{
	UInteractableComponent* I = Focused.Get();
	if (!I || IsBlocked()) { return false; }
	const bool bUsed = I->Use(User);
	if (!I->IsAvailable()) { Focused.Reset(); } // once-only or a fact it set just took it out of play
	return bUsed;
}

FString UInteractionSubsystem::Describe() const
{
	FString Out = FString::Printf(TEXT("Interaction: %d registered, blocked %d, focused: %s\n"), Registered.Num(), BlockCount,
		Focused.IsValid() ? *GetNameSafe(Focused->GetOwner()) : TEXT("-"));
	for (const TWeakObjectPtr<UInteractableComponent>& W : Registered)
	{
		if (const UInteractableComponent* I = W.Get())
		{
			Out += FString::Printf(TEXT("  %-28s %-24s %s %s\n"), *GetNameSafe(I->GetOwner()), *I->Verb.ToString(),
				*I->InteractionTag.ToString(), I->IsAvailable() ? TEXT("") : TEXT("[unavailable]"));
		}
	}
	return Out;
}
