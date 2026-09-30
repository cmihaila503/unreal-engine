// Makes a car feel as heavy in the air and against a wall as it does on the road. On every AMurdarVehicle (player,
// traffic, police - the same physics for all): extra gravity only while all wheels are off, pitch/roll damping and a
// gentle leveling in the air, a damped landing, a no-bounce body material and capped depenetration, and - for the
// player's car - an impact feel scaled by the delta-v (shake, a few frames of hit-stop, a low thud).
// Rules: MurdarWeight (unit-tested).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicle/Weight/CarWeightRules.h"
#include "CarWeightComponent.generated.h"

class AMurdarVehicle;
class UPrimitiveComponent;

UCLASS(ClassGroup = (Murdar), meta = (BlueprintSpawnableComponent))
class MURDAR_GAMEDEV_API UCarWeightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarWeightComponent();

	/** From UVehicleEffectsComponent::NotifyImpact (README patch 2): delta-v (cm/s) of the hit, where it was. */
	void OnImpact(float HorizontalCms, float VerticalCms, const FVector& Where);

	bool IsAirborne() const { return Air.bAirborne; }
	FString Describe() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	AMurdarVehicle* Vehicle() const;
	UPrimitiveComponent* Body() const;
	void ApplyBodyContact();
	int32 WheelsDown() const;
	void HitStop(float Seconds, float Dilation);

	MurdarWeight::FAirState Air;
	float LastHitStopTime = -100.f;
	bool bHitStopActive = false;
};
