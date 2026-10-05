#pragma once

#include "CoreMinimal.h"
#include "CanyonGrayboxLayout.h"
#include "GameFramework/Actor.h"
#include "ProceduralIsland.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UPointLightComponent;
class AExponentialHeightFog;

UENUM(BlueprintType)
enum class EIslandTheme : uint8
{
    PirateBeach,
    JungleRuins,
    MistForest,
    CanyonGraybox
};

UCLASS()
class TREASURESKETCH_API AProceduralIsland : public AActor
{
    GENERATED_BODY()

public:
    AProceduralIsland();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Seed, Category="Island")
    int32 Seed = 1337;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Seed, Category="Island")
    EIslandTheme Theme = EIslandTheme::PirateBeach;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Seed, Category="Island", meta=(ClampMin="17", ClampMax="321"))
    int32 GridSize = 39;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Seed, Category="Island")
    float CellSize = 330.f;

    // Multiplier of generated area, not length. Each theme defines its own 1x extent.
    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Seed, Category="Island", meta=(ClampMin="0.5", ClampMax="5", DisplayName="Map Area Multiplier"))
    float MapScale = 1.f;

    float HeightAt(float X, float Y) const;
    float GetFallRecoveryLimitZ() const;
    bool IsCanyonRouteAt(float X, float Y) const;
    FVector FindTreasurePoint(FRandomStream& Stream, float MinimumHeight = 130.f) const;
    FVector FindRandomLandPoint(FRandomStream& Stream, float MinimumHeight = 130.f) const;
    TArray<FVector> FindSeparatedTreasurePoints(FRandomStream& Stream, int32 Count, float MinimumSpacing) const;
    FVector FindSpawnPoint(float LateralOffset = 0.f) const;
    FRotator GetCanyonStartFacing() const;
    const FCanyonGrayboxLayout& GetCanyonLayout() const { return CanyonLayout; }
    FString GetShapeName() const;
    FString GetThemeName() const;
    static EIslandTheme SelectThemeFromTable(int32 InSeed, bool bIncludeLockedThemes = true, uint8 AllowedThemesMask = 0xff);
    void ConfigureThemeParameters();
    bool ToggleDebugFog();
    bool IsWeatherFogEnabled() const;

private:
    bool FindCanyonCaveFloor(const FVector& RoutePoint, FVector& Floor) const;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> IslandMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> WaterMesh;

    UPROPERTY(Transient)
    TObjectPtr<AExponentialHeightFog> WeatherFogActor;

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

    UPROPERTY()
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> ForestInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> JungleTreeCollisionInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CanyonLandmarkBoxes;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CanyonLandmarkCylinders;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> CanyonCaveMesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UPointLightComponent>> CanyonFillLights;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> CanyonAssetInstances;

    FCanyonGrayboxLayout CanyonLayout;
    // Terrain cells owned by the single mountain/tunnel mesh, in grid coordinates.
    FIntRect CanyonCavePatchCells;

    TMap<FIntPoint, TArray<FVector2D>> OccupiedBuckets;
    void RecordDecoration(float X, float Y);

    void BuildIsland();
    void BuildCanyonGrayboxTerrain();
    void BuildCanyonGrayboxLandmarks();
    void BuildCanyonAssets();
    void BuildCanyonCaves();
    void BuildWater();
    void BuildDecorations();
    void CreateRuntimeForestFog();
    void BuildLandmarks(FRandomStream& Stream);
    void BuildJungleDecorations();
    void BuildForestDecorations();
    void ApplyDecorationMaterials();
    float NormalizedIslandDistance(float X, float Y) const;
    float SlopeAt(float X, float Y) const;
    bool IsClearOfDecorations(float X, float Y, float Radius) const;
    int32 ScaledDecorationCount(int32 BaseCount) const;

    UFUNCTION()
    void OnRep_Seed();
};
