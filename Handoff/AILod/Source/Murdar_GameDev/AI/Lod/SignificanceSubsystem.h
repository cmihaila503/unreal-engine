// AI LOD: every 0.25 s, rank the AI pawns (MurdarLod, unit-tested), keep a tier per pawn with hysteresis, and set
// tick intervals only when a tier changes: pedestrian brain, character movement, anim only when rendered, car
// signals, and — far and out of view only — the traffic driver. Engaged units (police past Patrol, NPCs with a target)
// and actors tagged "Mission" are always full rate. Also grows the World Partition streaming radius with the player's
// speed when the car carries a streaming source (README §Streaming).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Lod/SignificanceRules.h"
#include "SignificanceSubsystem.generated.h"

class APawn;

UCLASS()
class MURDAR_GAMEDEV_API USignificanceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static USignificanceSubsystem* Get(const UObject* WorldContext);

	/** Cheat / profiling: off = everyone back to full rate. */
	void SetEnabled(bool bOn);
	bool IsEnabled() const { return bEnabled; }
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Pass();
	void ApplyTier(APawn* Pawn, MurdarLod::ETier Tier) const;
	bool IsEngaged(const APawn* Pawn) const;
	void UpdateStreaming(const APawn* Player);

	TMap<TWeakObjectPtr<APawn>, MurdarLod::FTierState> States;
	FTimerHandle PassTimer;
	bool bEnabled = true;
	float StreamingRadius = 0.f;
	int32 Counts[4] = { 0, 0, 0, 0 };
};
