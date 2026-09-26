// A car that follows the player at night — an undercover crew or a gang car — driven by the same
// UVehiclePursuitComponent every AI car uses (one driver implementation). It knows only what it can see
// (MurdarRearview::CanSeeTarget: lights off at night = a shadow past a few metres); when it loses him it switches on the
// high beams and floors it to where he should be. It reacts to the player's tests the way its role would, and that
// reaction is the only thing that tells the player who it is:
//   brake check      undercover: drops back, calmly, no horn     gang: comes up alongside
//   inspection turn  undercover: a pro often drives on           gang: follows, every time
//   lights out       both: high beams, sudden speed — a civilian behind just carries on
// Blown ("he's made me"): the undercover crew breaks off; the gang car stops pretending (alongside, then rams).
// 4 Hz timer brain, like the police controller.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/Rearview/RearviewRules.h"
#include "RearviewTailController.generated.h"

class AMurdarVehicle;
class UVehiclePursuitComponent;
class USpotLightComponent;

UENUM(BlueprintType)
enum class ETailState : uint8
{
	Tailing,     // behind him at the role's distance
	BackingOff,  // undercover after a brake check: calm, further back
	Alongside,   // gang after a brake check: level with him, staring
	Lost,        // lost sight: high beams, flat out to where he should be
	Aggressive,  // gang, blown: alongside, then ram
	BreakOff,    // done: drive on, vanish when out of his sight
};

UCLASS()
class MURDAR_GAMEDEV_API ARearviewTailController : public AAIController
{
	GENERATED_BODY()

public:
	ARearviewTailController();

	/** Before Possess. */
	void Setup(MurdarRearview::ETailRole InRole, float InSkill01, AMurdarVehicle* InTarget);

	// --- the player's tests (URearviewSubsystem) ---
	void OnPlayerBrakeCheck();
	void OnPlayerInspectionTurn();

	UFUNCTION(BlueprintPure, Category = "Murdar|Rearview") ETailState GetTailState() const { return State; }
	FString Describe() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Think();                       // 4 Hz
	void Enter(ETailState Next, const TCHAR* Why);
	bool CanSeePlayer(float& OutDistCm) const;
	bool IsWithinReact() const;
	void SetHighBeams(bool bOn);
	void BreakOff(const TCHAR* Why);
	void Finish();
	UVehiclePursuitComponent* Pursuit() const;
	AMurdarVehicle* Car() const;

	MurdarRearview::ETailRole Role = MurdarRearview::ETailRole::Undercover;
	float Skill01 = 0.5f;
	TWeakObjectPtr<AMurdarVehicle> Target;
	TUniquePtr<MurdarRearview::FTailExposure> Exposure;

	ETailState State = ETailState::Tailing;
	float StateSince = 0.f;
	float StartTime = 0.f;
	float LastSeenTime = -1.e6f;
	FVector LastSeenLocation = FVector::ZeroVector;
	FVector LastSeenVelocity = FVector::ZeroVector;
	float LastReportTime = -1.e6f;
	FVector BreakOffGoal = FVector::ZeroVector;
	FString Reason;

	bool bHighBeams = false;
	struct FBeam { TWeakObjectPtr<USpotLightComponent> Light; float Intensity = 0.f; float Radius = 0.f; };
	TArray<FBeam> Beams;

	FTimerHandle ThinkTimer;
	FRandomStream Rng;
	bool bFinished = false;
};
