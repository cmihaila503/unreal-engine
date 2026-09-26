// Project Settings > Game > Murdar Consequences. What death and arrest cost in the open city; natural recovery.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Director/Consequences/ConsequenceRules.h"
#include "ConsequenceSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Consequences"))
class MURDAR_GAMEDEV_API UConsequenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** APlayerStart actors with this tag (or PlayerStartTag) are hospitals. None in the map = back to the checkpoint. */
	UPROPERTY(EditAnywhere, config, Category = "Places") FName HospitalTag = TEXT("Hospital");
	UPROPERTY(EditAnywhere, config, Category = "Places") FName StationTag = TEXT("PoliceStation");

	// ---- Hospital ----
	UPROPERTY(EditAnywhere, config, Category = "Hospital", meta = (Units = "h", ClampMin = "0", ClampMax = "72")) float HospitalHours = 6.f;
	UPROPERTY(EditAnywhere, config, Category = "Hospital", meta = (ClampMin = "0", ClampMax = "1")) float HospitalFeeFraction = 0.1f;
	UPROPERTY(EditAnywhere, config, Category = "Hospital", meta = (ClampMin = "0")) float HospitalFeeMin = 100.f;
	UPROPERTY(EditAnywhere, config, Category = "Hospital", meta = (MultiLine = true))
	FText HospitalLine = NSLOCTEXT("MurdarConsequence", "Hospital", "Spitalul Municipal. Te-au cârpit. Nota: {Bill}.");

	// ---- Station ----
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (Units = "h", ClampMin = "0")) float CellHoursBase = 8.f;
	/** Per point of heat (0..100) at the moment of the arrest. */
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (Units = "h", ClampMin = "0")) float CellHoursPerHeat = 0.2f;
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (Units = "h", ClampMin = "1", ClampMax = "240")) float CellHoursMax = 72.f;
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (ClampMin = "0")) float FineBase = 100.f;
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (ClampMin = "0")) float FinePerHeat = 5.f;
	/** Each earlier arrest (Stat.Arrests, saved) adds this fraction to time and fine. They remember you. */
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (ClampMin = "0", ClampMax = "2")) float RepeatFactor = 0.25f;
	/** Firearms and their reserve ammo are taken at or above this heat. 0 = always, 101 = never. */
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (ClampMin = "0", ClampMax = "101")) float ConfiscateFromHeat = 0.f;
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (MultiLine = true))
	FText StationLine = NSLOCTEXT("MurdarConsequence", "Station", "Secția 5. Ai stat {Hours} ore la răcoare. Amenda: {Fine}.");
	UPROPERTY(EditAnywhere, config, Category = "Station", meta = (MultiLine = true))
	FText ConfiscatedLine = NSLOCTEXT("MurdarConsequence", "Confiscated", "Armele au rămas la ei.");

	// ---- Recovery ----
	UPROPERTY(EditAnywhere, config, Category = "Recovery", meta = (Units = "s", ClampMin = "0", ClampMax = "60")) float RegenDelaySeconds = 6.f;
	UPROPERTY(EditAnywhere, config, Category = "Recovery", meta = (ClampMin = "0", ClampMax = "50")) float RegenPerSecond = 2.f;
	/** Health comes back by itself only this far (fraction of max). The rest: a doctor (an economy item). */
	UPROPERTY(EditAnywhere, config, Category = "Recovery", meta = (ClampMin = "0", ClampMax = "1")) float RegenCapFraction = 0.6f;
	/** Buying one of these (Event.Economy.Bought with this payload) heals fully. */
	UPROPERTY(EditAnywhere, config, Category = "Recovery", meta = (Categories = "Event.Economy.Buy"))
	FGameplayTagContainer FullHealItems;

	MurdarConsequence::FTuning ToTuning() const
	{
		MurdarConsequence::FTuning T;
		T.HospitalHours = HospitalHours; T.HospitalFeeFraction = HospitalFeeFraction; T.HospitalFeeMin = HospitalFeeMin;
		T.CellHoursBase = CellHoursBase; T.CellHoursPerHeat = CellHoursPerHeat; T.CellHoursMax = CellHoursMax;
		T.FineBase = FineBase; T.FinePerHeat = FinePerHeat; T.RepeatFactor = RepeatFactor; T.ConfiscateFromHeat = ConfiscateFromHeat;
		return T;
	}

	MurdarConsequence::FRegen ToRegen() const
	{
		MurdarConsequence::FRegen R;
		R.DelaySeconds = RegenDelaySeconds; R.PerSecond = RegenPerSecond; R.CapFraction = RegenCapFraction;
		return R;
	}
};
