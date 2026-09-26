// A conversation as data (DA_Dialogue_*): entries with conditions, nodes with lines, voice, choices and effects.
// Mirrors MurdarDialogue::FDialogueSpec (DialogueRules.h); UDialogueSubsystem converts one into the other.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DialogueDefinition.generated.h"

class USoundBase;

USTRUCT(BlueprintType)
struct FDialogueValueCheck
{
	GENERATED_BODY()

	/** A narrative value: Stat.Money, Stat.Stress... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (Categories = "Stat"))
	FGameplayTag Value;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition") float Min = -1000000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition") float Max = 1000000.f;
};

USTRUCT(BlueprintType)
struct FDialogueCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition") FGameplayTagContainer RequiredFacts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition") FGameplayTagContainer ForbiddenFacts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition") TArray<FDialogueValueCheck> Values;
};

UENUM(BlueprintType)
enum class EDialogueEffect : uint8
{
	SetFact,
	ClearFact,
	/** Tag = the value, Amount = the delta (money: Stat.Money, negative to pay). */
	AddValue,
	/** Publish Tag on the bus (Source = the speaker). Missions and other systems listen. */
	Publish,
	/** The speaker is a police unit: offer it Amount (it may refuse at an arrest). Money is taken by the economy on
	 *  Event.Police.Bribed, i.e. only when the money is actually accepted. */
	PayPolice,
};

USTRUCT(BlueprintType)
struct FDialogueEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect") EDialogueEffect Kind = EDialogueEffect::SetFact;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect") FGameplayTag Tag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect") float Amount = 0.f;
};

USTRUCT(BlueprintType)
struct FDialogueChoice
{
	GENERATED_BODY()

	/** Short, what he says or does: „Dă-i 200 de lei”, „Taci”. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Choice") FText Text;
	/** Hidden (not greyed out) when it fails: e.g. not enough money. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Choice") FDialogueCondition Condition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Choice") TArray<FDialogueEffect> Effects;
	/** Node to go to; None = the conversation ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Choice") FName Next;
};

USTRUCT(BlueprintType)
struct FDialogueNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node") FName Id;
	/** Shown before the line: „Gică: ...”. Empty = no name (a voice from a car, the player himself). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node") FText Speaker;
	/** Empty = no line; the node only offers choices or applies effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (MultiLine = true)) FText Text;
	/** Played at the speaker. Its length sets the line's time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node") TSoftObjectPtr<USoundBase> Voice;
	/** 0 = voice length, or reading time from the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (Units = "s", ClampMin = "0")) float Seconds = 0.f;
	/** Fails = the node is skipped (straight to Next). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node") FDialogueCondition Condition;
	/** Applied when the node starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node") TArray<FDialogueEffect> Effects;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node") TArray<FDialogueChoice> Choices;
	/** Choices left alone this long take DefaultChoice. 0 = wait forever. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (Units = "s", ClampMin = "0")) float ChoiceTimeout = 0.f;
	/** Index into Choices taken on timeout; -1 = the conversation just ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (ClampMin = "-1")) int32 DefaultChoice = -1;
	/** After the line when there are no choices; None = end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node") FName Next;
};

USTRUCT(BlueprintType)
struct FDialogueEntry
{
	GENERATED_BODY()

	/** The most specific passing entry wins (most conditions); a tie goes to the first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry") FDialogueCondition Condition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry") FName Node;
};

UCLASS(BlueprintType)
class MURDAR_GAMEDEV_API UDialogueDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Dialogue"), GetFName()); }

	/** Published as the payload of Event.Dialogue.Started / Ended. Dialogue.* */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (Categories = "Dialogue"))
	FGameplayTag DialogueTag;

	/** Starts on this bus event (Event.Interact, Event.Police.Demand, Event.Trigger...). The event's Source is the speaker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	FGameplayTag StartOnEvent;

	/** ...with this payload (Interact.Kiosk.Gica, Trigger.X). None = any payload. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	FGameplayTag StartOnPayload;

	/** Takes over from a running conversation (police beats small talk). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start")
	bool bInterruptsOthers = false;

	/** Walking (or driving) this far from the speaker ends it. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start", meta = (Units = "cm", ClampMin = "0"))
	float MaxDistanceCm = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDialogueEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDialogueNode> Nodes;
};
