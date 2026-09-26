// Hold a key: the view locks onto the interior mirror — narrow, framed, the engine quiet enough to hear the car behind.
// Release: back to the normal camera. Added to the player's car by URearviewSubsystem; bound to an input action by the
// car (README §Patches). No UI — the mirror *is* the interface.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MirrorViewComponent.generated.h"

class UCameraComponent;
class UAudioComponent;

UCLASS(ClassGroup = (Murdar))
class MURDAR_GAMEDEV_API UMirrorViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Murdar|Rearview")
	void SetMirrorView(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Murdar|Rearview")
	bool IsMirrorView() const { return bOn_; }

protected:
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void EnsureCamera();

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> MirrorCamera;

	/** The car's own cameras we switched off, to switch back on. */
	TArray<TWeakObjectPtr<UCameraComponent>> Hidden;
	TWeakObjectPtr<UAudioComponent> Engine;
	float EngineVolumeBefore = 1.f;
	bool bOn_ = false;
};
