// Project Settings > Game > Murdar Garages.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Vehicle/Garage/GarageRules.h"
#include "GarageSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Garages"))
class MURDAR_GAMEDEV_API UGarageSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** The colours cars come in (and get resprayed to): a 90s Romanian street. */
	UPROPERTY(EditAnywhere, config, Category = "Paint")
	TArray<FLinearColor> Palette = {
		FLinearColor(0.88f, 0.88f, 0.85f), // alb
		FLinearColor(0.55f, 0.06f, 0.05f), // roșu
		FLinearColor(0.78f, 0.70f, 0.52f), // bej
		FLinearColor(0.12f, 0.30f, 0.16f), // verde
		FLinearColor(0.10f, 0.18f, 0.45f), // albastru
		FLinearColor(0.85f, 0.65f, 0.10f), // galben
		FLinearColor(0.35f, 0.36f, 0.38f), // gri
		FLinearColor(0.05f, 0.05f, 0.06f), // negru
		FLinearColor(0.45f, 0.25f, 0.12f), // maro
	};

	/** Two colours a witness calls the same are closer than this (MurdarGarage::ColorDistance). */
	UPROPERTY(EditAnywhere, config, Category = "Paint", meta = (ClampMin = "0.05", ClampMax = "1")) float SameColourDistance = 0.35f;

	UPROPERTY(EditAnywhere, config, Category = "Prices", meta = (ClampMin = "0")) float Respray = 150.f;
	UPROPERTY(EditAnywhere, config, Category = "Prices", meta = (ClampMin = "0")) float RepairFull = 400.f;
	UPROPERTY(EditAnywhere, config, Category = "Prices", meta = (ClampMin = "1", ClampMax = "5")) float HotMultiplier = 2.f;
	UPROPERTY(EditAnywhere, config, Category = "Prices", meta = (ClampMin = "0")) float ImpoundBase = 200.f;
	UPROPERTY(EditAnywhere, config, Category = "Prices", meta = (ClampMin = "0")) float ImpoundPerDay = 50.f;

	/** Seconds stopped in the bay before the job starts, and how long the job takes (screen black). */
	UPROPERTY(EditAnywhere, config, Category = "Respray", meta = (Units = "s", ClampMin = "0.2", ClampMax = "5")) float SettleSeconds = 1.f;
	UPROPERTY(EditAnywhere, config, Category = "Respray", meta = (Units = "s", ClampMin = "0.5", ClampMax = "10")) float JobSeconds = 2.f;

	UPROPERTY(EditAnywhere, config, Category = "Lines") FText ResprayDone = NSLOCTEXT("MurdarGarage", "Done", "Gata, șefu'. N-o mai recunoaște nici mă-sa.");
	UPROPERTY(EditAnywhere, config, Category = "Lines") FText ResprayNoMoney = NSLOCTEXT("MurdarGarage", "NoMoney", "Fără bani nu vopsim nimic.");
	UPROPERTY(EditAnywhere, config, Category = "Lines") FText RespraySeen = NSLOCTEXT("MurdarGarage", "Seen", "Cu gaborii în coadă? Pleacă de-aici!");
	UPROPERTY(EditAnywhere, config, Category = "Lines") FText GarageFull = NSLOCTEXT("MurdarGarage", "Full", "Garajul e plin.");

	MurdarGarage::FPrices ToPrices() const
	{
		MurdarGarage::FPrices P;
		P.Respray = Respray; P.RepairFull = RepairFull; P.HotMultiplier = HotMultiplier; P.ImpoundBase = ImpoundBase; P.ImpoundPerDay = ImpoundPerDay;
		return P;
	}

	static MurdarGarage::FColor3 ToC3(const FLinearColor& C) { return { C.R, C.G, C.B }; }
};
