// Plays cutscenes: started by bus events (Chapter triggers, Event.Mission.*, Event.Interact...), judged by
// MurdarCutscene::Decide (never mid-chase; once-only by fact), one at a time with a short queue. While one plays: the
// player's input is off, interaction and the HUD are hidden, a dialogue is aborted, hold the skip key ~1 s to skip.
// Facts on end; Event.Cutscene.Started / Ended on the bus.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/Cutscenes/CutsceneRules.h"
#include "CutsceneSubsystem.generated.h"

class UCutsceneDefinition;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class SWidget;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UCutsceneSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCutsceneSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Murdar|Cutscene") bool Play(UCutsceneDefinition* Cutscene);
	UFUNCTION(BlueprintPure, Category = "Murdar|Cutscene") bool IsPlaying() const { return Active.IsValid(); }
	void Skip();
	UCutsceneDefinition* FindByName(const FString& Name) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCutsceneSubsystem, STATGROUP_Tickables); }

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnBusEvent(const FGameEvent& E);
	MurdarCutscene::FGate GateFor(const UCutsceneDefinition* C) const;
	UFUNCTION() void OnFinished();
	void Begin();
	void End();

	UPROPERTY(Transient) TArray<TObjectPtr<UCutsceneDefinition>> All;
	TWeakObjectPtr<UCutsceneDefinition> Active;
	UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> Player;
	UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
	MurdarCutscene::FQueue Queue;
	MurdarCutscene::FSkipHold SkipHold;
	TSharedPtr<SWidget> Letterbox;
	int32 Dialogues = 0;
	int32 BusHandle = 0;
	bool bInHandler = false;
};
