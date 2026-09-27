#include "Director/Radio/RadioSubsystem.h"

#include "Director/Radio/RadioSettings.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Director/Music/MusicSubsystem.h"      // Handoff/Music
#include "UI/Menus/MurdarPlayerSettings.h"      // Handoff/Menus (+ README patch: bStreamerMode)
#include "Character/MurdarHUD.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
	std::string ToStd(const FGameplayTag& T) { return T.IsValid() ? std::string(TCHAR_TO_UTF8(*T.GetTagName().ToString())) : std::string(); }
	float Dur(const TSoftObjectPtr<USoundBase>& S) { const USoundBase* B = S.LoadSynchronous(); return B ? B->GetDuration() : 0.f; }
}

URadioSubsystem* URadioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URadioSubsystem>() : nullptr;
}

bool URadioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URadioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this))
	{
		Station = FMath::RoundToInt(State->GetValue(Tag(TEXT("Stat.RadioStation")), 0.f));
	}
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<URadioSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
}

void URadioSubsystem::Deinitialize()
{
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	Stop();
	Super::Deinitialize();
}

void URadioSubsystem::Rebuild()
{
	// Durations come from the sound assets (loaded once, the first time he sits in a car). ADAPT: songs should
	// stream (Loading Behavior: Stream) so this doesn't pull whole files into memory.
	const URadioSettings* S = GetDefault<URadioSettings>();
	const bool bStreamer = UMurdarPlayerSettings::Get()->bStreamerMode;
	Programs.clear();
	for (int32 i = 0; i < S->Stations.Num(); ++i)
	{
		const FRadioStation& St = S->Stations[i];
		MurdarRadio::FStationContent C;
		for (const FRadioTrack& T : St.Songs) { C.Songs.push_back(Dur(T.Sound)); C.SongLicensed.push_back(T.bLicensed); }
		for (const TSoftObjectPtr<USoundBase>& D : St.DjLinks) { C.Djs.push_back(Dur(D)); }
		for (const TSoftObjectPtr<USoundBase>& A : St.Ads) { C.Ads.push_back(Dur(A)); }
		C.NewsDuration = St.NewsSlotSeconds;
		C.SongsPerDj = St.SongsPerDj; C.SongsPerAd = St.SongsPerAd; C.SongsPerNews = St.SongsPerNews;
		C.Seed = 1000u + unsigned(i) * 7919u;
		Programs.push_back(MurdarRadio::BuildProgram(C, bStreamer));
	}
	bBuilt = true;
	PlayingItem = -1;
}

void URadioSubsystem::OnBusEvent(const FGameEvent& E)
{
	if (E.Tag == MurdarTags::Event_Player_EnteredVehicle) { bInCar = true; if (!bBuilt) { Rebuild(); } StartCurrent(); return; }
	if (E.Tag == MurdarTags::Event_Player_ExitedVehicle) { bInCar = false; Stop(); return; }
	if (E.Tag == Tag(TEXT("Event.Dialogue.Started"))) { ++Dialogues; }
	if (E.Tag == Tag(TEXT("Event.Dialogue.Ended"))) { Dialogues = FMath::Max(0, Dialogues - 1); }
	for (const FRadioNewsTrigger& T : GetDefault<URadioSettings>()->NewsTriggers)
	{
		if (T.Event.IsValid() && E.Tag.MatchesTag(T.Event))
		{
			Desk.Add(ToStd(T.Story), T.Priority, GetWorld()->GetTimeSeconds(), T.FreshSeconds);
		}
	}
}

void URadioSubsystem::Tune(int32 Dir)
{
	const URadioSettings* S = GetDefault<URadioSettings>();
	Station = MurdarRadio::Dial(Station, S->Stations.Num(), Dir);
	if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this)) { State->SetValue(Tag(TEXT("Stat.RadioStation")), float(Station)); }
	if (USoundBase* Static = S->TuneStatic.LoadSynchronous()) { UGameplayStatics::PlaySound2D(this, Static, 0.5f); }
	if (AMurdarHUD* HUD = AMurdarHUD::Get(this))
	{
		HUD->ShowSubtitle(Station >= 0 ? S->Stations[Station].Name : NSLOCTEXT("MurdarRadio", "Off", "Radio oprit"), 1.5f);
	}
	PlayingItem = -1;
	StartCurrent();
}

