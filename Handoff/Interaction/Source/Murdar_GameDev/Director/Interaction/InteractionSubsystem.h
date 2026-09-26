// Which interactable gets the "E" right now. A registry (components add themselves), an 8 Hz focus scan for the
// on-foot player, MurdarInteract::PickBest (unit-tested) with stickiness, one line-of-sight trace for the winner.
// The character asks InteractFocused(); the HUD asks GetFocused() for the prompt. Nothing runs while driving.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InteractionSubsystem.generated.h"

class UInteractableComponent;
class APawn;

UCLASS()
class MURDAR_GAMEDEV_API UInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UInteractionSubsystem* Get(const UObject* WorldContext);

	void Register(UInteractableComponent* Interactable);
	void Unregister(UInteractableComponent* Interactable);

	/** The thing the "E" would use now, or null. */
	UFUNCTION(BlueprintPure, Category = "Murdar|Interaction")
	UInteractableComponent* GetFocused() const { return Focused.Get(); }

	/** Use the focused thing. False when there is none (the caller then tries the next thing, e.g. a car). */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Interaction")
	bool InteractFocused(APawn* User);

	/** Dialogue, cutscenes, the arrest: no prompt, no use. Counted, so nested blocks are fine. */
	void PushBlock() { ++BlockCount; Focused.Reset(); }
	void PopBlock() { BlockCount = FMath::Max(0, BlockCount - 1); }
	bool IsBlocked() const { return BlockCount > 0; }

	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Scan();
	bool HasLineOfSight(const APawn* Pawn, const FVector& EyeLocation, const UInteractableComponent* Target) const;

	TArray<TWeakObjectPtr<UInteractableComponent>> Registered;
	TWeakObjectPtr<UInteractableComponent> Focused;
	FTimerHandle ScanTimer;
	int32 BlockCount = 0;
};
