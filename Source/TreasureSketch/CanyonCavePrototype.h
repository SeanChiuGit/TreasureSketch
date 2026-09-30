#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CanyonCavePrototype.generated.h"

class UProceduralMeshComponent;

// A fixed, isolated test of one mountain, one through tunnel, and a surface bypass.
// The exterior, entrance faces, tunnel walls, and floor come from one solid field.
UCLASS()
class TREASURESKETCH_API ACanyonCavePrototype : public AActor
{
    GENERATED_BODY()

public:
    ACanyonCavePrototype();
    virtual void OnConstruction(const FTransform& Transform) override;

    static float SurfaceHeight(float X, float Y);
    static float BypassY(float X);

private:
    UPROPERTY(VisibleAnywhere, Category="Canyon Cave")
    TObjectPtr<UProceduralMeshComponent> MountainMesh;
};
