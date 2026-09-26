// Civilian witnesses' reports on their way to the police (Docs/HEAT_SYSTEM.md §3). A crime a civilian sees does not
// raise heat when it happens — it raises heat when the witness gets the word to the police: a patrol nearby (seconds),
// otherwise a phone booth or a station (tens of seconds). Until then the player can get away, or stop the witness.
// Police who see a crime themselves still report instantly (the faction memory's existing path).
//
// The rules live in MurdarHeat::FReportQueue (pure C++, unit-tested); this class holds the actors, the clock and the
// hand-off to UFactionMemorySubsystem. 2 Hz timer, nothing per frame.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "MurdarHeatModel.h"
#include "WitnessReportSubsystem.generated.h"

UCLASS()
class MURDAR_GAMEDEV_API UWitnessReportSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** A civilian saw a crime. Replaces the direct UFactionMemorySubsystem::ReportCrime call for civilian witnesses.
	 *  Where = where the crime happened (the witness's report will carry this, not the player's later position). */
	UFUNCTION(BlueprintCallable, Category = "Heat")
	void WitnessCrime(AActor* Witness, FGameplayTag Crime, FVector Where);

	/** Police saw it themselves: heat went in through the memory's direct path; civilian reports of the same
	 *  incident will only corroborate. Call next to the memory's police-sourced ReportCrime. */
	UFUNCTION(BlueprintCallable, Category = "Heat")
	void PoliceWitnessedCrime(FGameplayTag Crime, FVector Where);

	/** The witness reached a phone / a patrol / a station: its reports arrive now (foot AI, later). */
	UFUNCTION(BlueprintCallable, Category = "Heat")
	void WitnessReachedPolice(AActor* Witness);

	/** The witness won't talk: paid, threatened (future), or killed (handled automatically). */
	UFUNCTION(BlueprintCallable, Category = "Heat")
	void SilenceWitness(AActor* Witness);

	UFUNCTION(BlueprintPure, Category = "Heat")
	bool HasPendingReport(const AActor* Witness) const;

	UFUNCTION(BlueprintPure, Category = "Heat")
	int32 NumPendingReports() const { return int32(Queue ? Queue->NumPending() : 0); }

	/** Delivers every pending report now. Call at the start of a save, so saving can't erase a witness. */
	UFUNCTION(BlueprintCallable, Category = "Heat")
	void FlushForSave();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Update();
	void Deliver(const std::vector<MurdarHeat::FDelivery>& Deliveries);
	uint64 WitnessIdOf(AActor* Witness);
	int32 CrimeTypeOf(const FGameplayTag& Crime);
	bool IsPoliceNear(const FVector& Where) const;
	void SetWitnessPinned(AActor* Witness, bool bPinned) const;
	void EnsureQueue();

	TUniquePtr<MurdarHeat::FReportQueue> Queue;
	TMap<uint64, TWeakObjectPtr<AActor>> Witnesses;
	TMap<TWeakObjectPtr<AActor>, uint64> WitnessIds;
	uint64 LastWitnessId = 0;
	TMap<FGameplayTag, int32> CrimeTypes;
	TArray<FGameplayTag> CrimeTags; // index = crime type id - 1
	FTimerHandle UpdateTimer;
	FRandomStream Rng;
};
