// Cutscenes — when a scene may play, the queue, and skipping. Pure C++17, unit-tested (Handoff/Cutscenes/Tests).
//   A scene plays on its event when its facts hold, once if it says so, and never in the middle of a chase (it waits
//   until the heat is gone, or is dropped if it went stale). One at a time; the next waits in a short queue.
//   Skipping is a hold (a second or so), so a stray press never throws away a scene.

#pragma once

#include <algorithm>
#include <deque>
#include <string>

namespace MurdarCutscene
{
	struct FGate
	{
		bool bFactsOk = true;
		bool bOnce = false;
		bool bAlreadyPlayed = false;
		int Wanted = 0;           // EWantedLevel
		bool bInDialogue = false;
	};

	enum class EDecision { Play, Wait, Drop };

	/** Now, later (when things calm down), or never. */
	inline EDecision Decide(const FGate& G)
	{
		if (!G.bFactsOk || (G.bOnce && G.bAlreadyPlayed)) { return EDecision::Drop; }
		if (G.Wanted >= 2 || G.bInDialogue) { return EDecision::Wait; }
		return EDecision::Play;
	}

	struct FQueued { std::string Id; double Since = 0.0; };

	class FQueue
	{
	public:
		explicit FQueue(int InMax = 3, double InStaleSeconds = 120.0) : Max(InMax), Stale(InStaleSeconds) {}

		/** False when full or already queued. */
		bool Push(const std::string& Id, double Now)
		{
			if (int(Items.size()) >= Max) { return false; }
			for (const FQueued& Q : Items) { if (Q.Id == Id) { return false; } }
			Items.push_back({ Id, Now });
			return true;
		}

		/** The next scene still worth playing (stale ones dropped); "" if none. */
		std::string Pop(double Now)
		{
			while (!Items.empty())
			{
				const FQueued Q = Items.front();
				Items.pop_front();
				if (Now - Q.Since <= Stale) { return Q.Id; }
			}
			return std::string();
		}

		int Size() const { return int(Items.size()); }

	private:
		std::deque<FQueued> Items;
		int Max;
		double Stale;
	};

	/** Hold-to-skip. Update with the key's state each frame; true once when the hold completes. */
	class FSkipHold
	{
	public:
		explicit FSkipHold(float InSeconds = 1.f) : Seconds(InSeconds) {}

		bool Update(bool bHeld, float Dt)
		{
			if (!bHeld) { Held = 0.f; bFired = false; return false; }
			Held += Dt;
			if (!bFired && Held >= Seconds) { bFired = true; return true; }
			return false;
		}

		float Progress01() const { return Seconds > 0.f ? std::clamp(Held / Seconds, 0.f, 1.f) : 1.f; }

	private:
		float Seconds;
		float Held = 0.f;
		bool bFired = false;
	};
}
