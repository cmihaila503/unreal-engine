// The pause menu's look: one column of rows over a darkened game, in the HUD's style (SMurdarHudWidget fonts and
// colours). It knows nothing about saving or graphics: UPauseMenuSubsystem hands it pages of rows with callbacks.
// Keyboard, gamepad and mouse; focus moves with MurdarMenu::MoveFocus (unit-tested).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

struct FMenuRow
{
	FText Label;
	/** Set = an option row ("< Ridicat >"): Step(-1 / +1) changes it. Unset = a button row. */
	TFunction<FText()> Value;
	TFunction<void(int32)> Step;
	/** Enter / click. On an option row, defaults to Step(+1). */
	TFunction<void()> Activate;
	/** Unset = always enabled. Disabled rows are shown dim and skipped by focus. */
	TFunction<bool()> IsEnabled;

	bool Enabled() const { return !IsEnabled || IsEnabled(); }
};

struct FMenuPage
{
	FText Title;
	TArray<FMenuRow> Rows;
	/** A line under the rows: why saving is refused, the display countdown... */
	TFunction<FText()> Footer;
	/** Escape / B. Unset = nothing. */
	TFunction<void()> Back;
};

class SMurdarPauseMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMurdarPauseMenu) {}
		/** Top-left block: chapter, day and hour, cash. */
		SLATE_ARGUMENT(TFunction<FText()>, Header)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetPage(FMenuPage InPage);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FNavigationReply OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event) override;

private:
	void Rebuild();
	void Move(int32 Dir);
	void StepFocused(int32 Dir);
	void ActivateRow(int32 Index);
	TArray<bool> EnabledMask() const;

	FMenuPage Page;
	int32 Focus = -1;
	TFunction<FText()> Header;
	TSharedPtr<SVerticalBox> RowsBox;
};
