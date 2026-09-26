#include "UI/Menus/PauseMenuSubsystem.h"

#include "UI/Menus/MenuSettings.h"
#include "UI/Menus/MurdarPlayerSettings.h"
#include "UI/Menus/SMurdarPauseMenu.h"
#include "Director/ChapterDefinition.h"
#include "Director/ChapterDirector.h"
#include "Director/GameEventSubsystem.h"
#include "Director/MurdarTags.h"
#include "Director/NarrativeStateSubsystem.h"
#include "AI/FactionMemorySubsystem.h" // ADAPT path
#include "Vehicle/MurdarVehicle.h"     // ADAPT path

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "MurdarMenu"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
	FText Str(const std::string& S) { return FText::FromString(UTF8_TO_TCHAR(S.c_str())); }

	FText Choice(int32 Index, std::initializer_list<FText> Names)
	{
		int32 i = 0;
		for (const FText& N : Names) { if (i++ == Index) { return N; } }
		return FText::GetEmpty();
	}
}

UPauseMenuSubsystem* UPauseMenuSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UPauseMenuSubsystem>() : nullptr;
}

bool UPauseMenuSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UPauseMenuSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The core ticker runs while the game is paused, which is exactly when the menu needs it.
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPauseMenuSubsystem::TickCore));
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UPauseMenuSubsystem> Weak(this);
		BusHandle = Bus->Subscribe(MurdarTags::Event, [Weak](const FGameEvent& E) { if (Weak.IsValid()) { Weak->OnBusEvent(E); } });
	}
	UMurdarPlayerSettings::Get()->ApplyAudio(&InWorld); // the saved volumes from the first frame
}

void UPauseMenuSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this)) { Bus->Unsubscribe(BusHandle); }
	if (Widget.IsValid() && GEngine && GEngine->GameViewport) { GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef()); }
	Widget.Reset();
	Super::Deinitialize();
}

void UPauseMenuSubsystem::OnBusEvent(const FGameEvent& E)
{
	if (E.Tag == MurdarTags::Event_Combat_Shot) { LastShotTime = GetWorld()->GetTimeSeconds(); }
	else if (E.Tag == Tag(TEXT("Event.Mission.Started"))) { ++ActiveMissions; }
	else if (E.Tag == Tag(TEXT("Event.Mission.Succeeded")) || E.Tag == Tag(TEXT("Event.Mission.Failed"))) { ActiveMissions = FMath::Max(0, ActiveMissions - 1); }
	else if (E.Tag == Tag(TEXT("Event.Dialogue.Started"))) { ++ActiveDialogues; }
	else if (E.Tag == Tag(TEXT("Event.Dialogue.Ended"))) { ActiveDialogues = FMath::Max(0, ActiveDialogues - 1); }
}

bool UPauseMenuSubsystem::TickCore(float)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) { return true; }

	// Display change waiting for "keep": revert on time (the player may not be able to see the screen).
	if (DisplayConfirmUntil > 0.0 && FPlatformTime::Seconds() >= DisplayConfirmUntil)
	{
		DisplayConfirmUntil = 0.0;
		UMurdarPlayerSettings::Get()->WriteAndApply(BeforeDisplayChange, World);
		Applied = Draft = BeforeDisplayChange;
		Message = LOCTEXT("Reverted", "Setările de afișare au revenit.");
		if (IsOpen()) { ShowSettings(); }
	}

	// Opening: an edge on any open key. While the menu is open the keys go to Slate (the widget closes it).
	bool bDown = false;
	for (const FKey& K : GetDefault<UMenuSettings>()->OpenKeys) { bDown = bDown || PC->IsInputKeyDown(K); }
	if (bDown && !bKeyWasDown && !IsOpen()) { Open(); }
	bKeyWasDown = bDown;
	return true;
}

void UPauseMenuSubsystem::Open()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (IsOpen() || !PC || !GEngine || !GEngine->GameViewport) { return; }
	Message = FText::GetEmpty();
	PC->SetPause(true);
	TWeakObjectPtr<UPauseMenuSubsystem> Weak(this);
	Widget = SNew(SMurdarPauseMenu).Header([Weak] { return Weak.IsValid() ? Weak->HeaderText() : FText::GetEmpty(); });
	GEngine->GameViewport->AddViewportWidgetContent(Widget.ToSharedRef(), /*ZOrder*/ 50);
	ShowMain();
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Widget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	FSlateApplication::Get().SetKeyboardFocus(Widget);
}

