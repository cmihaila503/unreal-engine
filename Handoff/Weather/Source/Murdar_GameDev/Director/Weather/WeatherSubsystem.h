// The weather. Each game hour (Handoff/TimeOfDay) the Markov chain picks the next state; the look blends in over
// ~90 s; roads get wet and dry; wet asphalt grips less (by scaling the road physical materials' Friction — grip stays
// authored on the physical material, one authority, see VehicleSurfaceResponse.h — and restoring it at the end);
// fewer people go out in the rain; fog thickens the level's height fog; rain falls around the camera. Saved as values.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/Weather/WeatherRules.h"
#include "WeatherSubsystem.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UPhysicalMaterial;
class AExponentialHeightFog;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UWeatherSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWeatherSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintPure, Category = "Murdar|Weather") float GetPeopleScale() const;
	/** Extra darkness 0..1 (storm at noon): headlights and the rearview darkness add it. */
	UFUNCTION(BlueprintPure, Category = "Murdar|Weather") float GetGloom() const { return MurdarWeather::Gloom(Look); }
	UFUNCTION(BlueprintPure, Category = "Murdar|Weather") float GetWetness() const { return Wetness; }
	UFUNCTION(BlueprintPure, Category = "Murdar|Weather") float GetRain() const { return Look.Rain; }

	/** Cheat / story: force a state (bInstant = no blend). */
	void SetState(MurdarWeather::EState NewState, bool bInstant);
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Tick1Hz();
	void Apply();
	void Restore();
	void SaveToState() const;
	void LoadFromState();
	float CurrentHour() const;

	MurdarWeather::EState State = MurdarWeather::EState::Clear;
	MurdarWeather::FLook Look = MurdarWeather::LookOf(MurdarWeather::EState::Clear);
	float Wetness = 0.f;
	int32 LastHour = -1;

	TMap<TWeakObjectPtr<UPhysicalMaterial>, float> OriginalFriction;
	TWeakObjectPtr<AExponentialHeightFog> Fog;
	float FogBaseDensity = -1.f;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Rain;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RainAudio;
	FTimerHandle TickTimer;
	int32 BusHandle = 0;
};
