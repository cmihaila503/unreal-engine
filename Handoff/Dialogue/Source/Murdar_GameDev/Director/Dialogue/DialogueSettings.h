// Project Settings > Game > Murdar Dialogue. Choice keys, barks (police lines and other one-liners), distances.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "DialogueSettings.generated.h"

class USoundBase;
class USoundAttenuation;

USTRUCT(BlueprintType)
struct FDialogueVoiceSet
{
	GENERATED_BODY()

	/** Variants of the same line; never the same one twice in a row. */
	UPROPERTY(EditAnywhere, config, Category = "Voice") TArray<TSoftObjectPtr<USoundBase>> Variants;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Dialogue"))
class MURDAR_GAMEDEV_API UDialogueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Choice N is picked with ChoiceKeys[N] or GamepadChoiceKeys[N]. ADAPT: keys not bound to anything else while
	 *  driving (on foot, 1/2 equip weapons: README §Patches 4 mutes them while choosing). */
	UPROPERTY(EditAnywhere, config, Category = "Choices")
	TArray<FKey> ChoiceKeys = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };

	UPROPERTY(EditAnywhere, config, Category = "Choices")
	TArray<FKey> GamepadChoiceKeys = { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left };

	/** Voices for bark tags (Police.Line.*, Bark.*): played at the event's source. The HUD already subtitles police lines. */
	UPROPERTY(EditAnywhere, config, Category = "Barks", meta = (ForceInlineRow))
	TMap<FGameplayTag, FDialogueVoiceSet> BarkVoices;

	/** The same bark tag is voiced at most once per this (a unit repeating an order is heard, not spammed). */
	UPROPERTY(EditAnywhere, config, Category = "Barks", meta = (Units = "s", ClampMin = "0", ClampMax = "60"))
	float BarkCooldownSeconds = 4.f;

	/** Attenuation for voices in the world (conversation lines and barks). Empty = the sound's own. */
	UPROPERTY(EditAnywhere, config, Category = "Voice")
	TSoftObjectPtr<USoundAttenuation> VoiceAttenuation;

	/** Added to a voice file's length so lines don't butt into each other. */
	UPROPERTY(EditAnywhere, config, Category = "Voice", meta = (Units = "s", ClampMin = "0", ClampMax = "1"))
	float VoiceTailSeconds = 0.2f;
};
