#include "Director/Music/MusicSubsystem.h"

#include "Director/Music/MusicSettings.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/TensionSubsystem.h"
#include "AI/FactionMemorySubsystem.h" // ADAPT path

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
	constexpr float TickSeconds = 0.1f;
	constexpr float StopAfterSilentSeconds = 30.f; // stems at zero this long are stopped (restarted together later)
	constexpr float MinAudible = 0.001f;           // never exactly 0 while playing: keeps stems from being culled
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
}

UMusicSubsystem* UMusicSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UMusicSubsystem>() : nullptr;
}

bool UMusicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

MurdarMusic::FTuning UMusicSubsystem::Tuning() const
{
	const UMusicSettings* S = GetDefault<UMusicSettings>();
	MurdarMusic::FTuning T;
	T.CombatShotWindow = S->CombatShotWindow; T.AftermathSeconds = S->AftermathSeconds; T.UneaseStress = S->UneaseStress;
	T.DownDelay = S->DownDelay; T.MinDwell = S->MinDwell; T.RiseRate = S->RiseRate; T.FallRate = S->FallRate;
	return T;
}

void UMusicSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UMusicSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
	InWorld.GetTimerManager().SetTimer(TickTimer, this, &UMusicSubsystem::Tick10Hz, TickSeconds, true);
}

void UMusicSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
	StopAll();
	Super::Deinitialize();
}

void UMusicSubsystem::OnBusEvent(const FGameEvent& E)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (E.Tag == MurdarTags::Event_Combat_Shot)
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player && FVector::DistSquared(Player->GetActorLocation(), E.Location) < FMath::Square(GetDefault<UMusicSettings>()->ShotHearingCm))
		{
			LastShotTime = Now;
		}
	}
	else if (E.Tag == Tag(TEXT("Event.Rearview.TailStarted"))) { bTailed = true; TailSince = Now; }
	else if (E.Tag == Tag(TEXT("Event.Rearview.TailBlown"))) { bTailed = false; }
	else if (E.Tag == Tag(TEXT("Event.Rearview.GangAttack"))) { bTailed = false; LastShotTime = Now; } // the pretence is over
	else if (E.Tag == MurdarTags::Event_Police_PursuitEnded) { PursuitEndedAt = Now; }
	else if (E.Tag == Tag(TEXT("Event.Dialogue.Started"))) { ++ActiveDialogues; }
	else if (E.Tag == Tag(TEXT("Event.Dialogue.Ended"))) { ActiveDialogues = FMath::Max(0, ActiveDialogues - 1); }

	if (GetDefault<UMusicSettings>()->Stings.Contains(E.Tag)) { PlaySting(E.Tag); }
}

MurdarMusic::FInputs UMusicSubsystem::GatherInputs(float Now) const
{
	MurdarMusic::FInputs In;
	if (const UTensionSubsystem* Tension = UTensionSubsystem::Get(this)) { In.Stress01 = Tension->GetStressAlpha(); }
	if (const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld())) { In.Wanted = int(Mem->GetWantedLevel()); }
	In.SinceShot = Now - LastShotTime;
	In.bTailed = bTailed && Now - TailSince < GetDefault<UMusicSettings>()->TailMemorySeconds;
	In.SincePursuitEnded = Now - PursuitEndedAt;
	return In;
}

void UMusicSubsystem::Tick10Hz()
{
	const float Now = GetWorld()->GetTimeSeconds();
	const MurdarMusic::FTuning T = Tuning();
	const MurdarMusic::FInputs In = GatherInputs(Now);
	const MurdarMusic::EMood Target = ForcedMood >= 0 ? MurdarMusic::EMood(FMath::Clamp(ForcedMood, 0, 5)) : MurdarMusic::TargetMood(In, T);
	if (Machine.Update(Target, Now, T))
	{
		UE_LOG(LogTemp, Verbose, TEXT("Music mood -> %d"), int32(Machine.Get()));
	}
	Intensity = MurdarMusic::Approach(Intensity, MurdarMusic::Intensity(Machine.Get(), In.Stress01), TickSeconds, T);

	if (Intensity > 0.f)
	{
		SilentSince = Now;
		EnsurePlaying();
	}
	else if (Now - SilentSince > StopAfterSilentSeconds)
	{
		StopAll();
	}
	ApplyOutput();
}

