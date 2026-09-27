#include "UI/FrontEnd/MurdarLoadingScreen.h"

#include "UI/FrontEnd/FrontEndRules.h"
#include "UI/FrontEnd/FrontEndSettings.h"

#include "MoviePlayer.h"                       // ADAPT: "MoviePlayer" in Build.cs
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateColorBrush.h"

namespace
{
	std::vector<int> RecentTips;

	TSharedRef<SWidget> MakeScreen()
	{
		const UFrontEndSettings* S = GetDefault<UFrontEndSettings>();
		const int Tip = MurdarFrontEnd::NextTip(S->Tips.Num(), RecentTips, FMath::FRand());
		if (Tip >= 0) { RecentTips.push_back(Tip); }
		static FSlateColorBrush Black(FLinearColor(0.01f, 0.01f, 0.012f));
		return SNew(SBorder).BorderImage(&Black).Padding(FMargin(80.f, 70.f))
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(STextBlock).Text(S->Title).Font(FCoreStyle::GetDefaultFontStyle("Bold", 36)).ColorAndOpacity(FLinearColor(0.95f, 0.94f, 0.9f)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
				[ SNew(STextBlock).Text(Tip >= 0 ? S->Tips[Tip] : FText::GetEmpty()).Font(FCoreStyle::GetDefaultFontStyle("Italic", 16)).ColorAndOpacity(FLinearColor(0.8f, 0.78f, 0.72f, 0.8f)) ]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
			[ SNew(SThrobber) ]
		];
	}
}

void MurdarLoadingScreen::Install()
{
	if (IsRunningDedicatedServer() || !IsMoviePlayerEnabled()) { return; }
	FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString&)
	{
		FLoadingScreenAttributes A;
		A.bAutoCompleteWhenLoadingCompletes = true;
		A.MinimumLoadingScreenDisplayTime = 1.5f; // long enough to read a tip
		A.WidgetLoadingScreen = MakeScreen();
		GetMoviePlayer()->SetupLoadingScreen(A);
	});
}
