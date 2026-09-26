#include "Director/TimeOfDay/TimeOfDaySubsystem.h"

#include "Director/TimeOfDay/TimeOfDaySettings.h"
#include "Director/GameEventSubsystem.h"
#include "Director/NarrativeStateSubsystem.h"
#include "Director/MurdarTags.h"
#include "Vehicle/MurdarVehicle.h"

#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	// Config tags (README §Patches 1): requested by name, no MurdarTags edit.
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
}

UTimeOfDaySubsystem* UTimeOfDaySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
}

bool UTimeOfDaySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTimeOfDaySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const UTimeOfDaySettings* S = GetDefault<UTimeOfDaySettings>();

	// The saved clock if there is one (a loaded game, a map change), else the settings' start hour. A chapter that
	// wants its own hour sets it after this (README §Patches 3).
	Clock = MurdarTime::FGameClock(S->StartHour * 60.f, 0);
	RestoreFromState();
	UpdateNight(/*bPublish*/ false);
	ApplySun();
	SweepHeadlights();
	// A load in the middle of play (MurdarLoad, continue) brings its own hour.
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		TWeakObjectPtr<UTimeOfDaySubsystem> Weak(this);
		LoadedHandle = Bus->Subscribe(MurdarTags::Event_State_Loaded, [Weak](const FGameEvent&)
		{
			if (UTimeOfDaySubsystem* Self = Weak.Get())
			{
				if (Self->RestoreFromState()) { Self->UpdateNight(true); Self->ApplySun(); Self->SweepHeadlights(); }
			}
		});
	}
	InWorld.GetTimerManager().SetTimer(UpdateTimer, this, &UTimeOfDaySubsystem::Update, S->UpdateIntervalSeconds, true);
}

void UTimeOfDaySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
	{
		Bus->Unsubscribe(LoadedHandle);
	}
	SaveToState(); // a map change keeps the hour
	Super::Deinitialize();
}

bool UTimeOfDaySubsystem::RestoreFromState()
{
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const FGameplayTag KMin = Tag(TEXT("Stat.TimeOfDay"));
	const FGameplayTag KDay = Tag(TEXT("Stat.Day"));
	const float Saved = State && KMin.IsValid() ? State->GetValue(KMin, -1.f) : -1.f;
	if (Saved < 0.f)
	{
		return false;
	}
	Clock.Set(Saved);
	Clock.SetDay(KDay.IsValid() ? int32(State->GetValue(KDay, 0.f)) : 0);
	MinutesSinceSave = 0.f;
	return true;
}

float UTimeOfDaySubsystem::GetLightLevel01() const
{
	return MurdarTime::LightLevel01(Clock.GetHour(), GetDefault<UTimeOfDaySettings>()->ToSun());
}

float UTimeOfDaySubsystem::GetPeopleScale() const
{
	return MurdarTime::HourlyCurve(UTimeOfDaySettings::ToCurve(GetDefault<UTimeOfDaySettings>()->PeopleByHour), Clock.GetHour());
}

float UTimeOfDaySubsystem::GetTrafficScale() const
{
	return MurdarTime::HourlyCurve(UTimeOfDaySettings::ToCurve(GetDefault<UTimeOfDaySettings>()->TrafficByHour), Clock.GetHour());
}

void UTimeOfDaySubsystem::SetHour(float Hour)
{
	Clock.Set(Hour * 60.f);
	UpdateNight(/*bPublish*/ true);
	ApplySun();
	SweepHeadlights();
	SaveToState();
}

