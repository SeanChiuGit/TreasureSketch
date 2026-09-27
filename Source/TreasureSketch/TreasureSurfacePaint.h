#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "TreasureSurfacePaint.generated.h"

class UProceduralMeshComponent;

USTRUCT()
struct FSurfacePaintStamp
{
    GENERATED_BODY()
    UPROPERTY() FVector Position = FVector::ZeroVector;
    UPROPERTY() FVector Normal = FVector::UpVector;
};

// Optional round-owned experiment. Surface geometry and network state live here,
// independently of the paper sketch and island-generation code.
UCLASS()
class TREASURESKETCH_API ATreasureSurfacePaint : public AActor
{
    GENERATED_BODY()
public:
    ATreasureSurfacePaint();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    bool AddStamp(const FHitResult& Hit);
    static constexpr int32 MaxStamps = 15;

private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> PaintMesh;
    UPROPERTY(ReplicatedUsing=RebuildPaint) TArray<FSurfacePaintStamp> Stamps;
    int32 BuiltStampCount = 0;
    TArray<FVector> Vertices, Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    UFUNCTION() void RebuildPaint();
};
