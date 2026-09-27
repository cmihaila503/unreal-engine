// Project Settings > Game > Murdar Paper Map.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PaperMapSettings.generated.h"

class UTexture2D;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Paper Map"))
class MURDAR_GAMEDEV_API UPaperMapSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** The drawn city (a scanned hand-drawn map works best). Square texture. */
	UPROPERTY(EditAnywhere, config, Category = "Map") TSoftObjectPtr<UTexture2D> MapTexture;
	/** World X/Y (cm) at the texture's top-left (U=0,V=0) and bottom-right (U=1,V=1). ADAPT by reading two
	 *  landmarks' world positions and where they sit on the drawing. */
	UPROPERTY(EditAnywhere, config, Category = "Map") FVector2D WorldAtUV0 = FVector2D(-300000.0, -300000.0);
	UPROPERTY(EditAnywhere, config, Category = "Map") FVector2D WorldAtUV1 = FVector2D(300000.0, 300000.0);
	UPROPERTY(EditAnywhere, config, Category = "Map", meta = (ClampMin = "1", ClampMax = "20")) float MaxZoom = 6.f;
	/** A pencil cross where he stands. Off = the true paper-map experience. */
	UPROPERTY(EditAnywhere, config, Category = "Map") bool bShowPlayer = true;
	UPROPERTY(EditAnywhere, config, Category = "Map") FLinearColor PencilColor = FLinearColor(0.12f, 0.1f, 0.35f, 0.9f);
	UPROPERTY(EditAnywhere, config, Category = "Map") FLinearColor JobColor = FLinearColor(0.7f, 0.05f, 0.05f, 0.95f);
};
