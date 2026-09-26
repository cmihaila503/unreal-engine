#include "UI/Menus/SMurdarPauseMenu.h"

#include "UI/Menus/MenuRules.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// Same palette as SMurdarHudWidget.
	const FLinearColor TextColor(0.95f, 0.94f, 0.9f, 1.f);
	const FLinearColor DimColor(0.8f, 0.78f, 0.72f, 0.45f);
	const FLinearColor AccentColor(1.f, 0.72f, 0.25f, 1.f);

	FSlateFontInfo Font(int32 Size, bool bBold = false) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size); }

	const FSlateBrush* Backdrop() { static FSlateColorBrush B(FLinearColor(0.f, 0.f, 0.f, 0.72f)); return &B; }
	const FSlateBrush* RowBrush() { static FSlateRoundedBoxBrush B(FLinearColor(1.f, 0.72f, 0.25f, 0.12f), 4.f); return &B; }
	const FSlateBrush* NoBrush() { static FSlateNoResource B; return &B; }

	bool IsAny(const FKey& K, std::initializer_list<FKey> Keys) { for (const FKey& X : Keys) { if (K == X) { return true; } } return false; }
}

void SMurdarPauseMenu::Construct(const FArguments& InArgs)
{
	Header = InArgs._Header;
	ChildSlot
	[
		SNew(SBorder).BorderImage(Backdrop()).Padding(FMargin(80.f, 70.f))
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Text_Lambda([this] { return Header ? Header() : FText::GetEmpty(); })
				.Font(Font(14)).ColorAndOpacity(DimColor)
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(560.f)
				[
					SAssignNew(RowsBox, SVerticalBox)
				]
			]
		]
	];
}

void SMurdarPauseMenu::SetPage(FMenuPage InPage)
{
	Page = MoveTemp(InPage);
	const TArray<bool> Mask = EnabledMask();
	Focus = MurdarMenu::FirstFocus(std::vector<bool>(Mask.GetData(), Mask.GetData() + Mask.Num()));
	Rebuild();
}

TArray<bool> SMurdarPauseMenu::EnabledMask() const
{
	TArray<bool> Mask;
	for (const FMenuRow& R : Page.Rows) { Mask.Add(R.Enabled()); }
	return Mask;
}

void SMurdarPauseMenu::Rebuild()
{
	RowsBox->ClearChildren();
	RowsBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 26.f)
	[
		SNew(STextBlock).Text(Page.Title).Font(Font(30, true)).ColorAndOpacity(TextColor)
	];

	for (int32 i = 0; i < Page.Rows.Num(); ++i)
	{
		const bool bOption = static_cast<bool>(Page.Rows[i].Value);
		TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Page.Rows[i].Label).Font(Font(18))
				.ColorAndOpacity_Lambda([this, i] { return Page.Rows.IsValidIndex(i) && Page.Rows[i].Enabled() ? (Focus == i ? AccentColor : TextColor) : DimColor; })
			];
		if (bOption)
		{
			// "<  value  >": the arrows are buttons for the mouse; keys and pad use left / right.
			auto Arrow = [this, i](const TCHAR* Glyph, int32 Dir)
			{
				return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder")
					.OnClicked_Lambda([this, i, Dir] { Focus = i; StepFocused(Dir); return FReply::Handled(); })
					[ SNew(STextBlock).Text(FText::FromString(Glyph)).Font(Font(18, true)).ColorAndOpacity(AccentColor) ];
			};
			Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[ Arrow(TEXT("<"), -1) ];
			Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f)
			[
				SNew(SBox).MinDesiredWidth(170.f).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text_Lambda([this, i] { return Page.Rows.IsValidIndex(i) && Page.Rows[i].Value ? Page.Rows[i].Value() : FText::GetEmpty(); })
					.Font(Font(18)).ColorAndOpacity(TextColor)
				]
			];
			Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[ Arrow(TEXT(">"), +1) ];
		}

		RowsBox->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage_Lambda([this, i] { return Focus == i ? RowBrush() : NoBrush(); })
			.Padding(FMargin(14.f, 8.f))
			.OnMouseButtonDown_Lambda([this, i](const FGeometry&, const FPointerEvent&)
			{
				if (!Page.Rows.IsValidIndex(i) || !Page.Rows[i].Enabled()) { return FReply::Handled(); }
				Focus = i;
				ActivateRow(i);
				return FReply::Handled();
			})
			[ Line ]
		];
	}

	RowsBox->AddSlot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
	[
		SNew(STextBlock)
		.Text_Lambda([this] { return Page.Footer ? Page.Footer() : FText::GetEmpty(); })
		.Font(Font(14)).ColorAndOpacity(AccentColor).AutoWrapText(true)
	];
}

void SMurdarPauseMenu::Move(int32 Dir)
{
	const TArray<bool> Mask = EnabledMask();
	Focus = MurdarMenu::MoveFocus(std::vector<bool>(Mask.GetData(), Mask.GetData() + Mask.Num()), Focus, Dir);
}

void SMurdarPauseMenu::StepFocused(int32 Dir)
{
	if (Page.Rows.IsValidIndex(Focus) && Page.Rows[Focus].Enabled() && Page.Rows[Focus].Step) { Page.Rows[Focus].Step(Dir); }
}

void SMurdarPauseMenu::ActivateRow(int32 Index)
{
	if (!Page.Rows.IsValidIndex(Index) || !Page.Rows[Index].Enabled()) { return; }
	// Copy: the callback may replace the page (and this row with it).
	const FMenuRow Row = Page.Rows[Index];
	if (Row.Activate) { Row.Activate(); }
	else if (Row.Step) { Row.Step(+1); }
}

FReply SMurdarPauseMenu::OnKeyDown(const FGeometry&, const FKeyEvent& KeyEvent)
{
	const FKey K = KeyEvent.GetKey();
	const bool bRepeat = KeyEvent.IsRepeat();
	if (IsAny(K, { EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up })) { Move(-1); return FReply::Handled(); }
	if (IsAny(K, { EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down })) { Move(+1); return FReply::Handled(); }
	if (IsAny(K, { EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left })) { StepFocused(-1); return FReply::Handled(); }
	if (IsAny(K, { EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right })) { StepFocused(+1); return FReply::Handled(); }
	// No auto-repeat on confirm / back: holding Enter must not click through a confirmation.
	if (IsAny(K, { EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom })) { if (!bRepeat) { ActivateRow(Focus); } return FReply::Handled(); }
	if (IsAny(K, { EKeys::Escape, EKeys::BackSpace, EKeys::P, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Right }))
	{
		if (Page.Back && !bRepeat) { const TFunction<void()> Back = Page.Back; Back(); }
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FNavigationReply SMurdarPauseMenu::OnNavigation(const FGeometry&, const FNavigationEvent&)
{
	// Our own focus model (OnKeyDown); Slate's widget navigation would move keyboard focus off the menu.
	return FNavigationReply::Stop();
}
