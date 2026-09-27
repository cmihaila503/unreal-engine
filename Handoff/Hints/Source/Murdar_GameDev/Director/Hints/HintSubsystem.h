// The quiet tutorial. Hints are data (Project Settings > Murdar Hints): a Hint.* tag (the fact that remembers it was
// said), the text, and the bus event that is its moment. MurdarHints::FScheduler (unit-tested) says each once, spaced,
// never while busy. Shown as a subtitle with a "»" so it reads as a note, not a voice. A player setting turns them off.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/Hints/HintRules.h"
#include "HintSubsystem.generated.h"

struct FGameEvent;

USTRUCT(BlueprintType)
struct FHintDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, config, Category = "Hint", meta = (Categories = "Hint")) FGameplayTag Hint;
	UPROPERTY(EditAnywhere, config, Category = "Hint") FText Text;
	UPROPERTY(EditAnywhere, config, Category = "Hint") FGameplayTag OnEvent;
	UPROPERTY(EditAnywhere, config, Category = "Hint") FGameplayTag OnPayload;
	UPROPERTY(EditAnywhere, config, Category = "Hint") FGameplayTagContainer RequiredFacts;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Hints"))
class MURDAR_GAMEDEV_API UHintSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	UPROPERTY(EditAnywhere, config, Category = "Hints") TArray<FHintDefinition> Hints;
	UPROPERTY(EditAnywhere, config, Category = "Hints", meta = (Units = "s")) float MinGapSeconds = 25.f;
	UPROPERTY(EditAnywhere, config, Category = "Hints", meta = (Units = "s")) float WaitForCalmSeconds = 20.f;
	UPROPERTY(EditAnywhere, config, Category = "Hints", meta = (Units = "s")) float ShowSeconds = 5.f;
};

UCLASS()
class MURDAR_GAMEDEV_API UHintSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHintSubsystem* Get(const UObject* WorldContext);
	/** Story / systems: this hint's moment is now (in addition to its event). */
	void Offer(const FGameplayTag& Hint);
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnBusEvent(const FGameEvent& E);
	void Tick1Hz();
	bool IsBusy() const;
	bool Shown(const FGameplayTag& Hint) const;

	MurdarHints::FScheduler Scheduler;
	int32 Dialogues = 0;
	bool bCutscene = false;
	int32 BusHandle = 0;
	FTimerHandle TickTimer;
};
