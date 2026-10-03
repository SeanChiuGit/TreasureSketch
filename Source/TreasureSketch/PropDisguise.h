#pragma once

#include "CoreMinimal.h"
#include "PropDisguise.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UWorld;

USTRUCT()
struct FPropDisguise
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY() FVector Scale = FVector::OneVector;
    UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Materials;
    UPROPERTY() TArray<FLinearColor> Colors;
};

namespace PropDisguise
{
    bool FindTarget(UWorld* World, const FVector& Origin, const FVector& Direction, FPropDisguise& Out);
}
