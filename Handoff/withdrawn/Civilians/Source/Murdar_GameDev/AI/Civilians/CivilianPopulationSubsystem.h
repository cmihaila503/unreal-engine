// Who is on the street. Keeps a population of civilians around the player at the density of the zone he is in,
// spawning and despawning only where he can't see (spec §4), reusing bodies from a pool. It decides who exists
// and where they start; it never steers anyone — the foot brain (AMurdarNPCAIController) does that.
// 2 Hz timer, nothing per frame (PROJECT_OVERVIEW §5).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CivilianPopulationSubsystem.generated.h"

class UCivilianProfile;
class UCivilianPopulationSettings;
struct FCivilianArchetypeWeight;

USTRUCT(BlueprintType)
struct FCivilianPopulationStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 Active = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 Pooled = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 Target = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 Pinned = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 SpawnedNew = 0;       // cumulative
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 ReusedFromPool = 0;   // cumulative
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 Despawned = 0;        // cumulative
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 RejectedVisible = 0;  // cumulative
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 RejectedNav = 0;      // cumulative
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 RejectedCrowded = 0;  // cumulative
	UPROPERTY(BlueprintReadOnly, Category = "Civilian") int32 SpawnFailed = 0;      // cumulative (SpawnNPC returned null)
};

UCLASS()
class MURDAR_GAMEDEV_API UCivilianPopulationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Multiplies every zone's density (cheat MurdarCivDensity, a future time-of-day curve). */
	UFUNCTION(BlueprintCallable, Category = "Civilian")
	void SetDensityScale(float Scale);

	UFUNCTION(BlueprintPure, Category = "Civilian")
	float GetDensityScale() const { return DensityScale; }

	/** Replaces the settings' MaxCivilians until cleared with a negative value (cheat MurdarCivMax N). */
	UFUNCTION(BlueprintCallable, Category = "Civilian")
	void SetMaxOverride(int32 Max);

	UFUNCTION(BlueprintPure, Category = "Civilian")
	FCivilianPopulationStats GetStats() const { return Stats; }

	UFUNCTION(BlueprintCallable, Category = "Civilian")
	TArray<APawn*> GetActiveCivilians() const;

	/** A pinned civilian is never despawned: an unreported witness, someone fleeing, knocked down, talking to the
	 *  player (spec §4). The brain pins and unpins; the population only obeys. */
	UFUNCTION(BlueprintCallable, Category = "Civilian")
	void SetPinned(APawn* Civilian, bool bPinned);

	/** The brain calls this when its civilian dies. The body stays a corpse under the existing CorpseSeconds rule;
	 *  it just stops counting against the population and is never pooled. */
	UFUNCTION(BlueprintCallable, Category = "Civilian")
	void NotifyCivilianDied(APawn* Civilian);

	UFUNCTION(BlueprintPure, Category = "Civilian")
	UCivilianProfile* GetProfileOf(const APawn* Civilian) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FSlot
	{
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<UCivilianProfile> Profile;
		double LastSeenTime = 0.0;
		bool bPinned = false;
	};

	struct FResolvedArchetype
	{
		UCivilianProfile* Profile = nullptr; // kept alive by LoadedProfiles
		float Weight = 0.f;
	};

	struct FViewer
	{
		FVector PawnLocation = FVector::ZeroVector;
		FVector SpawnCentre = FVector::ZeroVector;   // PawnLocation + look-ahead along his velocity
		float CosViewCone = 0.f;                     // frustum half-diagonal + margin, as a cosine
		float PixelsPerUnitAtUnitDistance = 0.f;     // viewport height / (2 tan(vertical half FOV))
		FVector ViewLocation = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
		const AActor* IgnoreActor = nullptr;
	};

	void Update();
	bool GetViewer(FViewer& Out) const;
	bool IsVisible(const FViewer& Viewer, const FVector& FeetLocation, const AActor* Candidate) const;

	void DespawnPass(const FViewer& Viewer, double Now);
	void SpawnPass(const FViewer& Viewer, double Now);

	float DespawnDistance() const;
	int32 ComputeTarget(const FVector& PlayerLocation, const TArray<FResolvedArchetype>*& OutArchetypes) const;
	bool FindSpawnPoint(const FViewer& Viewer, FVector& OutLocation);
	UCivilianProfile* PickProfile(const TArray<FResolvedArchetype>& Archetypes);

	APawn* AcquirePawn(const FVector& Location, const FRotator& Rotation, UCivilianProfile* Profile);
	void SetBodyActive(APawn* Pawn, bool bActive) const;
	void ReturnToPool(APawn* Pawn);

	void ResolveArchetypes();
	void DebugDraw(const FViewer& Viewer) const;

	FSlot* FindSlot(const APawn* Pawn);
	const FSlot* FindSlot(const APawn* Pawn) const;

	TArray<FSlot> Active;
	TArray<TWeakObjectPtr<APawn>> Pool;

	TArray<FResolvedArchetype> DefaultArchetypes;
	TArray<TArray<FResolvedArchetype>> ZoneArchetypes; // parallel to settings ZoneDensities

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCivilianProfile>> LoadedProfiles;

	FTimerHandle UpdateTimer;
	FRandomStream Rng;
	float DensityScale = 1.f;
	int32 MaxOverride = -1;
	FCivilianPopulationStats Stats;
};
