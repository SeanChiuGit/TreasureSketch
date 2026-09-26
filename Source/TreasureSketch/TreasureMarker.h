#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TreasureMarker.generated.h"

class UStaticMeshComponent;

UCLASS()
class TREASURESKETCH_API ATreasureMarker : public AActor
{
    GENERATED_BODY()

public:
    ATreasureMarker();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> BarA;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> BarB;
};
