// A mission, authored as data (DA_Mission_*, primary asset type "Mission"), in the project's own vocabulary: facts,
// zones (which are facts), bus events, checkpoints, values. No scripting: every mission is ordered objectives, fail
// conditions and an outcome. The rules are MurdarMission:: (MissionRules.h, unit-tested).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MissionDefinition.generated.h"

USTRUCT(BlueprintType)
struct FMissionObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName Id;

	/** Shown once as a subtitle when the objective starts ("Du Dacia la garajul din Ferentari."). Empty = silent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText Text;

	/** Completes when this bus event arrives while the objective is active (a parent tag matches its children). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Complete when", meta = (Categories = "Event"))
	FGameplayTag CompleteOnEvent;

	/** ...and its payload matches this (e.g. Event.Trigger with payload Trigger.Garage). Empty = any payload. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Complete when")
	FGameplayTag CompleteOnPayload;

	/** ...and all these facts hold. A zone is a fact while the player is in it (Zone.*). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Complete when")
	FGameplayTagContainer RequiredFacts;

	/** ...and the player is within ReachRadiusCm of the actor carrying this tag. None = no place needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Complete when")
	FName ReachActorTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Complete when", meta = (Units = "cm", ClampMin = "50", ClampMax = "20000"))
	float ReachRadiusCm = 500.f;

	/** Fails the mission if the objective takes longer. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (Units = "s", ClampMin = "0"))
	float TimeLimitSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FGameplayTagContainer FactsOnComplete;
};

UCLASS(BlueprintType)
class MURDAR_GAMEDEV_API UMissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Mission"), GetFName()); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText Title;

	/** The mission's identity. Set as a fact when it succeeds (that's how "done" is saved). Mission.* */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (Categories = "Mission"))
	FGameplayTag MissionTag;

	// ---- Start ----
	/** Starts when this bus event arrives (typically Event.Trigger from a chapter trigger, or Event.Dialogue.* later). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Start", meta = (Categories = "Event"))
	FGameplayTag StartOnEvent;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Start")
	FGameplayTag StartOnPayload;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Start")
	FGameplayTagContainer RequiredFacts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Start")
	FGameplayTagContainer ForbiddenFacts;
	/** Can be played again after success (a side job). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Start")
	bool bRepeatable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FMissionObjective> Objectives;

	// ---- Fail ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Fail", meta = (Categories = "Event"))
	FGameplayTagContainer FailOnEvents;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Fail")
	FGameplayTagContainer FailIfFacts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Fail")
	bool bFailOnPlayerDeath = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Fail")
	bool bFailOnArrest = true;
	/** 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Fail", meta = (Units = "s", ClampMin = "0"))
	float TimeLimitSeconds = 0.f;

	// ---- Outcome ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Outcome")
	FGameplayTagContainer FactsOnSuccess;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Outcome")
	FGameplayTagContainer FactsOnFail;
	/** Values added on success: Stat.Money (economy), Stat.* ... Honor is NOT reachable from here (FHonorToken). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Outcome")
	TMap<FGameplayTag, float> ValueRewards;
	/** Reached (with its autosave) on success. None = stay where you are. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Outcome")
	FName SuccessCheckpoint;
	/** Retry puts the player here and starts the mission again. None = retry in place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Outcome")
	FName RetryCheckpoint;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Outcome")
	FText SuccessText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Outcome")
	FText FailText;
};
