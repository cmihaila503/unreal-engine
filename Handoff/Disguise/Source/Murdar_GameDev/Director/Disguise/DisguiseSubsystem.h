// Disguise. The police describe him on foot by outfit and headwear (MurdarDisguise). Changing either out of their
// sight: at a distance they no longer know him (UFactionMemorySubsystem::ReportSighting asks IsRecognisable — README
// §Patches), and a chase lost that way cools to a stop. Outfits change at the safehouse wardrobe
// (Event.Player.OutfitChanged); headwear toggles with a key through what he owns (bought: economy items). The look is
// put on the character (mesh / hat) from the settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Director/Disguise/DisguiseRules.h"
#include "DisguiseSubsystem.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class APawn;
struct FGameEvent;

USTRUCT(BlueprintType)
struct FOutfitLook
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, config, Category = "Look") FText Name;
	/** ADAPT: GASP characters may use a modular mesh set; then list the parts to swap instead. */
	UPROPERTY(EditAnywhere, config, Category = "Look") TSoftObjectPtr<USkeletalMesh> Mesh;
};

USTRUCT(BlueprintType)
struct FHeadwearLook
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, config, Category = "Look") FText Name;
	UPROPERTY(EditAnywhere, config, Category = "Look") TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, config, Category = "Look") FName Socket = TEXT("head");
	/** Buying this economy item gives it (Event.Economy.Bought payload). */
	UPROPERTY(EditAnywhere, config, Category = "Look", meta = (Categories = "Event.Economy.Buy")) FGameplayTag ShopItem;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Disguise"))
class MURDAR_GAMEDEV_API UDisguiseSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	UPROPERTY(EditAnywhere, config, Category = "Looks") TArray<FOutfitLook> Outfits;
	UPROPERTY(EditAnywhere, config, Category = "Looks") TArray<FHeadwearLook> Headwear;
	/** ADAPT: a key free on foot. */
	UPROPERTY(EditAnywhere, config, Category = "Input") TArray<FKey> HeadwearKeys = { EKeys::H, EKeys::Gamepad_DPad_Up };
	UPROPERTY(EditAnywhere, config, Category = "Recognition", meta = (Units = "cm")) float CloseDay = 800.f;
	UPROPERTY(EditAnywhere, config, Category = "Recognition", meta = (Units = "cm")) float CloseNight = 300.f;
};

UCLASS()
class MURDAR_GAMEDEV_API UDisguiseSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDisguiseSubsystem* Get(const UObject* WorldContext);

	/** From ReportSighting (on foot): would this witness know him? */
	bool IsRecognisable(const AActor* Witness, const APawn* Player) const;
	/** From ReportSighting after a recognised on-foot sighting: the description is what he wears now. */
	void NoteSighting();
	MurdarDisguise::FLookOfHim GetLook() const;
	void ToggleHeadwear();
	FString Describe() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDisguiseSubsystem, STATGROUP_Tickables); }

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnBusEvent(const FGameEvent& E);
	void Changed();
	void ApplyLook();
	MurdarDisguise::FTuning Tuning() const;

	MurdarDisguise::FDescription Description;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Hat;
	int32 BusHandle = 0;
};