void UPauseMenuSubsystem::Close()
{
	if (!IsOpen()) { return; }
	if (DisplayConfirmUntil > 0.0) // closing with the countdown running = not confirmed
	{
		DisplayConfirmUntil = 0.0;
		UMurdarPlayerSettings::Get()->WriteAndApply(BeforeDisplayChange, GetWorld());
	}
	if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef()); }
	Widget.Reset();
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		PC->SetPause(false);
	}
}

// ------------------------------------------------------------------------------------------------ pages

void UPauseMenuSubsystem::ShowMain()
{
	TWeakObjectPtr<UPauseMenuSubsystem> Weak(this);
	const UMenuSettings* S = GetDefault<UMenuSettings>();
	const FName Manual = S->ManualSlot, Auto = S->AutoSlot;

	FMenuPage Page;
	Page.Title = LOCTEXT("Paused", "Pauză");
	Page.Rows.Add({ LOCTEXT("Continue", "Continuă"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->Close(); } }, nullptr });
	Page.Rows.Add({ LOCTEXT("Save", "Salvează"), nullptr, nullptr,
		[Weak] { if (Weak.IsValid()) { Weak->DoSave(); } },
		[Weak] { return Weak.IsValid() && Weak->SaveBlock() == MurdarMenu::ESaveBlock::None; } });
	auto LoadRow = [Weak](const FText& Label, FName Slot)
	{
		return FMenuRow{ Label, nullptr, nullptr, [Weak, Slot]
		{
			if (!Weak.IsValid()) { return; }
			Weak->ShowConfirm(LOCTEXT("LoadQ", "Încarci? Ce n-ai salvat se pierde."),
				[Weak, Slot]
				{
					if (!Weak.IsValid()) { return; }
					Weak->Close();
					if (UChapterDirector* Director = UChapterDirector::Get(Weak->GetWorld())) { Director->ContinueFromSave(Slot); }
				},
				[Weak] { if (Weak.IsValid()) { Weak->ShowMain(); } });
		}, [Slot] { return UNarrativeStateSubsystem::SaveExists(Slot); } };
	};
	Page.Rows.Add(LoadRow(LOCTEXT("LoadManual", "Încarcă salvarea"), Manual));
	Page.Rows.Add(LoadRow(LOCTEXT("LoadAuto", "Ultimul checkpoint"), Auto));
	Page.Rows.Add({ LOCTEXT("Settings", "Setări"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->ShowSettings(); } }, nullptr });
	Page.Rows.Add({ LOCTEXT("Quit", "Ieși din joc"), nullptr, nullptr, [Weak]
	{
		if (!Weak.IsValid()) { return; }
		Weak->ShowConfirm(LOCTEXT("QuitQ", "Ieși din joc? Ce n-ai salvat se pierde."),
			[Weak] { if (Weak.IsValid()) { UKismetSystemLibrary::QuitGame(Weak.Get(), Weak->GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false); } },
			[Weak] { if (Weak.IsValid()) { Weak->ShowMain(); } });
	}, nullptr });
	Page.Footer = [Weak]
	{
		if (!Weak.IsValid()) { return FText::GetEmpty(); }
		if (!Weak->Message.IsEmpty()) { return Weak->Message; }
		return SaveBlockText(Weak->SaveBlock());
	};
	Page.Back = [Weak] { if (Weak.IsValid()) { Weak->Close(); } };
	Widget->SetPage(MoveTemp(Page));
}

void UPauseMenuSubsystem::ShowConfirm(const FText& Question, TFunction<void()> OnYes, TFunction<void()> OnNo)
{
	FMenuPage Page;
	Page.Title = Question;
	Page.Rows.Add({ LOCTEXT("No", "Nu"), nullptr, nullptr, OnNo, nullptr }); // "No" first: a stray Enter is harmless
	Page.Rows.Add({ LOCTEXT("Yes", "Da"), nullptr, nullptr, OnYes, nullptr });
	Page.Back = OnNo;
	Widget->SetPage(MoveTemp(Page));
}

