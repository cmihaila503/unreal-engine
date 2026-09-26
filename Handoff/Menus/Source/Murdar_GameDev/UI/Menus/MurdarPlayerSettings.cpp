#include "UI/Menus/MurdarPlayerSettings.h"

#include "UI/Menus/MenuSettings.h"

#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

MurdarMenu::FSettings UMurdarPlayerSettings::Read() const
{
	MurdarMenu::FSettings S;
	if (const UGameUserSettings* G = UGameUserSettings::GetGameUserSettings())
	{
		S.Quality = FMath::Clamp(G->GetOverallScalabilityLevel(), 0, 3); // -1 (custom) shows as Epic... ADAPT if needed
		const EWindowMode::Type Mode = G->GetFullscreenMode();
		S.WindowMode = Mode == EWindowMode::Fullscreen ? 0 : (Mode == EWindowMode::WindowedFullscreen ? 1 : 2);
		const FIntPoint R = G->GetScreenResolution();
		S.Resolution = { R.X, R.Y };
		S.bVSync = G->IsVSyncEnabled();
		const float Limit = G->GetFrameRateLimit();
		S.FrameLimit = Limit <= 0.f ? 0 : Limit <= 30.f ? 1 : Limit <= 60.f ? 2 : Limit <= 120.f ? 3 : 4;
	}
	S.Master = MasterVolume; S.Music = MusicVolume; S.Effects = EffectsVolume; S.Voices = VoicesVolume;
	S.Sensitivity = MouseSensitivity; S.bInvertY = bInvertY; S.SubtitleSize = SubtitleSize;
	return S;
}

void UMurdarPlayerSettings::WriteAndApply(const MurdarMenu::FSettings& S, UWorld* World)
{
	if (UGameUserSettings* G = UGameUserSettings::GetGameUserSettings())
	{
		G->SetOverallScalabilityLevel(S.Quality);
		G->SetFullscreenMode(S.WindowMode == 0 ? EWindowMode::Fullscreen : (S.WindowMode == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed));
		G->SetScreenResolution(FIntPoint(S.Resolution.W, S.Resolution.H));
		G->SetVSyncEnabled(S.bVSync);
		G->SetFrameRateLimit(float(MurdarMenu::FrameLimitValue(S.FrameLimit)));
		G->ApplySettings(/*bCheckForCommandLineOverrides*/ false); // also saves GameUserSettings.ini
	}
	MasterVolume = S.Master; MusicVolume = S.Music; EffectsVolume = S.Effects; VoicesVolume = S.Voices;
	MouseSensitivity = S.Sensitivity; bInvertY = S.bInvertY; SubtitleSize = S.SubtitleSize;
	SaveConfig();
	ApplyAudio(World);
}

void UMurdarPlayerSettings::ApplyAudio(UWorld* World) const
{
	const UMenuSettings* M = GetDefault<UMenuSettings>();
	USoundMix* Mix = M->VolumeMix.LoadSynchronous();
	if (!World || !Mix) { return; } // no mix configured yet: sliders are saved but inaudible (README §Content)
	auto Set = [World, Mix](const TSoftObjectPtr<USoundClass>& Class, float Volume)
	{
		if (USoundClass* C = Class.LoadSynchronous())
		{
			// Not applied to children: each class gets its final volume (master x its own), so the order doesn't matter.
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, C, FMath::Clamp(Volume, 0.f, 1.f), 1.f, 0.f, /*bApplyToChildren*/ false);
		}
	};
	Set(M->MasterClass, MasterVolume);
	Set(M->MusicClass, MasterVolume * MusicVolume);
	Set(M->EffectsClass, MasterVolume * EffectsVolume);
	Set(M->VoicesClass, MasterVolume * VoicesVolume);
	UGameplayStatics::PushSoundMixModifier(World, Mix);
}