void UMusicSubsystem::EnsurePlaying()
{
	const UMusicSettings* S = GetDefault<UMusicSettings>();
	if (!S->MusicMetaSound.IsNull())
	{
		if (!Score)
		{
			if (USoundBase* Sound = S->MusicMetaSound.LoadSynchronous())
			{
				Score = UGameplayStatics::SpawnSound2D(this, Sound, MinAudible, 1.f, 0.f, nullptr, false, /*bAutoDestroy*/ false);
			}
		}
		return;
	}
	if (StemComponents.Num() > 0 || S->Stems.Num() == 0) { return; }
	// All stems in the same frame so they start in step (ADAPT: Quartz if drift is ever audible).
	for (const FMusicStem& Stem : S->Stems)
	{
		USoundBase* Sound = Stem.Sound.LoadSynchronous();
		StemComponents.Add(Sound ? UGameplayStatics::SpawnSound2D(this, Sound, MinAudible, 1.f, 0.f, nullptr, false, false) : nullptr);
	}
}

void UMusicSubsystem::StopAll()
{
	if (Score) { Score->Stop(); Score->DestroyComponent(); Score = nullptr; }
	for (UAudioComponent* C : StemComponents) { if (C) { C->Stop(); C->DestroyComponent(); } }
	StemComponents.Reset();
}

void UMusicSubsystem::ApplyOutput()
{
	const UMusicSettings* S = GetDefault<UMusicSettings>();
	const float Duck = ActiveDialogues > 0 ? FMath::Pow(10.f, S->DialogueDuckDb / 20.f) : 1.f;
	const float Master = S->MasterVolume * Duck;
	if (Score)
	{
		Score->SetFloatParameter(S->IntensityParameter, Intensity);
		Score->SetIntParameter(S->MoodParameter, int32(Machine.Get()));
		Score->AdjustVolume(TickSeconds, FMath::Max(MinAudible, Master));
	}
	for (int32 i = 0; i < StemComponents.Num() && i < S->Stems.Num(); ++i)
	{
		if (UAudioComponent* C = StemComponents[i])
		{
			const FMusicStem& Stem = S->Stems[i];
			const float Gain = MurdarMusic::LayerGain(Intensity, Stem.FadeInStart, Stem.FullAt) * Stem.Volume * Master;
			C->AdjustVolume(TickSeconds, FMath::Max(MinAudible, Gain)); // a 0.1 s ramp per step: no zipper noise
		}
	}
}

void UMusicSubsystem::PlaySting(const FGameplayTag& EventTag)
{
	const UMusicSettings* S = GetDefault<UMusicSettings>();
	const FMusicStingSet* Set = S->Stings.Find(EventTag);
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Set || Set->Variants.Num() == 0 || Now < StingNextAllowed.FindRef(EventTag)) { return; }
	StingNextAllowed.Add(EventTag, Now + S->StingCooldownSeconds);
	if (USoundBase* Sound = Set->Variants[FMath::RandHelper(Set->Variants.Num())].LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Sound, S->MasterVolume);
	}
}

void UMusicSubsystem::ForceMood(int32 Mood, bool bForce)
{
	ForcedMood = bForce ? FMath::Clamp(Mood, 0, 5) : -1;
}

FString UMusicSubsystem::Describe() const
{
	static const TCHAR* Names[] = { TEXT("silence"), TEXT("unease"), TEXT("suspense"), TEXT("aftermath"), TEXT("pursuit"), TEXT("combat") };
	const float Now = GetWorld()->GetTimeSeconds();
	const MurdarMusic::FInputs In = GatherInputs(Now);
	return FString::Printf(TEXT("Music: %s for %.0f s, intensity %.2f | stress %.2f wanted %d shot %.0f s ago tailed %d chase ended %.0f s ago | %s%s%s"),
		Names[int32(Machine.Get())], Machine.PlayingFor(Now), Intensity, In.Stress01, In.Wanted, FMath::Min(In.SinceShot, 999.f),
		In.bTailed ? 1 : 0, FMath::Min(In.SincePursuitEnded, 999.f),
		Score ? TEXT("metasound") : *FString::Printf(TEXT("%d stems"), StemComponents.Num()),
		ActiveDialogues > 0 ? TEXT(", ducked") : TEXT(""), ForcedMood >= 0 ? TEXT(", FORCED") : TEXT(""));
}