void UPauseMenuSubsystem::ShowSettings()
{
	TWeakObjectPtr<UPauseMenuSubsystem> Weak(this);
	if (DisplayConfirmUntil <= 0.0)
	{
		Applied = UMurdarPlayerSettings::Get()->Read();
		Draft = Applied;
	}
	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	std::vector<MurdarMenu::FRes> All;
	for (const FIntPoint& P : Supported) { All.push_back({ P.X, P.Y }); }
	Resolutions = MurdarMenu::OfferedResolutions(All, Draft.Resolution);

	auto ChoiceRow = [Weak](const FText& Label, int32 MurdarMenu::FSettings::* Field, int32 Count, TFunction<FText(int32)> Name)
	{
		return FMenuRow{ Label,
			[Weak, Field, Name] { return Weak.IsValid() ? Name(Weak->Draft.*Field) : FText::GetEmpty(); },
			[Weak, Field, Count](int32 Dir) { if (Weak.IsValid()) { Weak->Draft.*Field = MurdarMenu::StepChoice(Weak->Draft.*Field, Count, Dir); } },
			nullptr, nullptr };
	};
	auto BoolRow = [Weak](const FText& Label, bool MurdarMenu::FSettings::* Field)
	{
		return FMenuRow{ Label,
			[Weak, Field] { return Weak.IsValid() && Weak->Draft.*Field ? LOCTEXT("On", "Da") : LOCTEXT("Off", "Nu"); },
			[Weak, Field](int32) { if (Weak.IsValid()) { Weak->Draft.*Field = !(Weak->Draft.*Field); } },
			nullptr, nullptr };
	};
	auto SliderRow = [Weak](const FText& Label, float MurdarMenu::FSettings::* Field, float Min, float Max, float Step, bool bPercent)
	{
		return FMenuRow{ Label,
			[Weak, Field, bPercent]
			{
				if (!Weak.IsValid()) { return FText::GetEmpty(); }
				const float V = Weak->Draft.*Field;
				return bPercent ? Str(MurdarMenu::Percent(V)) : FText::AsNumber(V, &FNumberFormattingOptions().SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1));
			},
			[Weak, Field, Min, Max, Step](int32 Dir) { if (Weak.IsValid()) { Weak->Draft.*Field = MurdarMenu::StepSlider(Weak->Draft.*Field, Min, Max, Step, Dir); } },
			nullptr, nullptr };
	};

	FMenuPage Page;
	Page.Title = LOCTEXT("SettingsTitle", "Setări");
	Page.Rows.Add(ChoiceRow(LOCTEXT("Quality", "Calitate grafică"), &MurdarMenu::FSettings::Quality, 4, [](int32 i)
		{ return Choice(i, { LOCTEXT("Q0", "Scăzută"), LOCTEXT("Q1", "Medie"), LOCTEXT("Q2", "Ridicată"), LOCTEXT("Q3", "Epică") }); }));
	Page.Rows.Add(ChoiceRow(LOCTEXT("Window", "Afișare"), &MurdarMenu::FSettings::WindowMode, 3, [](int32 i)
		{ return Choice(i, { LOCTEXT("W0", "Ecran complet"), LOCTEXT("W1", "Fără margini"), LOCTEXT("W2", "Fereastră") }); }));
	Page.Rows.Add(FMenuRow{ LOCTEXT("Resolution", "Rezoluție"),
		[Weak] { return Weak.IsValid() ? Str(MurdarMenu::Format(Weak->Draft.Resolution)) : FText::GetEmpty(); },
		[Weak](int32 Dir)
		{
			if (!Weak.IsValid() || Weak->Resolutions.empty()) { return; }
			const int32 I = MurdarMenu::StepChoice(FMath::Max(0, MurdarMenu::IndexOf(Weak->Resolutions, Weak->Draft.Resolution)), int32(Weak->Resolutions.size()), Dir);
			Weak->Draft.Resolution = Weak->Resolutions[I];
		}, nullptr,
		[Weak] { return Weak.IsValid() && Weak->Draft.WindowMode != 1; } }); // borderless uses the desktop's
	Page.Rows.Add(BoolRow(LOCTEXT("VSync", "VSync"), &MurdarMenu::FSettings::bVSync));
	Page.Rows.Add(ChoiceRow(LOCTEXT("Fps", "Limită FPS"), &MurdarMenu::FSettings::FrameLimit, 5, [](int32 i)
		{ return i == 0 ? LOCTEXT("Unlimited", "Fără") : FText::AsNumber(MurdarMenu::FrameLimitValue(i)); }));
	Page.Rows.Add(SliderRow(LOCTEXT("Master", "Volum general"), &MurdarMenu::FSettings::Master, 0.f, 1.f, 0.05f, true));
	Page.Rows.Add(SliderRow(LOCTEXT("Music", "Muzică"), &MurdarMenu::FSettings::Music, 0.f, 1.f, 0.05f, true));
	Page.Rows.Add(SliderRow(LOCTEXT("Effects", "Efecte"), &MurdarMenu::FSettings::Effects, 0.f, 1.f, 0.05f, true));
	Page.Rows.Add(SliderRow(LOCTEXT("Voices", "Voci"), &MurdarMenu::FSettings::Voices, 0.f, 1.f, 0.05f, true));
	Page.Rows.Add(SliderRow(LOCTEXT("Sensitivity", "Sensibilitate mouse"), &MurdarMenu::FSettings::Sensitivity, 0.2f, 3.f, 0.1f, false));
	Page.Rows.Add(BoolRow(LOCTEXT("InvertY", "Inversează axa Y"), &MurdarMenu::FSettings::bInvertY));
	Page.Rows.Add(ChoiceRow(LOCTEXT("Subtitles", "Subtitrări"), &MurdarMenu::FSettings::SubtitleSize, 3, [](int32 i)
		{ return Choice(i, { LOCTEXT("S0", "Mici"), LOCTEXT("S1", "Normale"), LOCTEXT("S2", "Mari") }); }));
	Page.Rows.Add({ LOCTEXT("Apply", "Aplică"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->ApplyDraft(); } },
		[Weak] { return Weak.IsValid() && Weak->Draft != Weak->Applied; } });
	Page.Rows.Add({ LOCTEXT("Back", "Înapoi"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->BackFromSettings(); } }, nullptr });
	Page.Footer = [Weak]
	{
		if (!Weak.IsValid()) { return FText::GetEmpty(); }
		if (!Weak->Message.IsEmpty()) { return Weak->Message; }
		return Weak->Draft != Weak->Applied ? LOCTEXT("Unapplied", "Modificări neaplicate.") : FText::GetEmpty();
	};
	Page.Back = [Weak] { if (Weak.IsValid()) { Weak->BackFromSettings(); } };
	Widget->SetPage(MoveTemp(Page));
}

