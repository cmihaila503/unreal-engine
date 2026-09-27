#include "UI/FrontEnd/FrontEndSubsystem.h"

#include "UI/FrontEnd/FrontEndSettings.h"
#include "UI/FrontEnd/SaveSlots.h"
#include "UI/Menus/PauseMenuSubsystem.h"   // Handoff/Menus
#include "UI/Menus/SMurdarPauseMenu.h"
#include "Director/ChapterDefinition.h"
#include "Director/ChapterDirector.h"
#include "Director/NarrativeStateSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "MurdarFrontEnd"

UFrontEndSubsystem* UFrontEndSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UFrontEndSubsystem>() : nullptr;
}

bool UFrontEndSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UFrontEndSubsystem::IsFrontEndMap() const
{
	const TSoftObjectPtr<UWorld>& Map = GetDefault<UFrontEndSettings>()->FrontEndMap;
	return !Map.IsNull() && UGameplayStatics::GetCurrentLevelName(this, true) == Map.GetAssetName();
}

void UFrontEndSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsFrontEndMap() || !GEngine || !GEngine->GameViewport) { return; }
	const UFrontEndSettings* S = GetDefault<UFrontEndSettings>();
	Widget = SNew(SMurdarPauseMenu).Header([S] { return S->Title; });
	GEngine->GameViewport->AddViewportWidgetContent(Widget.ToSharedRef(), 50);
	ShowMain();
	if (APlayerController* PC = InWorld.GetFirstPlayerController())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Widget);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
}

void UFrontEndSubsystem::Deinitialize()
{
	Close();
	Super::Deinitialize();
}

void UFrontEndSubsystem::Close()
{
	if (Widget.IsValid() && GEngine && GEngine->GameViewport) { GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef()); }
	Widget.Reset();
}

void UFrontEndSubsystem::ShowMain()
{
	if (!Widget.IsValid()) { return; }
	Widget->SetVisibility(EVisibility::Visible);
	FSlateApplication::Get().SetKeyboardFocus(Widget);
	TWeakObjectPtr<UFrontEndSubsystem> Weak(this);
	const FName Continue = MurdarSlots::ContinueSlot();
	FMenuPage Page;
	Page.Title = FText::GetEmpty();
	Page.Rows.Add({ LOCTEXT("Continue", "Continuă"), nullptr, nullptr, [Weak, Continue] { if (Weak.IsValid()) { Weak->LoadSlot(Continue); } },
		[Continue] { return !Continue.IsNone(); } });
	Page.Rows.Add({ LOCTEXT("New", "Joc nou"), nullptr, nullptr, [Weak, Continue]
	{
		if (!Weak.IsValid()) { return; }
		if (Continue.IsNone()) { Weak->NewGame(); return; }
		// Existing saves stay; only the current progress starts over.
		Weak->ShowConfirm(LOCTEXT("NewQ", "Joc nou? Salvările rămân, dar pornești de la început."), [Weak] { if (Weak.IsValid()) { Weak->NewGame(); } });
	}, nullptr });
	Page.Rows.Add({ LOCTEXT("Load", "Încarcă"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->ShowLoad(); } }, [Continue] { return !Continue.IsNone(); } });
	Page.Rows.Add({ LOCTEXT("Settings", "Setări"), nullptr, nullptr, [Weak]
	{
		// The pause menu's settings page, over the title (README §Patches 3).
		if (!Weak.IsValid()) { return; }
		if (UPauseMenuSubsystem* Menu = UPauseMenuSubsystem::Get(Weak.Get()))
		{
			Weak->Widget->SetVisibility(EVisibility::Collapsed);
			Menu->OpenSettingsFromFrontEnd();
		}
	}, nullptr });
	Page.Rows.Add({ LOCTEXT("Quit", "Ieșire"), nullptr, nullptr, [Weak]
	{
		if (Weak.IsValid()) { UKismetSystemLibrary::QuitGame(Weak.Get(), Weak->GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false); }
	}, nullptr });
	Widget->SetPage(MoveTemp(Page));
}

void UFrontEndSubsystem::ShowLoad()
{
	TWeakObjectPtr<UFrontEndSubsystem> Weak(this);
	FMenuPage Page;
	Page.Title = LOCTEXT("LoadTitle", "Încarcă");
	for (const MurdarSlots::FEntry& E : MurdarSlots::Gather(true))
	{
		const FName Slot = E.Slot;
		const bool bExists = E.Info.bExists;
		Page.Rows.Add({ E.Label(), nullptr, nullptr, [Weak, Slot] { if (Weak.IsValid()) { Weak->LoadSlot(Slot); } }, [bExists] { return bExists; } });
	}
	Page.Rows.Add({ LOCTEXT("Back", "Înapoi"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->ShowMain(); } }, nullptr });
	Page.Back = [Weak] { if (Weak.IsValid()) { Weak->ShowMain(); } };
	Widget->SetPage(MoveTemp(Page));
}

void UFrontEndSubsystem::ShowConfirm(const FText& Question, TFunction<void()> OnYes)
{
	TWeakObjectPtr<UFrontEndSubsystem> Weak(this);
	FMenuPage Page;
	Page.Title = Question;
	Page.Rows.Add({ LOCTEXT("No", "Nu"), nullptr, nullptr, [Weak] { if (Weak.IsValid()) { Weak->ShowMain(); } }, nullptr });
	Page.Rows.Add({ LOCTEXT("Yes", "Da"), nullptr, nullptr, OnYes, nullptr });
	Page.Back = [Weak] { if (Weak.IsValid()) { Weak->ShowMain(); } };
	Widget->SetPage(MoveTemp(Page));
}

void UFrontEndSubsystem::NewGame()
{
	UChapterDefinition* First = GetDefault<UFrontEndSettings>()->NewGameChapter.LoadSynchronous();
	UChapterDirector* Director = UChapterDirector::Get(GetWorld());
	if (!First || !Director) { UE_LOG(LogTemp, Error, TEXT("FrontEnd: set NewGameChapter in Project Settings > Murdar Front End")); return; }
	if (UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this)) { State->ResetAll(); }
	Close();
	Director->EnterChapter(First, NAME_None, EChapterEntry::Fresh); // opens the chapter's map (loading screen)
}

void UFrontEndSubsystem::LoadSlot(FName Slot)
{
	UChapterDirector* Director = UChapterDirector::Get(GetWorld());
	if (Slot.IsNone() || !Director) { return; }
	Close();
	Director->ContinueFromSave(Slot);
}

#undef LOCTEXT_NAMESPACE
