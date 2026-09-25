#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralIsland.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class TREASURESKETCH_API AProceduralIsland : public AActor
{
    GENERATED_BODY()

public:
    AProceduralIsland();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Seed, Category="Island")
    int32 Seed = 1337;

    UPROPERTY(EditAnywhere, Category="Island", meta=(ClampMin="17", ClampMax="61"))
    int32 GridSize = 35;

    UPROPERTY(EditAnywhere, Category="Island")
    float CellSize = 260.f;

    float HeightAt(float X, float Y) const;
    FVector FindRandomLandPoint(FRandomStream& Stream, float MinimumHeight = 130.f) const;
    FString GetShapeName() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> IslandMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> WaterMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PalmInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PalmCollisionInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BushInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DriftwoodInstances;

    TArray<FVector2D> OccupiedPoints;

    void BuildIsland();
    void BuildWater();
    void BuildDecorations();
    void ApplyDecorationMaterials();
    float NormalizedIslandDistance(float X, float Y) const;
    float SlopeAt(float X, float Y) const;
    bool IsClearOfDecorations(float X, float Y, float Radius) const;

    UFUNCTION()
    void OnRep_Seed();
};