void UPauseMenuSubsystem::BackFromSettings()
{
	TWeakObjectPtr<UPauseMenuSubsystem> Weak(this);
	Message = FText::GetEmpty();
	if (Draft == Applied) { ShowMain(); return; }
	ShowConfirm(LOCTEXT("DiscardQ", "Renunți la modificări?"),
		[Weak] { if (Weak.IsValid()) { Weak->Draft = Weak->Applied; Weak->ShowMain(); } },
		[Weak] { if (Weak.IsValid()) { Weak->ShowSettings(); } });
}

void UPauseMenuSubsystem::ApplyDraft()
{
	const bool bDisplay = MurdarMenu::NeedsDisplayConfirm(Applied, Draft);
	BeforeDisplayChange = Applied;
	UMurdarPlayerSettings::Get()->WriteAndApply(Draft, GetWorld());
	if (!bDisplay)
	{
		Applied = Draft;
		Message = LOCTEXT("Applied", "Aplicat.");
		ShowSettings();
		return;
	}
	DisplayConfirmUntil = FPlatformTime::Seconds() + GetDefault<UMenuSettings>()->DisplayConfirmSeconds;
	ShowDisplayConfirm();
}

void UPauseMenuSubsystem::ShowDisplayConfirm()
{
	TWeakObjectPtr<UPauseMenuSubsystem> Weak(this);
	FMenuPage Page;
	Page.Title = LOCTEXT("KeepQ", "Păstrezi afișarea?");
	auto Revert = [Weak]
	{
		if (!Weak.IsValid()) { return; }
		Weak->DisplayConfirmUntil = 0.0;
		UMurdarPlayerSettings::Get()->WriteAndApply(Weak->BeforeDisplayChange, Weak->GetWorld());
		Weak->Applied = Weak->Draft = Weak->BeforeDisplayChange;
		Weak->ShowSettings();
	};
	Page.Rows.Add({ LOCTEXT("Revert", "Revino"), nullptr, nullptr, Revert, nullptr });
	Page.Rows.Add({ LOCTEXT("Keep", "Păstrează"), nullptr, nullptr, [Weak]
	{
		if (!Weak.IsValid()) { return; }
		Weak->DisplayConfirmUntil = 0.0;
		Weak->Applied = Weak->Draft;
		Weak->Message = LOCTEXT("Applied", "Aplicat.");
		Weak->ShowSettings();
	}, nullptr });
	Page.Footer = [Weak]
	{
		const int32 Left = Weak.IsValid() ? FMath::Max(0, FMath::CeilToInt(float(Weak->DisplayConfirmUntil - FPlatformTime::Seconds()))) : 0;
		return FText::Format(LOCTEXT("RevertIn", "Revine singur în {0} s."), FText::AsNumber(Left));
	};
	Page.Back = Revert;
	Widget->SetPage(MoveTemp(Page));
}

