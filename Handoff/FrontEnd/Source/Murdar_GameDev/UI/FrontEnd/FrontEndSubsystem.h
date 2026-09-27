// The title screen, on the front-end map only: Continuă (newest save) · Joc nou · Încarcă (slots) · Setări · Ieșire.
// Same look and controls as the pause menu (SMurdarPauseMenu, Handoff/Menus). The game isn't paused here: the map
// behind it is a quiet street living its evening.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FrontEndSubsystem.generated.h"

class SMurdarPauseMenu;

UCLASS()
class MURDAR_GAMEDEV_API UFrontEndSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UFrontEndSubsystem* Get(const UObject* WorldContext);
	/** Back from the settings page (UPauseMenuSubsystem calls it when it was opened from here). */
	void ShowMain();

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsFrontEndMap() const;
	void ShowLoad();
	void ShowConfirm(const FText& Question, TFunction<void()> OnYes);
	void NewGame();
	void LoadSlot(FName Slot);
	void Close();

	TSharedPtr<SMurdarPauseMenu> Widget;
};
