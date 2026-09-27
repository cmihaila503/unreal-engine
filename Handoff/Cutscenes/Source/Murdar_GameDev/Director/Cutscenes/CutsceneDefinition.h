// A cutscene as data (DA_Cutscene_*): a Level Sequence, when it plays, and what it leaves behind.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CutsceneDefinition.generated.h"

class ULevelSequence;

UCLASS(BlueprintType)
class MURDAR_GAMEDEV_API UCutsceneDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Cutscene"), GetFName()); }

	/** Set as a fact when it has played (so bOnce holds across saves). Cutscene.* */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutscene", meta = (Categories = "Cutscene")) FGameplayTag CutsceneTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutscene") TSoftObjectPtr<ULevelSequence> Sequence;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start") FGameplayTag StartOnEvent;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start") FGameplayTag StartOnPayload;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start") FGameplayTagContainer RequiredFacts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start") FGameplayTagContainer ForbiddenFacts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start") bool bOnce = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback") bool bSkippable = true;
	/** Black bars top and bottom (2.39:1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback") bool bLetterbox = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") FGameplayTagContainer FactsOnEnd;
};
