// Car weight — why the car drives heavy but flies and crashes like 10 kg, and the rules that fix it. Pure C++17,
// unit-tested (Handoff/CarWeight/Tests). Written against the real source (27–30 Sep):
//  - MassKg 1550, inertia fine: the mass is not the problem;
//  - the only extra vertical force is UVehicleEffectsComponent::UpdateDownforce - speed-squared, on the ground and in
//    the air alike, nothing that makes a jump end sooner;
//  - no physical material override on the body -> the engine default bounces the car off walls;
//  - max depenetration velocity not overridden -> a car nudged into a wall is spat out;
//  - NotifyImpact already measures the delta-v and plays a shake + sound; nothing stops the frame on a hard hit;
//  - the camera (FOV 75 -> 92, arm 5.5 -> 7 m) makes the car look small, and small reads light.
// The Unreal side (UCarWeightComponent, the README patches, Tools/scale_audit.py) only calls these.

#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace MurdarWeight
{
	constexpr float G = 980.f; // cm/s^2

	struct FAirTuning
	{
		float AirborneAfterSeconds = 0.08f;  // all wheels off this long = in the air (a bump is not a jump)
		float GravityMultiplier = 1.8f;      // total gravity while airborne (1 = real)
		float RampSeconds = 0.25f;           // the extra gravity builds up over this (a crest does not slam down)
		float PitchRollDamping = 2.5f;       // 1/s: how fast pitch/roll spin dies in the air (yaw untouched)
		float LevelingTorque = 0.6f;         // 1/s^2 per radian of tilt: a gentle hand keeping it wheels-down
		float MaxLevelTiltDeg = 50.f;        // beyond this it is a real roll-over: no help
		float SettleSeconds = 0.35f;         // after landing, bounces are damped this long
		float SettleDamping = 6.f;           // 1/s on upward vertical speed during the settle
	};

	/** Airborne tracker: feed it every physics tick with how many wheels touch the ground. */
	struct FAirState
	{
		float OffFor = 0.f;         // seconds with no wheel down
		float AirTime = 0.f;        // seconds since it counted as airborne (0 on the ground)
		float SinceLanding = 1e6f;  // seconds since the last landing
		bool bAirborne = false;
		float LastAirTime = 0.f;    // how long the last jump lasted (for the landing feel)

		/** Returns true on the tick it lands. */
		bool Update(int WheelsDown, float Dt, const FAirTuning& T)
		{
			bool bLanded = false;
			if (WheelsDown > 0)
			{
				if (bAirborne) { bLanded = true; LastAirTime = AirTime; SinceLanding = 0.f; }
				else { SinceLanding += Dt; }
				OffFor = 0.f; AirTime = 0.f; bAirborne = false;
			}
			else
			{
				OffFor += Dt;
				SinceLanding += Dt;
				if (!bAirborne && OffFor >= T.AirborneAfterSeconds) { bAirborne = true; AirTime = 0.f; }
				if (bAirborne) { AirTime += Dt; }
			}
			return bLanded;
		}
	};

	/** Extra downward acceleration (cm/s^2, >= 0) to add on top of real gravity while airborne. */
	inline float ExtraGravity(const FAirState& S, const FAirTuning& T)
	{
		if (!S.bAirborne || T.GravityMultiplier <= 1.f) { return 0.f; }
		const float Ramp = T.RampSeconds > 0.f ? std::clamp(S.AirTime / T.RampSeconds, 0.f, 1.f) : 1.f;
		return (T.GravityMultiplier - 1.f) * G * Ramp;
	}

	/**
	 * Angular acceleration (rad/s^2) to apply about the car's own pitch and roll axes while airborne: damp the spin,
	 * nudge towards level. Yaw is never touched (it is the player's). Off beyond MaxLevelTiltDeg for the leveling part:
	 * a car really rolling over rolls over.
	 */
	struct FAxes { float Pitch = 0.f; float Roll = 0.f; };

	inline FAxes AirStabilize(const FAxes& RateRad, const FAxes& TiltRad, const FAirState& S, const FAirTuning& T)
	{
		if (!S.bAirborne) { return {}; }
		const float MaxTilt = T.MaxLevelTiltDeg * 3.14159265f / 180.f;
		auto Axis = [&](float Rate, float Tilt)
		{
			float A = -T.PitchRollDamping * Rate;
			if (std::fabs(Tilt) < MaxTilt) { A += -T.LevelingTorque * Tilt; }
			return A;
		};
		return { Axis(RateRad.Pitch, TiltRad.Pitch), Axis(RateRad.Roll, TiltRad.Roll) };
	}

	/**
	 * Landing settle: for SettleSeconds after touching down, an upward vertical speed (the bounce) is damped. Returns
	 * an acceleration (cm/s^2) to apply; 0 when moving down (the suspension is taking the load - leave it).
	 */
	inline float LandingSettle(float VzCms, const FAirState& S, const FAirTuning& T)
	{
		if (S.SinceLanding > T.SettleSeconds || VzCms <= 0.f) { return 0.f; }
		// Fades out over the window so the car does not stick then let go.
		const float Fade = 1.f - std::clamp(S.SinceLanding / T.SettleSeconds, 0.f, 1.f);
		return -T.SettleDamping * VzCms * Fade;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Impact feel
	// -----------------------------------------------------------------------------------------------------------------

	struct FImpactTuning
	{
		float MinDeltaVCms = 150.f;     // below: a scrape (the existing CrashMinDeltaV)
		float FullDeltaVCms = 1500.f;   // at and above: severity 1 (the existing CrashFullDeltaV)
		float HitStopMinSeverity = 0.35f;
		float HitStopMaxSeconds = 0.07f; // real seconds the world nearly stops on the hardest hit
		float HitStopDilation = 0.05f;
		float HitStopCooldown = 1.0f;    // never two in a row (a pinball against a wall)
		float ShakeMin = 0.3f, ShakeMax = 1.6f;
		float LowThumpMinSeverity = 0.2f;
	};

	struct FImpactFeel
	{
		float Severity = 0.f;       // 0..1
		float Shake = 0.f;          // camera shake scale (0 = none)
		float HitStopSeconds = 0.f; // 0 = none
		float ThumpVolume = 0.f;    // the low "weight" layer under the crash sound
	};

	/** Delta-v as NotifyImpact computes it (horizontal, and vertical halved: a hard landing counts less). */
	inline FImpactFeel ImpactFeel(float HorizontalCms, float VerticalCms, bool bPlayerCar, float SinceLastHitStop, const FImpactTuning& T)
	{
		FImpactFeel F;
		const float Dv = std::max(HorizontalCms, VerticalCms * 0.5f);
		if (Dv < T.MinDeltaVCms) { return F; }
		F.Severity = std::clamp((Dv - T.MinDeltaVCms) / std::max(T.FullDeltaVCms - T.MinDeltaVCms, 1.f), 0.f, 1.f);
		if (!bPlayerCar) { return F; } // AI cars: severity only (damage, sound); no camera, no hit-stop
		F.Shake = T.ShakeMin + (T.ShakeMax - T.ShakeMin) * F.Severity;
		if (F.Severity >= T.HitStopMinSeverity && SinceLastHitStop >= T.HitStopCooldown)
		{
			const float K = (F.Severity - T.HitStopMinSeverity) / std::max(1.f - T.HitStopMinSeverity, 1e-3f);
			F.HitStopSeconds = T.HitStopMaxSeconds * (0.4f + 0.6f * K);
		}
		F.ThumpVolume = F.Severity >= T.LowThumpMinSeverity ? 0.4f + 0.6f * F.Severity : 0.f;
		return F;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Scale audit: is it the car that is small, or the world that is big?
	// -----------------------------------------------------------------------------------------------------------------

	enum class EScale { Ok, TooSmall, TooBig };

	struct FScaleCheck
	{
		EScale Verdict = EScale::Ok;
		float Factor = 1.f;   // multiply the measured thing by this to reach the expected size
	};

	/** Measured vs expected (same unit), tolerance as a fraction (0.12 = ±12%). */
	inline FScaleCheck CheckScale(float Measured, float Expected, float Tolerance = 0.12f)
	{
		FScaleCheck C;
		if (Measured <= 0.f || Expected <= 0.f) { return C; }
		C.Factor = Expected / Measured;
		if (Measured < Expected * (1.f - Tolerance)) { C.Verdict = EScale::TooSmall; }
		else if (Measured > Expected * (1.f + Tolerance)) { C.Verdict = EScale::TooBig; }
		return C;
	}

	/** Reference sizes (cm) for the audit. */
	struct FReference { const char* Name; float Cm; };
	constexpr FReference Dacia1300Length{ "Dacia 1300 length", 435.f };
	constexpr FReference Dacia1300Width{ "Dacia 1300 width", 165.f };
	constexpr FReference Dacia1300Height{ "Dacia 1300 height", 144.f };
	constexpr FReference Person{ "person", 178.f };
	constexpr FReference Lane{ "traffic lane", 340.f };
	constexpr FReference Kerb{ "kerb", 15.f };
	constexpr FReference Storey{ "block storey", 270.f };
	constexpr FReference Door{ "door", 210.f };
	constexpr FReference DashLine{ "dashed line", 300.f };

	/**
	 * The audit's conclusion from the car and the lane: which side to fix. The car is the reference everything else is
	 * judged against, so it is fixed first; a lane is only "too wide" when the car is right.
	 */
	inline std::string Diagnose(const FScaleCheck& Car, const FScaleCheck& LaneCheck)
	{
		if (Car.Verdict != EScale::Ok) { return "car mesh scale is wrong: fix the car first (factor " + std::to_string(Car.Factor) + ")"; }
		if (LaneCheck.Verdict == EScale::TooBig) { return "roads too wide for the car: narrow the lanes (factor " + std::to_string(LaneCheck.Factor) + ")"; }
		if (LaneCheck.Verdict == EScale::TooSmall) { return "roads too narrow: widen the lanes"; }
		return "sizes are right: the small look is the camera (FOV / arm) and the empty surroundings";
	}

	/** Camera that shows a car its real size: narrower FOV, closer arm (and the stretch at speed kept modest). */
	struct FCameraTarget
	{
		float FovAtRest = 70.f, FovAtSpeed = 80.f;
		float DistanceCm = 480.f, DistanceAtSpeedCm = 560.f;
		float PivotZ = 115.f, PitchDeg = -7.f;
	};

	inline bool CameraLooksSmall(float FovAtSpeed, float DistanceAtSpeedCm)
	{
		return FovAtSpeed > 85.f || DistanceAtSpeedCm > 650.f;
	}
}