// ------------------------------------------------------------------------------------------------ saving

MurdarMenu::ESaveBlock UPauseMenuSubsystem::SaveBlock() const
{
	const UMenuSettings* S = GetDefault<UMenuSettings>();
	MurdarMenu::FSaveContext C;
	if (const UFactionMemorySubsystem* Mem = UFactionMemorySubsystem::Get(GetWorld())) { C.Wanted = int(Mem->GetWantedLevel()); }
	C.bMissionActive = ActiveMissions > 0;
	C.SinceShot = GetWorld()->GetTimeSeconds() - LastShotTime;
	C.bTalking = ActiveDialogues > 0;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AMurdarVehicle* Car = PC ? Cast<AMurdarVehicle>(PC->GetPawn()) : nullptr;
	C.bDrivingFast = Car && Car->GetSpeedKph() > S->SaveMaxKph;
	return MurdarMenu::CanSave(C, S->CombatWindowSeconds);
}

FText UPauseMenuSubsystem::SaveBlockText(MurdarMenu::ESaveBlock Block)
{
	switch (Block)
	{
	case MurdarMenu::ESaveBlock::Wanted:  return LOCTEXT("NoSaveWanted", "Nu poți salva: poliția e pe urmele tale.");
	case MurdarMenu::ESaveBlock::Combat:  return LOCTEXT("NoSaveCombat", "Nu poți salva acum. Încă se trage.");
	case MurdarMenu::ESaveBlock::Mission: return LOCTEXT("NoSaveMission", "Nu poți salva în mijlocul unei treburi.");
	case MurdarMenu::ESaveBlock::Talking: return LOCTEXT("NoSaveTalking", "Nu poți salva în timp ce vorbești cu cineva.");
	case MurdarMenu::ESaveBlock::Driving: return LOCTEXT("NoSaveDriving", "Oprește mașina ca să salvezi.");
	default: return FText::GetEmpty();
	}
}

void UPauseMenuSubsystem::DoSave()
{
	if (SaveBlock() != MurdarMenu::ESaveBlock::None) { return; }
	if (UChapterDirector* Director = UChapterDirector::Get(GetWorld())) { Director->CaptureLoadout(); } // as MurdarSave does
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const bool bOk = State && State->Save(GetDefault<UMenuSettings>()->ManualSlot);
	Message = bOk ? LOCTEXT("Saved", "Salvat.") : LOCTEXT("SaveFailed", "Salvarea a eșuat.");
}

FText UPauseMenuSubsystem::HeaderText() const
{
	// The only place the game shows the hour and the money: a man checking his watch and his pockets.
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const UChapterDirector* Director = UChapterDirector::Get(GetWorld());
	const UChapterDefinition* Chapter = Director ? Director->GetCurrentChapter() : nullptr;
	const float Minutes = State ? State->GetValue(Tag(TEXT("Stat.TimeOfDay")), -1.f) : -1.f;
	const int32 Day = State ? FMath::RoundToInt(State->GetValue(Tag(TEXT("Stat.Day")))) : 0;
	const float Money = State ? State->GetValue(Tag(TEXT("Stat.Money"))) : 0.f;

	FString Out = Chapter && !Chapter->DisplayName.IsEmpty() ? Chapter->DisplayName.ToString() : FString();
	if (Minutes >= 0.f)
	{
		const int32 M = FMath::FloorToInt(Minutes);
		Out += FString::Printf(TEXT("%sZiua %d, %02d:%02d"), Out.IsEmpty() ? TEXT("") : TEXT("\n"), Day + 1, (M / 60) % 24, M % 60);
	}
	Out += FString::Printf(TEXT("%s%s lei"), Out.IsEmpty() ? TEXT("") : TEXT("\n"), *FText::AsNumber(FMath::RoundToInt(Money)).ToString());
	return FText::FromString(Out);
}

#undef LOCTEXT_NAMESPACE
