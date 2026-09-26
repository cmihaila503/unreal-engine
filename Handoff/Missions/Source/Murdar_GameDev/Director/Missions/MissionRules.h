// Missions — the rules, pure C++17 (unit-tested in Handoff/Missions/Tests). Tags are plain strings here ("Event.Trigger")
// matched by the gameplay-tag hierarchy rule (a parent matches its children); the engine side converts FGameplayTags.
//
// A mission is an ordered list of objectives. An objective completes when every condition it names holds at once:
//   an event seen while it was active (latched: an event is a moment, the rest are states),
//   facts present (zones are facts too — the zone trigger system makes "player inside Zone.X" a fact),
//   the player within a radius of a point.
// The mission fails on a fail event, a fail fact, or a time limit (the mission's or the objective's).

#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace MurdarMission
{
	/** Gameplay-tag hierarchy match: "A.B" matches "A.B" and "A.B.C", never "A.BC". Empty Wanted matches nothing. */
	inline bool TagMatches(const std::string& Actual, const std::string& Wanted)
	{
		if (Wanted.empty() || Actual.size() < Wanted.size() || Actual.compare(0, Wanted.size(), Wanted) != 0)
		{
			return false;
		}
		return Actual.size() == Wanted.size() || Actual[Wanted.size()] == '.';
	}

	struct FVec { float X = 0.f, Y = 0.f, Z = 0.f; };

	struct FObjectiveSpec
	{
		std::string Id;
		std::string CompleteOnEvent;            // empty = no event needed
		std::string CompleteOnPayload;          // empty = any payload
		std::vector<std::string> RequiredFacts; // zones included
		bool bReach = false;
		FVec ReachLocation;
		float ReachRadiusCm = 500.f;
		float TimeLimitSeconds = 0.f;           // 0 = none
	};

	struct FMissionSpec
	{
		std::string Id;
		std::vector<FObjectiveSpec> Objectives;
		std::vector<std::string> FailOnEvents;
		std::vector<std::string> FailIfFacts;
		float TimeLimitSeconds = 0.f;
	};

	enum class EState { Idle, Running, Succeeded, Failed };

	enum class EStepKind { ObjectiveStarted, ObjectiveCompleted, Succeeded, Failed };

	struct FStep
	{
		EStepKind Kind;
		int Objective = -1;   // index for objective steps
		std::string Reason;   // for Failed
	};

	using FFactQuery = std::function<bool(const std::string&)>;

	class FMissionRuntime
	{
	public:
		explicit FMissionRuntime(FMissionSpec InSpec) : Spec(std::move(InSpec)) {}

		EState GetState() const { return State; }
		int GetObjective() const { return Current; }
		const FMissionSpec& GetSpec() const { return Spec; }

		std::vector<FStep> Start(double Now)
		{
			std::vector<FStep> Out;
			if (State == EState::Running)
			{
				return Out;
			}
			State = EState::Running;
			StartTime = Now;
			Current = -1;
			Advance(Now, Out);
			return Out;
		}

		/** A bus event. Fail events are checked first: dying at the finish line is still dying. */
		std::vector<FStep> OnEvent(const std::string& Tag, const std::string& Payload)
		{
			std::vector<FStep> Out;
			if (State != EState::Running)
			{
				return Out;
			}
			for (const std::string& F : Spec.FailOnEvents)
			{
				if (TagMatches(Tag, F))
				{
					Fail("event " + Tag, Out);
					return Out;
				}
			}
			const FObjectiveSpec& O = Spec.Objectives[Current];
			if (!O.CompleteOnEvent.empty() && TagMatches(Tag, O.CompleteOnEvent)
				&& (O.CompleteOnPayload.empty() || TagMatches(Payload, O.CompleteOnPayload)))
			{
				bEventLatched = true;
			}
			return Out; // completion is judged in Tick, where the states are known
		}

		/** Poll: facts, position, clocks. Call at a steady rate (the subsystem's 4 Hz). */
		std::vector<FStep> Tick(double Now, const FVec& Player, const FFactQuery& HasFact)
		{
			std::vector<FStep> Out;
			if (State != EState::Running)
			{
				return Out;
			}
			for (const std::string& F : Spec.FailIfFacts)
			{
				if (HasFact && HasFact(F))
				{
					Fail("fact " + F, Out);
					return Out;
				}
			}
			if (Spec.TimeLimitSeconds > 0.f && Now - StartTime > Spec.TimeLimitSeconds)
			{
				Fail("mission time", Out);
				return Out;
			}
			const FObjectiveSpec& O = Spec.Objectives[Current];
			if (O.TimeLimitSeconds > 0.f && Now - ObjectiveStart > O.TimeLimitSeconds)
			{
				Fail("objective time: " + O.Id, Out);
				return Out;
			}
			if (IsComplete(O, Player, HasFact))
			{
				Out.push_back({EStepKind::ObjectiveCompleted, Current, ""});
				Advance(Now, Out);
			}
			return Out;
		}

		/** Give up (player abandons, a cheat, the chapter changes). Counts as a failure with a reason. */
		std::vector<FStep> Abort(const std::string& Reason)
		{
			std::vector<FStep> Out;
			if (State == EState::Running)
			{
				Fail(Reason, Out);
			}
			return Out;
		}

	private:
		bool IsComplete(const FObjectiveSpec& O, const FVec& P, const FFactQuery& HasFact) const
		{
			if (!O.CompleteOnEvent.empty() && !bEventLatched)
			{
				return false;
			}
			for (const std::string& F : O.RequiredFacts)
			{
				if (!HasFact || !HasFact(F))
				{
					return false;
				}
			}
			if (O.bReach)
			{
				const float DX = P.X - O.ReachLocation.X, DY = P.Y - O.ReachLocation.Y, DZ = P.Z - O.ReachLocation.Z;
				if (DX * DX + DY * DY + DZ * DZ > O.ReachRadiusCm * O.ReachRadiusCm)
				{
					return false;
				}
			}
			return true;
		}

		void Advance(double Now, std::vector<FStep>& Out)
		{
			++Current;
			bEventLatched = false;
			ObjectiveStart = Now;
			if (Current >= int(Spec.Objectives.size()))
			{
				State = EState::Succeeded;
				Out.push_back({EStepKind::Succeeded, -1, ""});
				return;
			}
			Out.push_back({EStepKind::ObjectiveStarted, Current, ""});
		}

		void Fail(const std::string& Why, std::vector<FStep>& Out)
		{
			State = EState::Failed;
			Out.push_back({EStepKind::Failed, Current, Why});
		}

		FMissionSpec Spec;
		EState State = EState::Idle;
		int Current = -1;
		double StartTime = 0.0;
		double ObjectiveStart = 0.0;
		bool bEventLatched = false;
	};
}
