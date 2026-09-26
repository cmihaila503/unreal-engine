// Menus — the logic under the pause menu and the settings page. Pure C++17, unit-tested (Handoff/Menus/Tests).
// Slate draws; these decide: where focus goes (keyboard / gamepad, skipping disabled rows), what an option does on
// left / right, whether saving is allowed now and why not, which resolutions to offer, and whether the settings page
// has unsaved changes.

#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace MurdarMenu
{
	// ---------------------------------------------------------------- focus

	/** Next enabled row in Dir (+1 / -1), wrapping. Current stays when nothing else is enabled; -1 when none at all. */
	inline int MoveFocus(const std::vector<bool>& Enabled, int Current, int Dir)
	{
		const int N = int(Enabled.size());
		if (N == 0) { return -1; }
		const int Step = Dir >= 0 ? 1 : -1;
		int I = (Current < 0 || Current >= N) ? (Step > 0 ? -1 : N) : Current;
		for (int k = 0; k < N; ++k)
		{
			I = ((I + Step) % N + N) % N;
			if (Enabled[I]) { return I; }
		}
		return (Current >= 0 && Current < N && Enabled[Current]) ? Current : -1;
	}

	/** First enabled row (when a page opens). */
	inline int FirstFocus(const std::vector<bool>& Enabled) { return MoveFocus(Enabled, -1, +1); }

	// ---------------------------------------------------------------- option values

	/** A choice among N named values (quality Low..Epic, On/Off): left / right step, no wrap (the ends are the ends). */
	inline int StepChoice(int Index, int Count, int Dir)
	{
		if (Count <= 0) { return 0; }
		return std::clamp(Index + (Dir >= 0 ? 1 : -1), 0, Count - 1);
	}

	/** A slider: step, clamp, and snap to the step so repeated presses don't drift (0.1 + 0.1 + 0.1 != 0.3). */
	inline float StepSlider(float Value, float Min, float Max, float Step, int Dir)
	{
		if (Step <= 0.f) { return std::clamp(Value, Min, Max); }
		const float Next = Value + (Dir >= 0 ? Step : -Step);
		const float Snapped = Min + std::round((Next - Min) / Step) * Step;
		return std::clamp(Snapped, Min, Max);
	}

	/** "80 %" for 0..1 sliders. */
	inline std::string Percent(float Value01) { return std::to_string(int(std::lround(std::clamp(Value01, 0.f, 1.f) * 100.f))) + " %"; }

	// ---------------------------------------------------------------- saving

	enum class ESaveBlock { None, Wanted, Mission, Combat, Talking, Driving };

	struct FSaveContext
	{
		int Wanted = 0;           // EWantedLevel
		bool bMissionActive = false;
		float SinceShot = 1e9f;
		bool bTalking = false;
		bool bDrivingFast = false; // saving at 90 km/h and waking up parked in a wall is not a save
	};

	/** GTA rule: you save when nothing is going on. The first reason wins, in this order. */
	inline ESaveBlock CanSave(const FSaveContext& C, float CombatWindow = 20.f)
	{
		if (C.Wanted > 0) { return ESaveBlock::Wanted; }
		if (C.SinceShot < CombatWindow) { return ESaveBlock::Combat; }
		if (C.bMissionActive) { return ESaveBlock::Mission; }
		if (C.bTalking) { return ESaveBlock::Talking; }
		if (C.bDrivingFast) { return ESaveBlock::Driving; }
		return ESaveBlock::None;
	}

	// ---------------------------------------------------------------- resolutions

	struct FRes
	{
		int W = 0, H = 0;
		bool operator==(const FRes& O) const { return W == O.W && H == O.H; }
	};

	/** What the settings page offers: unique, at least MinH tall, smallest first; the current one always included. */
	inline std::vector<FRes> OfferedResolutions(std::vector<FRes> All, FRes Current, int MinH = 720)
	{
		std::vector<FRes> Out;
		All.push_back(Current);
		for (const FRes& R : All)
		{
			if (R.W <= 0 || R.H <= 0) { continue; }
			if (R.H < MinH && !(R == Current)) { continue; }
			if (std::find(Out.begin(), Out.end(), R) == Out.end()) { Out.push_back(R); }
		}
		std::sort(Out.begin(), Out.end(), [](const FRes& A, const FRes& B) { return A.W != B.W ? A.W < B.W : A.H < B.H; });
		return Out;
	}

	inline int IndexOf(const std::vector<FRes>& List, FRes R)
	{
		for (int i = 0; i < int(List.size()); ++i) { if (List[i] == R) { return i; } }
		return -1;
	}

	inline std::string Format(FRes R) { return std::to_string(R.W) + " x " + std::to_string(R.H); }

	// ---------------------------------------------------------------- the settings page

	struct FSettings
	{
		int Quality = 2;          // 0 low .. 3 epic (scalability overall)
		int WindowMode = 0;       // 0 fullscreen, 1 windowed fullscreen, 2 windowed
		FRes Resolution{ 1920, 1080 };
		bool bVSync = true;
		int FrameLimit = 0;       // index into {unlimited, 30, 60, 120, 144}
		float Master = 1.f, Music = 0.8f, Effects = 1.f, Voices = 1.f;
		float Sensitivity = 1.f;  // 0.2 .. 3
		bool bInvertY = false;
		int SubtitleSize = 1;     // 0 small, 1 normal, 2 large

		bool operator==(const FSettings& O) const
		{
			auto Eq = [](float A, float B) { return std::fabs(A - B) < 1e-4f; };
			return Quality == O.Quality && WindowMode == O.WindowMode && Resolution == O.Resolution && bVSync == O.bVSync
				&& FrameLimit == O.FrameLimit && Eq(Master, O.Master) && Eq(Music, O.Music) && Eq(Effects, O.Effects)
				&& Eq(Voices, O.Voices) && Eq(Sensitivity, O.Sensitivity) && bInvertY == O.bInvertY && SubtitleSize == O.SubtitleSize;
		}
		bool operator!=(const FSettings& O) const { return !(*this == O); }
	};

	/** Changes that need a confirm-or-revert countdown (a resolution the monitor can't show must fix itself). */
	inline bool NeedsDisplayConfirm(const FSettings& Applied, const FSettings& Draft)
	{
		return !(Applied.Resolution == Draft.Resolution) || Applied.WindowMode != Draft.WindowMode;
	}

	inline int FrameLimitValue(int Index)
	{
		static const int V[] = { 0, 30, 60, 120, 144 };
		return V[std::clamp(Index, 0, 4)];
	}
}
