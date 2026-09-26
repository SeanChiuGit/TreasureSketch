#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralIsland.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

UENUM(BlueprintType)
enum class EIslandTheme : uint8
{
    PirateBeach,
    JungleRuins
};

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

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Seed, Category="Island")
    EIslandTheme Theme = EIslandTheme::PirateBeach;

    UPROPERTY(EditAnywhere, Category="Island", meta=(ClampMin="17", ClampMax="61"))
    int32 GridSize = 39;

    UPROPERTY(EditAnywhere, Category="Island")
    float CellSize = 330.f;

    float HeightAt(float X, float Y) const;
    FVector FindRandomLandPoint(FRandomStream& Stream, float MinimumHeight = 130.f) const;
    FVector FindSpawnPoint(float LateralOffset = 0.f) const;
    FString GetShapeName() const;
    FString GetThemeName() const;
    static EIslandTheme SelectThemeFromTable(int32 InSeed, bool bIncludeLockedThemes = true);

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

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SkullIdolInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FaceIdolInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GiantAnchorInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShipwreckInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BrokenMastInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StoneRingInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CampfireInstances;

    UPROPERTY()
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> JungleInstances;

    TArray<FVector2D> OccupiedPoints;

    void BuildIsland();
    void BuildWater();
    void BuildDecorations();
    void BuildLandmarks(FRandomStream& Stream);
    void BuildJungleDecorations();
    void ApplyDecorationMaterials();
    float NormalizedIslandDistance(float X, float Y) const;
    float SlopeAt(float X, float Y) const;
    bool IsClearOfDecorations(float X, float Y, float Radius) const;

    UFUNCTION()
    void OnRep_Seed();
};