void UTimeOfDaySubsystem::Update()
{
	const UTimeOfDaySettings* S = GetDefault<UTimeOfDaySettings>();
	const float Step = S->UpdateIntervalSeconds;
	if (!bFrozen)
	{
		const float DayLength = DayLengthOverride >= 0.f ? DayLengthOverride : S->RealSecondsPerGameDay;
		const float Before = Clock.GetMinutes();
		if (Clock.Advance(Step, DayLength))
		{
			if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
			{
				FGameEvent E; E.Tag = Tag(TEXT("Event.Time.NewDay")); E.Magnitude = float(Clock.GetDay()); Bus->Publish(E);
			}
		}
		MinutesSinceSave += MurdarTime::FGameClock::Wrap(Clock.GetMinutes() - Before);
	}
	UpdateNight(/*bPublish*/ true);
	ApplySun();
	SecondsSinceSweep += Step;
	if (SecondsSinceSweep >= S->HeadlightSweepSeconds)
	{
		SecondsSinceSweep = 0.f;
		SweepHeadlights();
	}
	if (MinutesSinceSave >= S->SaveEveryGameMinutes)
	{
		SaveToState();
	}
}

void UTimeOfDaySubsystem::UpdateNight(bool bPublish)
{
	const UTimeOfDaySettings* S = GetDefault<UTimeOfDaySettings>();
	const bool bNow = MurdarTime::IsNight(Clock.GetHour(), S->ToSun(), S->NightBelowLight);
	if (bNow == bNight)
	{
		return;
	}
	bNight = bNow;
	if (bPublish)
	{
		if (UGameEventSubsystem* Bus = UGameEventSubsystem::Get(this))
		{
			FGameEvent E; E.Tag = Tag(bNight ? TEXT("Event.Time.Dusk") : TEXT("Event.Time.Dawn")); E.Magnitude = Clock.GetHour(); Bus->Publish(E);
		}
		SweepHeadlights(); // don't wait for the next sweep at the moment it turns
	}
}

ADirectionalLight* UTimeOfDaySubsystem::Sun()
{
	if (!CachedSun.IsValid() && GetWorld())
	{
		// The level's sun: the first directional light (the one the sky atmosphere uses). ADAPT if a level has several.
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			CachedSun = *It;
			break;
		}
	}
	return CachedSun.Get();
}

void UTimeOfDaySubsystem::ApplySun()
{
	const UTimeOfDaySettings* S = GetDefault<UTimeOfDaySettings>();
	ADirectionalLight* L = S->bDriveSun ? Sun() : nullptr;
	if (!L)
	{
		return;
	}
	// The sun must be Movable to turn at runtime (README §Manual 1). A Stationary/Static one would warn and not move.
	const MurdarTime::FSunAngles A = MurdarTime::SunAngles(Clock.GetHour(), S->ToSun());
	L->SetActorRotation(FRotator(A.PitchDeg, A.YawDeg, 0.f));
}

void UTimeOfDaySubsystem::SweepHeadlights()
{
	// AI cars drive lit at night and dark by day. The player switches his own (L); parked cars stay as they are.
	for (TActorIterator<AMurdarVehicle> It(GetWorld()); It; ++It)
	{
		AMurdarVehicle* V = *It;
		if (V->IsAIDriven() && V->AreHeadlightsOn() != bNight)
		{
			V->SetHeadlights(bNight);
		}
	}
}

void UTimeOfDaySubsystem::SaveToState()
{
	MinutesSinceSave = 0.f;
	UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(this);
	const FGameplayTag KMin = Tag(TEXT("Stat.TimeOfDay"));
	const FGameplayTag KDay = Tag(TEXT("Stat.Day"));
	if (State && KMin.IsValid() && KDay.IsValid())
	{
		State->SetValue(KMin, Clock.GetMinutes());
		State->SetValue(KDay, float(Clock.GetDay()));
	}
}

FString UTimeOfDaySubsystem::Describe() const
{
	const int32 M = int32(Clock.GetMinutes());
	return FString::Printf(TEXT("time: day %d, %02d:%02d, light %.2f, %s%s | people x%.2f traffic x%.2f"),
		Clock.GetDay(), M / 60, M % 60, GetLightLevel01(), bNight ? TEXT("night") : TEXT("day"), bFrozen ? TEXT(" [frozen]") : TEXT(""),
		GetPeopleScale(), GetTrafficScale());
}
