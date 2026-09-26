// The player's choices that aren't graphics (those live in UGameUserSettings): volumes, mouse, subtitles.
// Saved per user in GameUserSettings.ini. Systems read it through Get(): the camera for sensitivity / invert, the HUD
// for subtitle size.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/MenuRules.h"
#include "MurdarPlayerSettings.generated.h"

UCLASS(config = GameUserSettings)
class MURDAR_GAMEDEV_API UMurdarPlayerSettings : public UObject
{
	GENERATED_BODY()

public:
	static UMurdarPlayerSettings* Get() { return GetMutableDefault<UMurdarPlayerSettings>(); }

	UPROPERTY(config) float MasterVolume = 1.f;
	UPROPERTY(config) float MusicVolume = 0.8f;
	UPROPERTY(config) float EffectsVolume = 1.f;
	UPROPERTY(config) float VoicesVolume = 1.f;
	UPROPERTY(config) float MouseSensitivity = 1.f;
	UPROPERTY(config) bool bInvertY = false;
	UPROPERTY(config) int32 SubtitleSize = 1;

	/** Everything the settings page edits, graphics read from UGameUserSettings. */
	MurdarMenu::FSettings Read() const;
	/** Write, apply (graphics + audio) and save both config files. */
	void WriteAndApply(const MurdarMenu::FSettings& S, UWorld* World);
	/** Audio only: on world start, so the sliders' values hold from the first frame. */
	void ApplyAudio(UWorld* World) const;
	/** For the GASP look input (Blueprint): multiply the look axis by this, negate Y when inverted. */
	UFUNCTION(BlueprintPure, Category = "Murdar|Settings") static float GetMouseSensitivity() { return Get()->MouseSensitivity; }
	UFUNCTION(BlueprintPure, Category = "Murdar|Settings") static bool GetInvertY() { return Get()->bInvertY; }

	/** 0.85 / 1 / 1.25 — for the HUD's subtitle font. */
	float SubtitleScale() const { return SubtitleSize <= 0 ? 0.85f : (SubtitleSize >= 2 ? 1.25f : 1.f); }
};
