// Runs conversations and voices barks. One conversation at a time, started by a bus event (Event.Interact on a
// person, Event.Police.Demand at a traffic stop, a chapter trigger), judged by MurdarDialogue::FRuntime (unit-tested).
// Lines go to the HUD's subtitle queue and play their voice at the speaker; choices go into the HUD prompt line and
// are picked with 1-4 / the d-pad; effects go through the narrative state, the bus and the police controller.
// Barks: Event.Police.Line (and Event.Dialogue.Bark) with a voice in the settings are played at their source.
// A conversation is not saved; what it changed (facts, values) is.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/Dialogue/DialogueRules.h"
#include "DialogueSubsystem.generated.h"

class UDialogueDefinition;
class USoundBase;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UDialogueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDialogueSubsystem* Get(const UObject* WorldContext);

	/** Start this conversation with this speaker (null = no one in the world: a phone, a thought). */
	UFUNCTION(BlueprintCallable, Category = "Murdar|Dialogue")
	bool StartDialogue(UDialogueDefinition* Dialogue, AActor* Speaker);

	UFUNCTION(BlueprintCallable, Category = "Murdar|Dialogue")
	void AbortDialogue();

	UFUNCTION(BlueprintPure, Category = "Murdar|Dialogue") bool IsTalking() const { return Runtime.IsValid() && Runtime->IsRunning(); }
	UFUNCTION(BlueprintPure, Category = "Murdar|Dialogue") bool IsChoosing() const;
	/** "1 Dă-i 200   2 Taci" while choosing, empty otherwise. The HUD shows it in the prompt line. */
	UFUNCTION(BlueprintPure, Category = "Murdar|Dialogue") FText GetChoicesPrompt() const { return ChoicesPrompt; }
	UFUNCTION(BlueprintCallable, Category = "Murdar|Dialogue") bool Choose(int32 VisibleIndex);

	UDialogueDefinition* FindByName(const FString& Name) const;
	FString Describe() const;

	// UTickableWorldSubsystem: per frame only for key presses and line timing; returns at once when idle.
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDialogueSubsystem, STATGROUP_Tickables); }

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void LoadDialogues();
	void OnBusEvent(const FGameEvent& Event);
	void Bark(const FGameEvent& Event);
	MurdarDialogue::FQuery MakeQuery() const;
	void BuildSpec(const UDialogueDefinition* D, MurdarDialogue::FDialogueSpec& Out) const;
	void Apply(const std::vector<MurdarDialogue::FStep>& Steps);
	void ApplyEffect(const MurdarDialogue::FEffect& Effect);
	void PlayVoice(USoundBase* Sound, const AActor* At, const FVector& Fallback) const;
	void PublishDialogue(const TCHAR* TagName) const;
	float Now() const;

	UPROPERTY(Transient) TArray<TObjectPtr<UDialogueDefinition>> Dialogues;
	TWeakObjectPtr<UDialogueDefinition> Active;
	TWeakObjectPtr<AActor> Speaker;
	bool bHadSpeaker = false;
	TUniquePtr<MurdarDialogue::FDialogueSpec> Spec;
	TUniquePtr<MurdarDialogue::FRuntime> Runtime;
	/** Node id -> voice, for the active conversation (lines come back as text; the voice is looked up by node). */
	TMap<FString, TSoftObjectPtr<USoundBase>> NodeVoices;
	FText ChoicesPrompt;
	int32 NumChoices = 0;
	MurdarDialogue::FBarkPicker Barks;
	int32 BusHandle = 0;
	bool bInHandler = false;
	bool bBlockingInteraction = false;
};
