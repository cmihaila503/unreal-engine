// The pause menu: Esc / P / Start opens it anywhere (polled on the core ticker, so it works paused, on foot and at the
// wheel, without an input asset). Pages: Continue, Save (only when nothing is going on — MurdarMenu::CanSave), Load,
// Settings (graphics through UGameUserSettings with a confirm-or-revert countdown for the display; audio, mouse,
// subtitles through UMurdarPlayerSettings), Quit. The header is the only place the game shows money and the hour.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/Menus/MenuRules.h"
#include "PauseMenuSubsystem.generated.h"

class SMurdarPauseMenu;
struct FGameEvent;
struct FMenuPage;

UCLASS()
class MURDAR_GAMEDEV_API UPauseMenuSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UPauseMenuSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Murdar|Menu") void Open();
	UFUNCTION(BlueprintCallable, Category = "Murdar|Menu") void Close();
	UFUNCTION(BlueprintPure, Category = "Murdar|Menu") bool IsOpen() const { return Widget.IsValid(); }

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool TickCore(float DeltaTime);
	void OnBusEvent(const FGameEvent& Event);

	void ShowMain();
	void ShowSettings();
	void ShowConfirm(const FText& Question, TFunction<void()> OnYes, TFunction<void()> OnNo);
	void ShowDisplayConfirm();

	MurdarMenu::ESaveBlock SaveBlock() const;
	static FText SaveBlockText(MurdarMenu::ESaveBlock Block);
	void DoSave();
	FText HeaderText() const;
	void ApplyDraft();
	void BackFromSettings();

	TSharedPtr<SMurdarPauseMenu> Widget;
	FTSTicker::FDelegateHandle TickerHandle;
	bool bKeyWasDown = false;

	// What the save rule needs, from the bus (so no handoff is a hard dependency).
	int32 ActiveMissions = 0;
	int32 ActiveDialogues = 0;
	float LastShotTime = -1e6f;
	int32 BusHandle = 0;

	// Settings page
	MurdarMenu::FSettings Draft;
	MurdarMenu::FSettings Applied;
	std::vector<MurdarMenu::FRes> Resolutions;
	double DisplayConfirmUntil = 0.0;
	MurdarMenu::FSettings BeforeDisplayChange;
	FText Message;
};
