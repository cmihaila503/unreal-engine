// Project Settings > Game > Murdar Menus (the project's choices; the player's own choices are UMurdarPlayerSettings).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "MenuSettings.generated.h"

class USoundMix;
class USoundClass;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Menus"))
class MURDAR_GAMEDEV_API UMenuSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Open / close the pause menu. Escape also stops PIE in the editor: use P there. */
	UPROPERTY(EditAnywhere, config, Category = "Input")
	TArray<FKey> OpenKeys = { EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right };

	/** The mix the volume sliders override, and the classes they drive. ADAPT: create SCM_Murdar and SC_* (README). */
	UPROPERTY(EditAnywhere, config, Category = "Audio") TSoftObjectPtr<USoundMix> VolumeMix;
	UPROPERTY(EditAnywhere, config, Category = "Audio") TSoftObjectPtr<USoundClass> MasterClass;
	UPROPERTY(EditAnywhere, config, Category = "Audio") TSoftObjectPtr<USoundClass> MusicClass;
	UPROPERTY(EditAnywhere, config, Category = "Audio") TSoftObjectPtr<USoundClass> EffectsClass;
	UPROPERTY(EditAnywhere, config, Category = "Audio") TSoftObjectPtr<USoundClass> VoicesClass;

	/** Saving is refused this long after a shot near him. */
	UPROPERTY(EditAnywhere, config, Category = "Saving", meta = (Units = "s", ClampMin = "0", ClampMax = "120")) float CombatWindowSeconds = 20.f;
	/** ...and while driving faster than this. */
	UPROPERTY(EditAnywhere, config, Category = "Saving", meta = (Units = "km/h", ClampMin = "0", ClampMax = "200")) float SaveMaxKph = 10.f;
	UPROPERTY(EditAnywhere, config, Category = "Saving") FName ManualSlot = TEXT("manual");
	UPROPERTY(EditAnywhere, config, Category = "Saving") FName AutoSlot = TEXT("auto");

	/** A new resolution / window mode reverts after this unless confirmed. */
	UPROPERTY(EditAnywhere, config, Category = "Display", meta = (Units = "s", ClampMin = "5", ClampMax = "30")) float DisplayConfirmSeconds = 12.f;
};
