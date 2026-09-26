// Captures the world into FNarrativeState::World just before every save (UNarrativeStateSubsystem::OnBeforeSave) and
// puts it back after "continue": on the first Event.Chapter.Entered after a load from file, on the same map.
// Order on continue: the chapter director restores the story and places him at the checkpoint (and spawns the chapter
// car), then this moves him and his car to where they were — unless the chapter is a story one.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldStateSubsystem.generated.h"

class UWorldStateComponent;
class AMurdarVehicle;
struct FGameEvent;
struct FMurdarWorldState;

UCLASS()
class MURDAR_GAMEDEV_API UWorldStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWorldStateSubsystem* Get(const UObject* WorldContext);

	void Register(UWorldStateComponent* Component);
	void Unregister(UWorldStateComponent* Component);
	void NotifyDestroyed(UWorldStateComponent* Component);

	FString Describe() const;

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	FMurdarWorldState* Data() const;
	FName MapName() const;
	bool IsStoryMode() const;
	void Capture();
	void Apply();
	AMurdarVehicle* RestoreCar(const FMurdarWorldState& W);

	TArray<TWeakObjectPtr<UWorldStateComponent>> Registered;
	TWeakObjectPtr<AMurdarVehicle> PlayerCar;
	TArray<int32> BusHandles;
	FDelegateHandle BeforeSaveHandle;
};
