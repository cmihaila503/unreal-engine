// Project Settings > Game > Murdar Rearview. Every tunable of "Paranoia în retrovizoare" lives here, with its unit and
// reason (police spec §1.11). The rules themselves are MurdarRearview:: in RearviewRules.h (unit-tested); ToXxx()
// hands them these values.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "AI/Rearview/RearviewRules.h"
#include "RearviewSettings.generated.h"

class UVehicleDefinition;
class UMaterialInterface;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Rearview"))
class MURDAR_GAMEDEV_API URearviewSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ---------------------------------------------------------------- Night
	/** There is no time of day yet: a chapter set at night turns this on (or MurdarNight 1). */
	UPROPERTY(EditAnywhere, config, Category = "Night")
	bool bForceNight = false;

	/** Also call it night when the level's sun is below the horizon (the first directional light's pitch). */
	UPROPERTY(EditAnywhere, config, Category = "Night")
	bool bNightFromSun = true;

	/** The sun counts as down above this pitch: a sun above the horizon points downward (negative pitch). */
	UPROPERTY(EditAnywhere, config, Category = "Night", meta = (Units = "deg", ClampMin = "-30", ClampMax = "30"))
	float SunDownPitchDeg = -2.f;

	// ---------------------------------------------------------------- Player tests
	UPROPERTY(EditAnywhere, config, Category = "Brake check", meta = (Units = "km/h", ClampMin = "10", ClampMax = "120"))
	float BrakeMinStartKph = 35.f;
	/** ~0.55 g: harder than any ordinary stop. */
	UPROPERTY(EditAnywhere, config, Category = "Brake check", meta = (ClampMin = "2", ClampMax = "12"))
	float BrakeMinDecelMps2 = 5.5f;
	UPROPERTY(EditAnywhere, config, Category = "Brake check", meta = (Units = "km/h", ClampMin = "3", ClampMax = "60"))
	float BrakeMinDropKph = 12.f;
	/** A tap, not a stop. */
	UPROPERTY(EditAnywhere, config, Category = "Brake check", meta = (Units = "s", ClampMin = "0.3", ClampMax = "4"))
	float BrakeMaxDurationSeconds = 1.6f;
	/** He must not come to a halt (that's a stop, not a check). */
	UPROPERTY(EditAnywhere, config, Category = "Brake check", meta = (Units = "km/h", ClampMin = "0", ClampMax = "60"))
	float BrakeMinEndKph = 10.f;
	UPROPERTY(EditAnywhere, config, Category = "Brake check", meta = (Units = "s", ClampMin = "0", ClampMax = "30"))
	float BrakeCooldownSeconds = 4.f;

	UPROPERTY(EditAnywhere, config, Category = "Inspection turn", meta = (Units = "deg", ClampMin = "30", ClampMax = "180"))
	float TurnMinDeg = 60.f;
	UPROPERTY(EditAnywhere, config, Category = "Inspection turn", meta = (Units = "s", ClampMin = "1", ClampMax = "8"))
	float TurnMaxSeconds = 3.5f;
	UPROPERTY(EditAnywhere, config, Category = "Inspection turn", meta = (Units = "km/h", ClampMin = "5", ClampMax = "80"))
	float TurnMinKph = 25.f;
	/** Sudden: an ordinary junction turn at 15-20 km/h is ~30 deg/s; taking a side street fast is 45+. */
	UPROPERTY(EditAnywhere, config, Category = "Inspection turn", meta = (ClampMin = "5", ClampMax = "120"))
	float TurnMinYawRateDegPerSec = 45.f;
	/** Indicating first makes it an ordinary turn. The player has no indicator input yet — see README §Manual. */
	UPROPERTY(EditAnywhere, config, Category = "Inspection turn")
	bool bTurnSignalCounts = true;
	UPROPERTY(EditAnywhere, config, Category = "Inspection turn", meta = (Units = "s", ClampMin = "0", ClampMax = "30"))
	float TurnCooldownSeconds = 6.f;

	// ---------------------------------------------------------------- Detection route: U-turn, round the block
	UPROPERTY(EditAnywhere, config, Category = "Maneuvers", meta = (Units = "deg", ClampMin = "90", ClampMax = "270"))
	float UTurnMinDeg = 150.f;
	UPROPERTY(EditAnywhere, config, Category = "Maneuvers", meta = (Units = "s", ClampMin = "3", ClampMax = "30"))
	float UTurnMaxSeconds = 10.f;
	/** Right-right-right: nobody innocent goes round a block with you. */
	UPROPERTY(EditAnywhere, config, Category = "Maneuvers", meta = (Units = "deg", ClampMin = "200", ClampMax = "720"))
	float LoopMinDeg = 300.f;
	UPROPERTY(EditAnywhere, config, Category = "Maneuvers", meta = (Units = "s", ClampMin = "20", ClampMax = "600"))
	float LoopMaxSeconds = 120.f;
	UPROPERTY(EditAnywhere, config, Category = "Maneuvers", meta = (Units = "km/h", ClampMin = "0", ClampMax = "30"))
	float ManeuverMinKph = 5.f;
	UPROPERTY(EditAnywhere, config, Category = "Maneuvers", meta = (Units = "s", ClampMin = "0", ClampMax = "60"))
	float ManeuverCooldownSeconds = 10.f;

	// ---------------------------------------------------------------- Pulling over (the stop test)
	/** He counts as stopped below this... */
	UPROPERTY(EditAnywhere, config, Category = "Stop test", meta = (Units = "km/h", ClampMin = "0", ClampMax = "20"))
	float PlayerStoppedKph = 4.f;
	/** ...for this long (a red light is shorter than a deliberate stop only sometimes — the rookie gets both wrong). */
	UPROPERTY(EditAnywhere, config, Category = "Stop test", meta = (Units = "s", ClampMin = "1", ClampMax = "30"))
	float PlayerStoppedSeconds = 4.f;
	/** A pro drives past and parks this far ahead of him, lights off. */
	UPROPERTY(EditAnywhere, config, Category = "Stop test", meta = (Units = "cm", ClampMin = "2000", ClampMax = "30000"))
	float ParkAheadCm = 8000.f;
	UPROPERTY(EditAnywhere, config, Category = "Stop test", meta = (Units = "km/h", ClampMin = "10", ClampMax = "90"))
	float ParkAheadKph = 40.f;
	/** He's moving again above this: the parked pro lets him pass, then pulls out behind. */
	UPROPERTY(EditAnywhere, config, Category = "Stop test", meta = (Units = "km/h", ClampMin = "5", ClampMax = "60"))
	float PlayerResumedKph = 15.f;

	// ---------------------------------------------------------------- What a follower can see
	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "cm", ClampMin = "2000", ClampMax = "40000"))
	float DayRangeCm = 15000.f;
	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "cm", ClampMin = "2000", ClampMax = "40000"))
	float NightLitRangeCm = 12000.f;
	/** Lights off at night: only a shape, only this close. */
	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "cm", ClampMin = "300", ClampMax = "10000"))
	float NightDarkRangeCm = 1800.f;
	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "cm", ClampMin = "0", ClampMax = "10000"))
	float HighBeamBonusCm = 1500.f;
	/** Lights off but braking: the brake lights give him away this far (use the handbrake to stay dark). */
	UPROPERTY(EditAnywhere, config, Category = "Visibility", meta = (Units = "cm", ClampMin = "1000", ClampMax = "30000"))
	float NightBrakeLightRangeCm = 9000.f;

	// ---------------------------------------------------------------- Exposure ("he's made me")
	UPROPERTY(EditAnywhere, config, Category = "Exposure", meta = (ClampMin = "0", ClampMax = "1"))
	float ExposureFollowedTurn = 0.40f;
	UPROPERTY(EditAnywhere, config, Category = "Exposure", meta = (ClampMin = "0", ClampMax = "1"))
	float ExposureHeldOnBrakeCheck = 0.25f;
	UPROPERTY(EditAnywhere, config, Category = "Exposure", meta = (ClampMin = "0", ClampMax = "1"))
	float ExposureRushedAfterDark = 0.35f;
	UPROPERTY(EditAnywhere, config, Category = "Exposure", meta = (ClampMin = "0", ClampMax = "1"))
	float ExposureFollowedUTurn = 0.70f;
	/** One loop round the block blows any follower (>= ExposureBlownAt). */
	UPROPERTY(EditAnywhere, config, Category = "Exposure", meta = (ClampMin = "0", ClampMax = "1"))
	float ExposureFollowedLoop = 1.0f;
	UPROPERTY(EditAnywhere, config, Category = "Exposure", meta = (ClampMin = "0", ClampMax = "1"))
	float ExposureStoppedBehind = 0.30f;
	UPROPERTY(EditAnywhere, config, Category = "Exposure", meta = (ClampMin = "0", ClampMax = "0.1"))
	float ExposureDecayPerSecond = 0.004f;
	UPROPERTY(EditAnywhere, config, Category = "Exposure", meta = (ClampMin = "0.1", ClampMax = "1"))
	float ExposureBlownAt = 0.8f;

	// ---------------------------------------------------------------- Who starts a tail, and when
	/** Chance per minute that a tail starts while every condition holds (night, driving, no chase, cooldown over). */
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (ClampMin = "0", ClampMax = "1"))
	float TailChancePerMinute = 0.25f;
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (Units = "s", ClampMin = "30", ClampMax = "1800"))
	float TailCooldownSeconds = 240.f;
	/** Only on a moving car: a parked player isn't "on the road". */
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (Units = "km/h", ClampMin = "0", ClampMax = "100"))
	float TailMinPlayerKph = 30.f;
	/** A tail gives up by itself after this — a follower can't sit behind you all night. */
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (Units = "s", ClampMin = "30", ClampMax = "1200"))
	float TailMaxSeconds = 300.f;

	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (ClampMin = "0", ClampMax = "10"))
	float UndercoverBaseWeight = 0.3f;
	/** Added while the police suspect him (wanted = Stop, or they have a description of his car) but have no chase on. */
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (ClampMin = "0", ClampMax = "10"))
	float UndercoverSuspicionWeight = 1.5f;
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (ClampMin = "0", ClampMax = "10"))
	float GangBaseWeight = 0.2f;
	/** An ordinary traffic car put behind him on purpose. Night roads are empty: without decoys every car in the mirror
	 *  would be a follower, and there is no paranoia without false alarms. */
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (ClampMin = "0", ClampMax = "10"))
	float CivilianDecoyWeight = 1.0f;
	/** A story fact that puts a gang on his trail (set by the chapter / a trigger). */
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (Categories = "Fact"))
	FGameplayTag GangHuntingFact;
	UPROPERTY(EditAnywhere, config, Category = "Director", meta = (ClampMin = "0", ClampMax = "10"))
	float GangFactWeight = 2.f;

	// ---------------------------------------------------------------- Spawning the follower
	/** Placed this far behind the player, on his lane, out of his view. */
	UPROPERTY(EditAnywhere, config, Category = "Spawn", meta = (Units = "cm", ClampMin = "3000", ClampMax = "30000"))
	float SpawnBehindCm = 12000.f;
	UPROPERTY(EditAnywhere, config, Category = "Spawn", meta = (Units = "cm", ClampMin = "500", ClampMax = "5000"))
	float SpawnLaneSearchCm = 2000.f;
	/** Unmarked car / gang car. Empty = a random traffic definition (a follower looks like anybody). */
	UPROPERTY(EditAnywhere, config, Category = "Spawn")
	TSoftObjectPtr<UVehicleDefinition> UndercoverCar;
	UPROPERTY(EditAnywhere, config, Category = "Spawn")
	TSoftObjectPtr<UVehicleDefinition> GangCar;

	// ---------------------------------------------------------------- How each follower drives
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "cm", ClampMin = "1000", ClampMax = "10000"))
	float UndercoverFollowCm = 4000.f;
	/** Thugs sit closer: they aren't hiding it much. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "cm", ClampMin = "500", ClampMax = "10000"))
	float GangFollowCm = 2200.f;
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (ClampMin = "0", ClampMax = "1"))
	float UndercoverSkillMin = 0.3f;
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (ClampMin = "0", ClampMax = "1"))
	float UndercoverSkillMax = 0.9f;
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (ClampMin = "0", ClampMax = "1"))
	float GangSkillMax = 0.4f;
	/** A follower reacts to the player's tests only within this distance. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "cm", ClampMin = "1000", ClampMax = "20000"))
	float ReactRangeCm = 7000.f;
	/** Brake check: the pro drops back by this factor, calmly, for BrakeReactSeconds. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (ClampMin = "1", ClampMax = "3"))
	float UndercoverBackOffScale = 1.5f;
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "s", ClampMin = "1", ClampMax = "20"))
	float BrakeReactSeconds = 5.f;
	/** People don't react on the same tick: each reaction waits a random delay in this range (skill shortens it). */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "s", ClampMin = "0", ClampMax = "3"))
	float ReactDelayMinSeconds = 0.4f;
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "s", ClampMin = "0", ClampMax = "5"))
	float ReactDelayMaxSeconds = 1.3f;
	/** Brake check: the thug comes up alongside and stays there this long. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "s", ClampMin = "1", ClampMax = "20"))
	float GangAlongsideSeconds = 4.f;
	/** Lost sight of him: how long before he counts as lost (a bend hides a car for a second). */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "s", ClampMin = "0.5", ClampMax = "10"))
	float LoseGraceSeconds = 1.5f;
	/** Lost: high beams on, flat out to where he should be, for this long, then give up. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "s", ClampMin = "3", ClampMax = "60"))
	float SearchSeconds = 15.f;
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "km/h", ClampMin = "40", ClampMax = "200"))
	float SearchKph = 110.f;
	/** Dead reckoning cap for where to search. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "s", ClampMin = "0", ClampMax = "10"))
	float SearchPredictSeconds = 3.f;
	/** High beams: headlight intensity × this and reach × HighBeamReachScale. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (ClampMin = "1", ClampMax = "10"))
	float HighBeamIntensityScale = 2.5f;
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (ClampMin = "1", ClampMax = "5"))
	float HighBeamReachScale = 2.f;
	/** Undercover with the police already suspecting him: tell the radio where he is, this often. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "s", ClampMin = "1", ClampMax = "60"))
	float UndercoverReportSeconds = 6.f;
	/** Breaking off: drive on this far, then vanish once he can't see us. */
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "cm", ClampMin = "2000", ClampMax = "50000"))
	float BreakOffDriveCm = 20000.f;
	UPROPERTY(EditAnywhere, config, Category = "Follower", meta = (Units = "cm", ClampMin = "5000", ClampMax = "60000"))
	float DespawnDistanceCm = 20000.f;

	// ---------------------------------------------------------------- Civilians behind him
	/** Brake check: the civilian car behind within this range leans on the horn. */
	UPROPERTY(EditAnywhere, config, Category = "Civilians", meta = (Units = "cm", ClampMin = "500", ClampMax = "6000"))
	float CivilianHonkRangeCm = 2500.f;
	UPROPERTY(EditAnywhere, config, Category = "Civilians", meta = (Units = "s", ClampMin = "0.1", ClampMax = "3"))
	float CivilianHonkSeconds = 0.8f;
	/** Further back but still close: a 1990s driver flashes his lights instead (two short blinks). */
	UPROPERTY(EditAnywhere, config, Category = "Civilians", meta = (Units = "cm", ClampMin = "1000", ClampMax = "15000"))
	float CivilianFlashRangeCm = 6000.f;
	UPROPERTY(EditAnywhere, config, Category = "Civilians", meta = (Units = "s", ClampMin = "0.05", ClampMax = "1"))
	float CivilianFlashBlinkSeconds = 0.15f;

	// ---------------------------------------------------------------- Mirror view
	/** The mirror camera sits just behind the car's body (placed from its bounds), at this height, looking back — inside
	 *  the cabin it would look through the car's own mesh. The mask material frames it as the interior mirror. */
	UPROPERTY(EditAnywhere, config, Category = "Mirror", meta = (Units = "cm", ClampMin = "0", ClampMax = "200"))
	float MirrorBehindBodyCm = 30.f;
	UPROPERTY(EditAnywhere, config, Category = "Mirror", meta = (Units = "cm", ClampMin = "50", ClampMax = "250"))
	float MirrorHeightCm = 120.f;
	UPROPERTY(EditAnywhere, config, Category = "Mirror", meta = (Units = "deg", ClampMin = "10", ClampMax = "90"))
	float MirrorFovDeg = 32.f;
	/** Engine volume while looking in the mirror: to hear the car behind. */
	UPROPERTY(EditAnywhere, config, Category = "Mirror", meta = (ClampMin = "0", ClampMax = "1"))
	float MirrorEngineVolume = 0.35f;
	UPROPERTY(EditAnywhere, config, Category = "Mirror", meta = (ClampMin = "0", ClampMax = "2"))
	float MirrorVignette = 1.2f;
	/** Optional post-process material: the mirror's frame and a left-right flip (manual asset, see README). */
	UPROPERTY(EditAnywhere, config, Category = "Mirror")
	TSoftObjectPtr<UMaterialInterface> MirrorMaskMaterial;

	// ---------------------------------------------------------------- Builders for the pure rules
	MurdarRearview::FBrakeCheckConfig ToBrakeCheck() const
	{
		MurdarRearview::FBrakeCheckConfig C;
		C.MinStartKph = BrakeMinStartKph; C.MinDecelMps2 = BrakeMinDecelMps2; C.MinDropKph = BrakeMinDropKph;
		C.MaxDurationSeconds = BrakeMaxDurationSeconds; C.MinEndKph = BrakeMinEndKph; C.CooldownSeconds = BrakeCooldownSeconds;
		return C;
	}
	MurdarRearview::FInspectionTurnConfig ToInspectionTurn() const
	{
		MurdarRearview::FInspectionTurnConfig C;
		C.MinTurnDeg = TurnMinDeg; C.MaxTurnSeconds = TurnMaxSeconds; C.MinKph = TurnMinKph;
		C.MinYawRateDegPerSec = TurnMinYawRateDegPerSec; C.bSignalCounts = bTurnSignalCounts; C.CooldownSeconds = TurnCooldownSeconds;
		return C;
	}
	MurdarRearview::FManeuverConfig ToManeuver() const
	{
		MurdarRearview::FManeuverConfig C;
		C.UTurnMinDeg = UTurnMinDeg; C.UTurnMaxSeconds = UTurnMaxSeconds; C.LoopMinDeg = LoopMinDeg;
		C.LoopMaxSeconds = LoopMaxSeconds; C.MinKph = ManeuverMinKph; C.CooldownSeconds = ManeuverCooldownSeconds;
		return C;
	}
	MurdarRearview::FVisibilityConfig ToVisibility() const
	{
		MurdarRearview::FVisibilityConfig C;
		C.DayRangeCm = DayRangeCm; C.NightLitRangeCm = NightLitRangeCm; C.NightDarkRangeCm = NightDarkRangeCm; C.HighBeamBonusCm = HighBeamBonusCm;
		C.NightBrakeLightRangeCm = NightBrakeLightRangeCm;
		return C;
	}
	MurdarRearview::FExposureConfig ToExposure() const
	{
		MurdarRearview::FExposureConfig C;
		C.FollowedTurn = ExposureFollowedTurn; C.HeldOnBrakeCheck = ExposureHeldOnBrakeCheck; C.RushedAfterDark = ExposureRushedAfterDark;
		C.FollowedUTurn = ExposureFollowedUTurn; C.FollowedLoop = ExposureFollowedLoop; C.StoppedBehind = ExposureStoppedBehind;
		C.DecayPerSecond = ExposureDecayPerSecond; C.BlownAt = ExposureBlownAt;
		return C;
	}
};
