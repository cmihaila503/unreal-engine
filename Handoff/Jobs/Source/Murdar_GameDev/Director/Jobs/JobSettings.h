// Project Settings > Game > Murdar Jobs. Contacts, pay, pager timing, the cars people want.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Director/Jobs/JobRules.h"
#include "JobSettings.generated.h"

class UVehicleDefinition;
class USoundBase;

UENUM(BlueprintType)
enum class EJobKind : uint8 { Delivery, Smuggling, CarDelivery, Collection };

USTRUCT(BlueprintType)
struct FJobContact
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = "Contact") FText Name = NSLOCTEXT("MurdarJobs", "Vali", "Vali");
	/** The page: „Sună-l pe Vali.” */
	UPROPERTY(EditAnywhere, config, Category = "Contact") FText Page = NSLOCTEXT("MurdarJobs", "PageVali", "Pager: Sună-l pe Vali. Urgent.");
	UPROPERTY(EditAnywhere, config, Category = "Contact") TArray<EJobKind> Kinds = { EJobKind::Delivery };
	UPROPERTY(EditAnywhere, config, Category = "Contact", meta = (ClampMin = "0.1", ClampMax = "5")) float PayMultiplier = 1.f;
	/** Only once this fact is set (met him in the story). None = from the start. */
	UPROPERTY(EditAnywhere, config, Category = "Contact", meta = (Categories = "Fact")) FGameplayTag RequiredFact;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Jobs"))
class MURDAR_GAMEDEV_API UJobSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(EditAnywhere, config, Category = "Contacts") TArray<FJobContact> Contacts;

	/** Models people order (car delivery). */
	UPROPERTY(EditAnywhere, config, Category = "Car delivery") TArray<TSoftObjectPtr<UVehicleDefinition>> WantedCars;

	UPROPERTY(EditAnywhere, config, Category = "Pager", meta = (Units = "s")) FVector2D PageSeconds = FVector2D(240.f, 600.f);
	UPROPERTY(EditAnywhere, config, Category = "Pager", meta = (Units = "s")) float PageExpirySeconds = 300.f;
	UPROPERTY(EditAnywhere, config, Category = "Pager") TSoftObjectPtr<USoundBase> PagerBeep;

	UPROPERTY(EditAnywhere, config, Category = "Pay") float BasePay = 100.f;
	UPROPERTY(EditAnywhere, config, Category = "Pay") float PayPerKm = 120.f;
	UPROPERTY(EditAnywhere, config, Category = "Pay") float SmugglingMultiplier = 2.5f;
	UPROPERTY(EditAnywhere, config, Category = "Pay") float CarDeliveryBase = 600.f;
	UPROPERTY(EditAnywhere, config, Category = "Pay") float CollectionBase = 250.f;
	UPROPERTY(EditAnywhere, config, Category = "Collection", meta = (ClampMin = "0", ClampMax = "1")) float DebtorPaysChance = 0.6f;

	MurdarJobs::FTuning ToTuning() const
	{
		MurdarJobs::FTuning T;
		T.BasePay = BasePay; T.PayPerKm = PayPerKm; T.SmugglingMultiplier = SmugglingMultiplier;
		T.CarDeliveryBase = CarDeliveryBase; T.CollectionBase = CollectionBase; T.DebtorPaysChance = DebtorPaysChance;
		T.PageMinSeconds = PageSeconds.X; T.PageMaxSeconds = PageSeconds.Y; T.PageExpirySeconds = PageExpirySeconds;
		return T;
	}
};
