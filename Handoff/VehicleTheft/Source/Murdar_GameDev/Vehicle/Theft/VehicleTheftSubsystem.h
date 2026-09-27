// Taking cars. AMurdarCharacter::Input_Interact asks TryTakeCar() where it used to call FindEnterable + Enter
// (README §Patches); the HUD asks GetPrompt(). Free unlocked car: in. Locked parked car: break the window, hotwire
// (seconds at the door; walk away = cancelled), maybe an alarm. Traffic car stopped or crawling: the driver is pulled
// out and the car is yours. Every theft gets a report time (MurdarTheft::ReportDelay); once reported, the first patrol
// that sees him in it reports Crime.CarTheft (a stop). Cars he has driven once are his: never locked again.
// A respray (Handoff/Garage) calls MarkCleaned. 10 Hz while a break-in runs, 2 Hz otherwise.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Vehicle/Theft/TheftRules.h"
#include "VehicleTheftSubsystem.generated.h"

class AMurdarCharacter;
class AMurdarVehicle;
class UAudioComponent;
struct FGameEvent;

UCLASS()
class MURDAR_GAMEDEV_API UVehicleTheftSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UVehicleTheftSubsystem* Get(const UObject* WorldContext);

	/** The "E" at a car. True when something started (entered, break-in begun, carjack). */
	bool TryTakeCar(AMurdarCharacter* Man);
	/** What the "E" would do near this man, for the HUD prompt; empty when no car is in reach. */
	FText GetPrompt(const AMurdarCharacter* Man) const;
	bool IsBreakingIn() const { return BreakIn.Get() == MurdarTheft::FBreakIn::EState::Window || BreakIn.Get() == MurdarTheft::FBreakIn::EState::Wires; }

	bool IsLocked(AMurdarVehicle* Car);
	bool IsReportedStolen(const AMurdarVehicle* Car) const;
	/** Resprayed / new plates: the paper trail ends. */
	void MarkCleaned(AMurdarVehicle* Car);
	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FTarget { AMurdarVehicle* Car = nullptr; bool bCarjack = false; };
	FTarget FindTarget(const AMurdarCharacter* Man) const;
	bool IsNight() const;
	float NearestWitnessCm(const FVector& Where) const;
	void Tick();
	void StartBreakIn(AMurdarCharacter* Man, AMurdarVehicle* Car);
	void StartCarjack(AMurdarCharacter* Man, AMurdarVehicle* Car);
	void FinishTheft(AMurdarCharacter* Man, AMurdarVehicle* Car, MurdarTheft::EMethod Method, bool bAlarm);
	void StartAlarm(AMurdarVehicle* Car);
	void StopAlarm();
	void Publish(const TCHAR* TagName, const AActor* Source) const;

	TMap<TWeakObjectPtr<AMurdarVehicle>, bool> Locks;         // decided the first time anyone asks
	TSet<TWeakObjectPtr<AMurdarVehicle>> Owned;               // driven by him once: never locked again
	TMap<TWeakObjectPtr<AMurdarVehicle>, MurdarTheft::FStolenCar> Stolen;

	MurdarTheft::FBreakIn BreakIn;
	TWeakObjectPtr<AMurdarVehicle> BreakInCar;
	TWeakObjectPtr<AMurdarCharacter> BreakInMan;
	bool bBreakInAlarm = false;

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Alarm;
	TWeakObjectPtr<AMurdarVehicle> AlarmCar;
	float AlarmEndsAt = 0.f;

	FTimerHandle TickTimer;
	int32 BusHandle = 0;
};
