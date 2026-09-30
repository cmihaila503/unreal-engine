// The GPS: two routed pins at once - the MISSION pin (yellow: a story mission's objective, or the running job's
// current stage) and YOUR WAYPOINT (purple: placed on the big map in the pause menu). Each has its own route along
// the road lanes (MurdarRoad::RouteAlongLanes, the police's router), re-planned when you leave it, when the pin moves,
// and now and then. On foot: the Human navmesh, else a straight line. Your waypoint clears when you arrive.
// Also puts the minimap (SMurdarMinimap) on screen. Rules: MurdarGps (unit-tested).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/Gps/GpsRules.h"
#include "Widgets/SWidget.h"
#include "GpsSubsystem.generated.h"

UENUM(BlueprintType)
enum class EGpsPin : uint8 { Mission, Waypoint };

UCLASS()
class MURDAR_GAMEDEV_API UGpsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGpsSubsystem* Get(const UObject* WorldContext);

	/** A story mission points somewhere (its objective). Cleared by ClearMissionPin or a new one. */
	UFUNCTION(BlueprintCallable, Category = "Murdar|GPS") void SetMissionPin(FVector Where, FText Label);
	UFUNCTION(BlueprintCallable, Category = "Murdar|GPS") void ClearMissionPin();
	/** The map's place/remove button: near your waypoint removes it, elsewhere moves it. True = a waypoint is set. */
	UFUNCTION(BlueprintCallable, Category = "Murdar|GPS") bool ToggleWaypoint(FVector Where);
	UFUNCTION(BlueprintCallable, Category = "Murdar|GPS") void ClearWaypoint();

	UFUNCTION(BlueprintPure, Category = "Murdar|GPS") bool GetPin(EGpsPin Kind, FVector& OutWhere, FText& OutLabel) const;
	/** The route (world points, from the start of the plan); empty when none. */
	UFUNCTION(BlueprintPure, Category = "Murdar|GPS") TArray<FVector> GetRoute(EGpsPin Kind) const;
	/** What is left to drive/walk (cm); 0 with no route. */
	UFUNCTION(BlueprintPure, Category = "Murdar|GPS") float GetRemainingCm(EGpsPin Kind) const;

	const MurdarGps::FRoute& Route(EGpsPin Kind) const { return Routes[static_cast<int32>(Kind)]; }
	/** Where the player is and faces (world 2D + yaw), speed in km/h; false without a pawn. */
	bool GetPlayer(FVector& OutLoc, float& OutYaw, float& OutKph) const;
	FString Describe() const;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGpsSubsystem, STATGROUP_Tickables); }

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Think(float Now);
	/** The job's current stage, when a job runs (Handoff/Jobs UJobSubsystem::GetTargetLocation). */
	void PullJobPin();
	void Plan(EGpsPin Kind, const FVector& From, const FVector& FromDir, bool bDriving, float Now);
	void EnsureMinimap();
	void RemoveMinimap();

	MurdarGps::FPins Pins;
	FText Labels[MurdarGps::NumRouted];
	float PinZ[MurdarGps::NumRouted] = { 0.f, 0.f };
	MurdarGps::FRoute Routes[MurdarGps::NumRouted];
	bool bMissionFromJob = false;
	bool bScriptedMission = false;
	float NextThink = 0.f;
	int32 PlanTurn = 0; // alternate which route may plan this think (never two A* in one frame)

	TSharedPtr<SWidget> Minimap;      // SMurdarMinimap
	TSharedPtr<SWidget> MinimapHost;  // the SWeakWidget added to the viewport
	bool bMinimapAdded = false;
	/** Bus subscriptions (UGameEventSubsystem::FHandle = int32). */
	TArray<int32> BusHandles;
};
