// Disguise — the police know him by what he wears. Pure C++17, unit-tested (Handoff/Disguise/Tests).
// On foot, a sighting fixes a description: outfit and headwear (hat, cap, glasses). Change either while nobody from
// the police sees you, and they don't know you at a distance any more: only close, in good light, might one look
// twice. A chase lost that way cools to a stop, as a respray does for a car (Handoff/Garage).

#pragma once

#include <algorithm>

namespace MurdarDisguise
{
	struct FLookOfHim
	{
		int Outfit = 0;
		int Headwear = 0; // 0 = none
		bool operator==(const FLookOfHim& O) const { return Outfit == O.Outfit && Headwear == O.Headwear; }
	};

	struct FDescription
	{
		bool bValid = false;
		FLookOfHim Look;
	};

	struct FTuning
	{
		float CloseDay = 800.f;     // cm: this close by day, a cop may still know the face
		float CloseNight = 300.f;
		float UnseenSeconds = 5.f;  // the change must happen out of their sight
	};

	/** Does a policeman at this distance recognise him? No description yet = anyone matching is him. */
	inline bool Recognise(const FDescription& D, const FLookOfHim& Now, float DistanceCm, bool bNight, const FTuning& T)
	{
		if (!D.bValid || D.Look == Now) { return true; }
		// Changed clothes: only the face gives him away, close up.
		return DistanceCm <= (bNight ? T.CloseNight : T.CloseDay);
	}

	/** A sighting (recognised) writes the description he's wearing now. */
	inline FDescription Describe(const FLookOfHim& Now) { FDescription D; D.bValid = true; D.Look = Now; return D; }

	/** A change counts only unseen. */
	inline bool ChangeCounts(float SinceSeenByPolice, const FTuning& T) { return SinceSeenByPolice >= T.UnseenSeconds; }

	/** Same cooling as a respray: a chase goes down to just under a stop. */
	inline float HeatAfterChange(float Heat, float HeatStop, float HeatPursuit) { return Heat >= HeatPursuit ? HeatStop - 1.f : Heat; }

	/** Headwear toggles through what he owns (bit i = owns item i+1). Returns the next worn index, 0 = none. */
	inline int NextHeadwear(int Current, unsigned OwnedMask, int Count)
	{
		for (int Step = 1; Step <= Count + 1; ++Step)
		{
			const int Candidate = (Current + Step) % (Count + 1);
			if (Candidate == 0 || (OwnedMask & (1u << (Candidate - 1)))) { return Candidate; }
		}
		return 0;
	}
}
