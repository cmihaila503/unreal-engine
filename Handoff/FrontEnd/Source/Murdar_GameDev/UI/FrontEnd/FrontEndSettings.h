// Project Settings > Game > Murdar Front End.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FrontEndSettings.generated.h"

class UChapterDefinition;
class UTexture2D;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Front End"))
class MURDAR_GAMEDEV_API UFrontEndSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** The title-screen map (a quiet street at dusk behind the menu). Set it as the Game Default Map too. */
	UPROPERTY(EditAnywhere, config, Category = "Title") TSoftObjectPtr<UWorld> FrontEndMap;
	/** "Joc nou" enters this chapter fresh. */
	UPROPERTY(EditAnywhere, config, Category = "Title") TSoftObjectPtr<UChapterDefinition> NewGameChapter;
	UPROPERTY(EditAnywhere, config, Category = "Title") FText Title = NSLOCTEXT("MurdarFrontEnd", "Title", "MURDAR");

	/** Manual save slots (files <name>.mrd); the autosave slot is listed too. */
	UPROPERTY(EditAnywhere, config, Category = "Slots") TArray<FName> Slots = { TEXT("slot1"), TEXT("slot2"), TEXT("slot3") };
	UPROPERTY(EditAnywhere, config, Category = "Slots") FName AutoSlot = TEXT("auto");

	UPROPERTY(EditAnywhere, config, Category = "Loading") TArray<FText> Tips = {
		NSLOCTEXT("MurdarFrontEnd", "Tip1", "Un geam spart face zgomot. Uită-te în jur înainte."),
		NSLOCTEXT("MurdarFrontEnd", "Tip2", "Poliția ține minte mașina și culoarea ei, nu numărul."),
		NSLOCTEXT("MurdarFrontEnd", "Tip3", "Banii ascunși acasă nu ți-i ia nimeni la secție."),
		NSLOCTEXT("MurdarFrontEnd", "Tip4", "Pe ploaie, asfaltul nu mai ține. Frânează din timp."),
		NSLOCTEXT("MurdarFrontEnd", "Tip5", "Cine te urmărește prin oglindă nu se uită înapoi.")
	};
	UPROPERTY(EditAnywhere, config, Category = "Loading") TSoftObjectPtr<UTexture2D> LoadingBackground;
};