USoundBase* URadioSubsystem::SoundFor(const MurdarRadio::FItem& Item)
{
	const FRadioStation& St = GetDefault<URadioSettings>()->Stations[Station];
	switch (Item.Kind)
	{
	case MurdarRadio::EKind::Song: return St.Songs.IsValidIndex(Item.Index) ? St.Songs[Item.Index].Sound.LoadSynchronous() : nullptr;
	case MurdarRadio::EKind::Dj:   return St.DjLinks.IsValidIndex(Item.Index) ? St.DjLinks[Item.Index].LoadSynchronous() : nullptr;
	case MurdarRadio::EKind::Ad:   return St.Ads.IsValidIndex(Item.Index) ? St.Ads[Item.Index].LoadSynchronous() : nullptr;
	case MurdarRadio::EKind::News:
	{
		// The story he made, in this station's words; else the day's generic bulletin.
		const std::string Story = Desk.Take(GetWorld()->GetTimeSeconds());
		if (!Story.empty())
		{
			if (const FRadioNewsSet* Set = St.Stories.Find(FGameplayTag::RequestGameplayTag(FName(UTF8_TO_TCHAR(Story.c_str())), false)); Set && Set->Clips.Num() > 0)
			{
				return Set->Clips[FMath::RandHelper(Set->Clips.Num())].LoadSynchronous();
			}
		}
		return St.GenericNews.Num() > 0 ? St.GenericNews[FMath::RandHelper(St.GenericNews.Num())].LoadSynchronous() : nullptr;
	}
	}
	return nullptr;
}

void URadioSubsystem::StartCurrent()
{
	if (!IsPlaying() || !Programs.size() || Station >= int32(Programs.size())) { Stop(); return; }
	const std::vector<MurdarRadio::FItem>& P = Programs[Station];
	const double Now = GetWorld()->GetTimeSeconds();
	const MurdarRadio::FPosition Pos = MurdarRadio::At(P, Now, Phase(Station));
	if (Pos.Item < 0) { Stop(); return; }
	PlayingItem = Pos.Item;
	ItemEndsAt = Now + double(P[Pos.Item].Duration - Pos.Offset);
	USoundBase* Sound = SoundFor(P[Pos.Item]);
	if (!Speaker)
	{
		Speaker = UGameplayStatics::CreateSound2D(this, Sound, 1.f, 1.f, 0.f, nullptr, false, false);
	}
	if (!Speaker) { return; }
	Speaker->Stop();
	Speaker->SetSound(Sound);
	const float Lpf = GetDefault<URadioSettings>()->SpeakerLowPassHz;
	Speaker->SetLowPassFilterEnabled(Lpf > 0.f);
	if (Lpf > 0.f) { Speaker->SetLowPassFilterFrequency(Lpf); }
	// News clips start at their beginning (the slot is fixed, the clip is whatever the desk picked).
	Speaker->Play(P[Pos.Item].Kind == MurdarRadio::EKind::News ? 0.f : Pos.Offset);
}

void URadioSubsystem::Stop()
{
	if (Speaker) { Speaker->Stop(); }
	PlayingItem = -1;
}

void URadioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bInCar) { return; }
	const URadioSettings* S = GetDefault<URadioSettings>();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && Dialogues == 0) // the d-pad answers dialogue
	{
		for (const FKey& K : S->NextKeys) { if (PC->WasInputKeyJustPressed(K)) { Tune(+1); return; } }
		for (const FKey& K : S->PrevKeys) { if (PC->WasInputKeyJustPressed(K)) { Tune(-1); return; } }
	}
	if (!IsPlaying()) { return; }
	if (GetWorld()->GetTimeSeconds() >= ItemEndsAt) { StartCurrent(); }
	if (Speaker)
	{
		// Steps back under the score and under a conversation.
		const UMusicSubsystem* Music = UMusicSubsystem::Get(this);
		const float UnderMusic = Music ? FMath::Lerp(1.f, S->UnderScore, FMath::Clamp(Music->GetIntensity() * 1.5f, 0.f, 1.f)) : 1.f;
		const float UnderTalk = Dialogues > 0 ? S->UnderDialogue : 1.f;
		Speaker->SetVolumeMultiplier(S->Volume * FMath::Min(UnderMusic, UnderTalk));
	}
}

FString URadioSubsystem::Describe() const
{
	const URadioSettings* S = GetDefault<URadioSettings>();
	return FString::Printf(TEXT("Radio: %s, station %s, item %d, %d stories waiting, streamer %d"),
		bInCar ? TEXT("in car") : TEXT("on foot"), Station >= 0 && S->Stations.IsValidIndex(Station) ? *S->Stations[Station].Name.ToString() : TEXT("off"),
		PlayingItem, Desk.Count(), UMurdarPlayerSettings::Get()->bStreamerMode ? 1 : 0);
}
