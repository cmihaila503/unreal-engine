// Gangs in the open city. 1 Hz: which gang's streets he is in (zone facts), their stance toward him (respect,
// MurdarGangs::StanceOf with hysteresis), a few of their men around him (hostile ones come for him), the taxa asked
// on entering while Wary and unpaid (their dialogue), respect moved by deeds (killing their men, paying, their jobs)
// and fading a little each game day. Respect and the paid-until day are saved values.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Gangs/GangRules.h"
#include "GangSubsystem.generated.h"

class APawn;
struct FGameEvent;
struct FGangDefinition;

UCLASS()
class MURDAR_GAMEDEV_API UGangSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGangSubsystem* Get(const UObject* WorldContext);

	float GetRespect(int32 GangIndex) const;
	void AddDeed(int32 GangIndex, MurdarGangs::EDeed Deed);
	MurdarGangs::EStance GetStance(int32 GangIndex) const { return Stances.IsValidIndex(GangIndex) ? Stances[GangIndex] : MurdarGangs::EStance::Wary; }
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Tick1Hz();
	void OnBusEvent(const FGameEvent& E);
	int32 TerritoryHere() const;
	int32 GangOf(const AActor* Actor) const;
	void KeepMembers(int32 GangIndex);
	APawn* SpawnMember(int32 GangIndex, bool bHostile);
	void TurnHostile(int32 GangIndex);
	void AskForTax(int32 GangIndex);
	float Day() const;

	TArray<MurdarGangs::EStance> Stances;
	TArray<TArray<TWeakObjectPtr<APawn>>> Members;
	int32 CurrentTerritory = -1;
	int32 BusHandle = 0;
	FTimerHandle TickTimer;
};
