#include "ProceduralIsland.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
struct FIslandThemeDefinition
{
    EIslandTheme Theme;
    const TCHAR* Name;
    int32 SelectionWeight;
    bool bUnlockedByDefault;
    float TreeDensity;
    float BushDensity;
    float TerrainRelief;
    int32 GridSize;
    float CellSize;
    float ResourceCountScale;
};

const FIslandThemeDefinition ThemeTable[] = {
    // Theme, display/internal name, random weight, default unlock, trees, undergrowth, relief.
    // Add future themes here first; their generator can then branch on the enum below.
    { EIslandTheme::PirateBeach, TEXT("PirateBeach"), 60, true,  1.00f, 1.00f, 1.00f, 39, 330.f, 1.f },
    { EIslandTheme::JungleRuins, TEXT("JungleRuins"), 0, false, 1.45f, 2.20f, 0.38f, 39, 330.f, 1.f },
    // Forest 1x has 75% of the former 125.4m side length; preserve original forest density.
    { EIslandTheme::MistForest, TEXT("MistForest"), 50, true, 3.60f, 3.80f, 1.00f, 39, 247.5f,
        (9405.f * 9405.f) / (24600.f * 24600.f) },
    { EIslandTheme::Canyon, TEXT("Canyon"), 0, false, 0.f, 0.f, 1.f, 81, 150.f, 1.f },
};

const FIslandThemeDefinition& GetThemeDefinition(EIslandTheme Theme)
{
    for (const FIslandThemeDefinition& Definition : ThemeTable)
        if (Definition.Theme == Theme) return Definition;
    return ThemeTable[0];
}

enum EJungleAsset : int32 { ButtressTree, ForkedTree, Fern, JungleBush, FallenLog, ExplorerHut, Temple, Watchtower, ShrineHall, Crypt, JungleAssetCount };
enum EForestAsset : int32
{
    AncientOak, TwinTrunk, SpreadingBeech, WeepingTree,
    MossBoulder, SplitBoulder, ThreeStoneStack, FlatStoneSlab,
    SmallPond, ReedCluster, HollowStump, MushroomRing, ForestAssetCount
};
enum class EIslandShape : uint8
{
    RoundBay, LongSpine, Crescent, TwinCove, TriCape, Hook, StarCove,
    MainAndSatellite, TwinIslands, ThreeIslets
};
enum class ETerrainProfile : uint8 { Flat, SinglePeak, TwinPeaks, Ridge, EdgeCliff, Basin, Rolling };

EIslandShape ShapeFromSeed(int32 Seed)
{
    return static_cast<EIslandShape>(FMath::Abs(Seed) % 10);
}

ETerrainProfile TerrainFromSeed(int32 Seed)
{
    return static_cast<ETerrainProfile>(FMath::Abs(Seed / 7) % 7);
}

float SeedNoise(float X, float Y, int32 Seed)
{
    const float A = FMath::Sin(X * 0.00137f + Y * 0.00191f + Seed * 0.071f);
    const float B = FMath::Sin(X * -0.00213f + Y * 0.00117f - Seed * 0.043f);
    const float C = FMath::Cos((X + Y) * 0.00073f + Seed * 0.019f);
    return (A + B + C) / 3.f;
}

void ConfigureInstances(UHierarchicalInstancedStaticMeshComponent* Component)
{
    Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Component->SetCollisionResponseToAllChannels(ECR_Block);
    Component->SetCanEverAffectNavigation(false);
    Component->SetMobility(EComponentMobility::Movable);
}
}

AProceduralIsland::AProceduralIsland()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;

    IslandMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("IslandMesh"));
    SetRootComponent(IslandMesh);
    IslandMesh->bUseComplexAsSimpleCollision = true;
    WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
    WaterMesh->SetupAttachment(RootComponent);

    PalmInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PalmInstances"));
    PalmInstances->SetupAttachment(RootComponent);
    PalmCollisionInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PalmCollisionInstances"));
    PalmCollisionInstances->SetupAttachment(RootComponent);
    RockInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RockInstances"));
    RockInstances->SetupAttachment(RootComponent);
    BushInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("BushInstances"));
    BushInstances->SetupAttachment(RootComponent);
    DriftwoodInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DriftwoodInstances"));
    DriftwoodInstances->SetupAttachment(RootComponent);

#define CREATE_LANDMARK_COMPONENT(Member) \
    Member = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT(#Member)); \
    Member->SetupAttachment(RootComponent); \
    ConfigureInstances(Member)
    CREATE_LANDMARK_COMPONENT(SkullIdolInstances);
    CREATE_LANDMARK_COMPONENT(FaceIdolInstances);
    CREATE_LANDMARK_COMPONENT(GiantAnchorInstances);
    CREATE_LANDMARK_COMPONENT(ShipwreckInstances);
    CREATE_LANDMARK_COMPONENT(BrokenMastInstances);
    CREATE_LANDMARK_COMPONENT(StoneRingInstances);
    CREATE_LANDMARK_COMPONENT(CampfireInstances);
#undef CREATE_LANDMARK_COMPONENT

    ConfigureInstances(PalmInstances);
    ConfigureInstances(PalmCollisionInstances);
    ConfigureInstances(RockInstances);
    ConfigureInstances(BushInstances);
    ConfigureInstances(DriftwoodInstances);
    DriftwoodInstances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PalmInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PalmCollisionInstances->SetVisibility(false, true);
    PalmCollisionInstances->SetHiddenInGame(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PalmMesh(TEXT("/Game/IslandAssets/Prototype/SM_PalmTree_A/StaticMeshes/SM_PalmTree_A.SM_PalmTree_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RockMesh(TEXT("/Game/IslandAssets/Prototype/SM_RockCluster_A/StaticMeshes/SM_RockCluster_A.SM_RockCluster_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BushMesh(TEXT("/Game/IslandAssets/Prototype/SM_Bush_A/StaticMeshes/SM_Bush_A.SM_Bush_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DriftwoodMesh(TEXT("/Game/IslandAssets/Prototype/SM_Driftwood_A/StaticMeshes/SM_Driftwood_A.SM_Driftwood_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SkullMesh(TEXT("/Game/IslandAssets/PirateLandmarks/SM_SkullIdol_A/StaticMeshes/SM_SkullIdol_A.SM_SkullIdol_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FaceMesh(TEXT("/Game/IslandAssets/PirateLandmarks/SM_FaceIdol_A/StaticMeshes/SM_FaceIdol_A.SM_FaceIdol_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> AnchorMesh(TEXT("/Game/IslandAssets/PirateLandmarks/SM_GiantAnchor_A/StaticMeshes/SM_GiantAnchor_A.SM_GiantAnchor_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShipwreckMesh(TEXT("/Game/IslandAssets/PirateLandmarks/SM_HalfBuriedShipwreck_A/StaticMeshes/SM_HalfBuriedShipwreck_A.SM_HalfBuriedShipwreck_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MastMesh(TEXT("/Game/IslandAssets/PirateLandmarks/SM_BrokenMast_A/StaticMeshes/SM_BrokenMast_A.SM_BrokenMast_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> StoneRingMesh(TEXT("/Game/IslandAssets/PirateLandmarks/SM_StoneRing_A/StaticMeshes/SM_StoneRing_A.SM_StoneRing_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CampfireMesh(TEXT("/Game/IslandAssets/PirateLandmarks/SM_CampfireRuins_A/StaticMeshes/SM_CampfireRuins_A.SM_CampfireRuins_A"));
    PalmInstances->SetStaticMesh(PalmMesh.Object);
    PalmCollisionInstances->SetStaticMesh(CylinderMesh.Object);
    RockInstances->SetStaticMesh(RockMesh.Object);
    BushInstances->SetStaticMesh(BushMesh.Object);
    DriftwoodInstances->SetStaticMesh(DriftwoodMesh.Object);
    SkullIdolInstances->SetStaticMesh(SkullMesh.Object);
    FaceIdolInstances->SetStaticMesh(FaceMesh.Object);
    GiantAnchorInstances->SetStaticMesh(AnchorMesh.Object);
    ShipwreckInstances->SetStaticMesh(ShipwreckMesh.Object);
    BrokenMastInstances->SetStaticMesh(MastMesh.Object);
    StoneRingInstances->SetStaticMesh(StoneRingMesh.Object);
    CampfireInstances->SetStaticMesh(CampfireMesh.Object);

    JungleTreeCollisionInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("JungleTreeCollisionInstances"));
    JungleTreeCollisionInstances->SetupAttachment(RootComponent);
    ConfigureInstances(JungleTreeCollisionInstances);
    JungleTreeCollisionInstances->SetStaticMesh(CylinderMesh.Object);
    JungleTreeCollisionInstances->SetVisibility(false, true);
    JungleTreeCollisionInstances->SetHiddenInGame(true);

    const TCHAR* JunglePaths[JungleAssetCount] = {
        TEXT("/Game/IslandAssets/JungleNature/SM_ButtressTree_A/StaticMeshes/SM_ButtressTree_A.SM_ButtressTree_A"),
        TEXT("/Game/IslandAssets/JungleNature/SM_ForkedJungleTree_A/StaticMeshes/SM_ForkedJungleTree_A.SM_ForkedJungleTree_A"),
        TEXT("/Game/IslandAssets/JungleNature/SM_FernCluster_A/StaticMeshes/SM_FernCluster_A.SM_FernCluster_A"),
        TEXT("/Game/IslandAssets/JungleNature/SM_JungleBush_A/StaticMeshes/SM_JungleBush_A.SM_JungleBush_A"),
        TEXT("/Game/IslandAssets/JungleNature/SM_FallenJungleLog_A/StaticMeshes/SM_FallenJungleLog_A.SM_FallenJungleLog_A"),
        TEXT("/Game/IslandAssets/JungleRuins/SM_RuinExplorerHut_A/StaticMeshes/SM_RuinExplorerHut_A.SM_RuinExplorerHut_A"),
        TEXT("/Game/IslandAssets/JungleRuins/SM_RuinTwoLevelTemple_A/StaticMeshes/SM_RuinTwoLevelTemple_A.SM_RuinTwoLevelTemple_A"),
        TEXT("/Game/IslandAssets/JungleRuins/SM_RuinWatchtower_A/StaticMeshes/SM_RuinWatchtower_A.SM_RuinWatchtower_A"),
        TEXT("/Game/IslandAssets/JungleRuins/SM_RuinShrineHall_A/StaticMeshes/SM_RuinShrineHall_A.SM_RuinShrineHall_A"),
        TEXT("/Game/IslandAssets/JungleRuins/SM_RuinCryptEntrance_A/StaticMeshes/SM_RuinCryptEntrance_A.SM_RuinCryptEntrance_A")
    };
    for (int32 Index = 0; Index < JungleAssetCount; ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            *FString::Printf(TEXT("JungleInstances_%d"), Index));
        Component->SetupAttachment(RootComponent);
        ConfigureInstances(Component);
        if (Index == ButtressTree || Index == ForkedTree || Index == Fern || Index == JungleBush)
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, JunglePaths[Index]));
        JungleInstances.Add(Component);
    }

    const TCHAR* ForestPaths[ForestAssetCount] = {
        TEXT("/Game/IslandAssets/MistForest/SM_MistForestAncientOak_A/StaticMeshes/SM_MistForestAncientOak_A.SM_MistForestAncientOak_A"),
        TEXT("/Game/IslandAssets/MistForest/SM_MistForestTwinTrunk_A/StaticMeshes/SM_MistForestTwinTrunk_A.SM_MistForestTwinTrunk_A"),
        TEXT("/Game/IslandAssets/MistForest/SM_MistForestSpreadingBeech_A/StaticMeshes/SM_MistForestSpreadingBeech_A.SM_MistForestSpreadingBeech_A"),
        TEXT("/Game/IslandAssets/MistForest/SM_MistForestWeepingTree_A/StaticMeshes/SM_MistForestWeepingTree_A.SM_MistForestWeepingTree_A"),
        TEXT("/Game/IslandAssets/MistForestLandmarks/SM_MistForestMossBoulder_A/StaticMeshes/SM_MistForestMossBoulder_A.SM_MistForestMossBoulder_A"),
        TEXT("/Game/IslandAssets/MistForestLandmarks/SM_MistForestSplitBoulder_A/StaticMeshes/SM_MistForestSplitBoulder_A.SM_MistForestSplitBoulder_A"),
        TEXT("/Game/IslandAssets/MistForestLandmarks/SM_MistForestThreeStoneStack_A/StaticMeshes/SM_MistForestThreeStoneStack_A.SM_MistForestThreeStoneStack_A"),
        TEXT("/Game/IslandAssets/MistForestLandmarks/SM_MistForestFlatStoneSlab_A/StaticMeshes/SM_MistForestFlatStoneSlab_A.SM_MistForestFlatStoneSlab_A"),
        TEXT("/Game/IslandAssets/MistForestLandmarks/SM_MistForestSmallPond_A/StaticMeshes/SM_MistForestSmallPond_A.SM_MistForestSmallPond_A"),
        TEXT("/Game/IslandAssets/MistForestLandmarks/SM_MistForestReedCluster_A/StaticMeshes/SM_MistForestReedCluster_A.SM_MistForestReedCluster_A"),
        TEXT("/Game/IslandAssets/MistForestLandmarks/SM_MistForestHollowStump_A/StaticMeshes/SM_MistForestHollowStump_A.SM_MistForestHollowStump_A"),
        TEXT("/Game/IslandAssets/MistForestLandmarks/SM_MistForestMushroomRing_A/StaticMeshes/SM_MistForestMushroomRing_A.SM_MistForestMushroomRing_A")
    };
    for (int32 Index = 0; Index < ForestAssetCount; ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            *FString::Printf(TEXT("ForestInstances_%d"), Index));
        Component->SetupAttachment(RootComponent);
        ConfigureInstances(Component);
        if (Index <= WeepingTree || Index == SmallPond || Index == ReedCluster || Index == MushroomRing)
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, ForestPaths[Index]));
        ForestInstances.Add(Component);
    }
    for (int32 Index = 0; Index < 18; ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            *FString::Printf(TEXT("CanyonInstances_%d"), Index));
        Component->SetupAttachment(RootComponent);
        ConfigureInstances(Component);
        // The continuous procedural ground owns collision at forks and along ramps.
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CanyonInstances.Add(Component);
    }
    CanyonLandmarkCubes = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CanyonLandmarkCubes"));
    CanyonLandmarkColumns = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CanyonLandmarkColumns"));
    CanyonLandmarkBoulders = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CanyonLandmarkBoulders"));
    for (UHierarchicalInstancedStaticMeshComponent* Component : {
        CanyonLandmarkCubes.Get(), CanyonLandmarkColumns.Get(), CanyonLandmarkBoulders.Get() })
    {
        Component->SetupAttachment(RootComponent);
        ConfigureInstances(Component);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    CanyonLandmarkCubes->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    CanyonLandmarkColumns->SetStaticMesh(CylinderMesh.Object);
    CanyonLandmarkBoulders->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
}

void AProceduralIsland::ConfigureThemeParameters()
{
    const FIslandThemeDefinition& Definition = GetThemeDefinition(Theme);
    MapScale = FMath::IsFinite(MapScale) ? FMath::Clamp(MapScale, 0.5f, 5.f) : 1.f;
    const float LengthScale = FMath::Sqrt(MapScale);
    GridSize = FMath::RoundToInt((Definition.GridSize - 1) * LengthScale) + 1;
    CellSize = Definition.CellSize * (Definition.GridSize - 1) * LengthScale / (GridSize - 1);
}

int32 AProceduralIsland::ScaledDecorationCount(int32 BaseCount) const
{
    return FMath::Max(1, FMath::RoundToInt(BaseCount * GetThemeDefinition(Theme).ResourceCountScale * MapScale));
}

void AProceduralIsland::BeginPlay()
{
    Super::BeginPlay();
    CreateRuntimeForestFog();
}

void AProceduralIsland::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (WeatherFogActor)
    {
        WeatherFogActor->Destroy();
        WeatherFogActor = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void AProceduralIsland::CreateRuntimeForestFog()
{
    if (Theme != EIslandTheme::MistForest)
    {
        if (WeatherFogActor)
        {
            WeatherFogActor->Destroy();
            WeatherFogActor = nullptr;
        }
        return;
    }
    if (WeatherFogActor || !GetWorld()) return;

    // PIE inherits editor viewport show flags. If Fog was unchecked in the editor's Show menu,
    // even a valid registered fog actor is deliberately skipped by the renderer.
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->EngineShowFlags.SetFog(true);
        GEngine->GameViewport->EngineShowFlags.SetVolumetricFog(true);
    }

    // A world fog actor is used deliberately. A fog component nested inside the procedural
    // island was reporting visible but was not consistently registered in the renderer.
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    WeatherFogActor = GetWorld()->SpawnActor<AExponentialHeightFog>(
        AExponentialHeightFog::StaticClass(), GetActorLocation() + FVector(0.f, 0.f, 900.f),
        FRotator::ZeroRotator, SpawnParameters);
    UExponentialHeightFogComponent* Fog = WeatherFogActor ? WeatherFogActor->GetComponent() : nullptr;
    if (!Fog) return;

    Fog->SetFogDensity(0.20f);
    Fog->SetFogHeightFalloff(0.10f);
    Fog->SetFogMaxOpacity(1.0f);
    Fog->SetStartDistance(0.f);
    Fog->SetEndDistance(30000.f);
    Fog->SetFogInscatteringColor(FLinearColor(0.30f, 0.36f, 0.33f));
    Fog->SetVolumetricFog(true);
    Fog->SetVolumetricFogExtinctionScale(8.0f);
    Fog->SetVolumetricFogDistance(30000.f);
    Fog->SetVisibility(true, true);
    Fog->MarkRenderStateDirty();

    const bool bFogShowFlag = GEngine && GEngine->GameViewport
        ? GEngine->GameViewport->EngineShowFlags.Fog : false;
    const bool bVolumetricShowFlag = GEngine && GEngine->GameViewport
        ? GEngine->GameViewport->EngineShowFlags.VolumetricFog : false;
    UE_LOG(LogTemp, Warning, TEXT("TREASURE_FOREST_FOG NetMode=%d Actor=%s Registered=%s Visible=%s ShowFog=%s ShowVolumetric=%s Density=0.20 Extinction=8.0 Height=%.0f"),
        static_cast<int32>(GetNetMode()),
        *GetNameSafe(WeatherFogActor), Fog->IsRegistered() ? TEXT("YES") : TEXT("NO"),
        Fog->IsVisible() ? TEXT("YES") : TEXT("NO"), bFogShowFlag ? TEXT("YES") : TEXT("NO"),
        bVolumetricShowFlag ? TEXT("YES") : TEXT("NO"), Fog->GetComponentLocation().Z);
}

bool AProceduralIsland::ToggleDebugFog()
{
    if (!WeatherFogActor) CreateRuntimeForestFog();
    UExponentialHeightFogComponent* Fog = WeatherFogActor ? WeatherFogActor->GetComponent() : nullptr;
    if (!Fog) return false;
    const bool bEnable = !Fog->IsVisible();
    Fog->SetVisibility(bEnable, true);
    return bEnable;
}

bool AProceduralIsland::IsWeatherFogEnabled() const
{
    return WeatherFogActor && WeatherFogActor->GetComponent() && WeatherFogActor->GetComponent()->IsVisible();
}

void AProceduralIsland::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AProceduralIsland, Seed);
    DOREPLIFETIME(AProceduralIsland, Theme);
    DOREPLIFETIME(AProceduralIsland, GridSize);
    DOREPLIFETIME(AProceduralIsland, CellSize);
    DOREPLIFETIME(AProceduralIsland, MapScale);
}

EIslandTheme AProceduralIsland::SelectThemeFromTable(int32 InSeed, bool bIncludeLockedThemes, uint8 AllowedThemesMask)
{
    int32 TotalWeight = 0;
    for (const FIslandThemeDefinition& Definition : ThemeTable)
        if ((AllowedThemesMask & (1u << static_cast<uint8>(Definition.Theme)))
            && (bIncludeLockedThemes || Definition.bUnlockedByDefault)) TotalWeight += Definition.SelectionWeight;
    if (TotalWeight <= 0) return EIslandTheme::PirateBeach;
    int32 Pick = FMath::Abs(InSeed / 10) % TotalWeight;
    for (const FIslandThemeDefinition& Definition : ThemeTable)
    {
        if (!(AllowedThemesMask & (1u << static_cast<uint8>(Definition.Theme)))
            || (!bIncludeLockedThemes && !Definition.bUnlockedByDefault)) continue;
        if (Pick < Definition.SelectionWeight) return Definition.Theme;
        Pick -= Definition.SelectionWeight;
    }
    return EIslandTheme::PirateBeach;
}

FString AProceduralIsland::GetThemeName() const
{
    for (const FIslandThemeDefinition& Definition : ThemeTable)
        if (Definition.Theme == Theme) return Definition.Name;
    return TEXT("Unknown");
}

void AProceduralIsland::OnRep_Seed()
{
    ConfigureThemeParameters();
    // Clients often begin play while Theme still has its default value. Create their local
    // rendering-only fog after the replicated forest theme arrives, not only on BeginPlay.
    CreateRuntimeForestFog();
    BuildIsland();
    BuildWater();
    BuildDecorations();
}

void AProceduralIsland::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConfigureThemeParameters();
    BuildIsland();
    BuildWater();
    BuildDecorations();
}

FString AProceduralIsland::GetShapeName() const
{
    if (Theme == EIslandTheme::Canyon) return FTreasureGameplayLayout::TopologyName(CanyonLayout.Topology);
    switch (ShapeFromSeed(Seed))
    {
    case EIslandShape::RoundBay: return TEXT("RoundBay");
    case EIslandShape::LongSpine: return TEXT("LongSpine");
    case EIslandShape::Crescent: return TEXT("Crescent");
    case EIslandShape::TwinCove: return TEXT("TwinCove");
    case EIslandShape::TriCape: return TEXT("TriCape");
    case EIslandShape::Hook: return TEXT("Hook");
    case EIslandShape::StarCove: return TEXT("StarCove");
    case EIslandShape::MainAndSatellite: return TEXT("MainAndSatellite");
    case EIslandShape::TwinIslands: return TEXT("TwinIslands");
    default: return TEXT("ThreeIslets");
    }
}

void AProceduralIsland::BuildCanyonTopology()
{
    const float Spacing = CellSize * (GridSize - 1) / 5.f;
    CanyonLayout = FTreasureGameplayLayoutGenerator::GenerateCanyon(Seed, Spacing);
    CanyonNodes.Reset();
    CanyonEdges.Reset();
    for (const FTreasureLayoutNode& Node : CanyonLayout.Nodes)
        CanyonNodes.Add({ Node.Position, Node.FloorHeight });
    for (const FTreasureLayoutEdge& Edge : CanyonLayout.Edges)
        CanyonEdges.Add({ Edge.A, Edge.B, Edge.bPrimary });
    CanyonGoalNode = CanyonLayout.TreasureNode;
    FString Reason;
    const bool bValid = CanyonLayout.ValidateGraph(&Reason);
    UE_LOG(LogTemp, Display,
        TEXT("TREASURE_CANYON_LAYOUT Seed=%d Topology=%s Relationship=%s Nodes=%d Edges=%d Landmarks=%d Routes=%d Valid=%s %s"),
        Seed, FTreasureGameplayLayout::TopologyName(CanyonLayout.Topology),
        FTreasureGameplayLayout::RelationshipName(CanyonLayout.TreasureRelationship),
        CanyonNodes.Num(), CanyonEdges.Num(), CanyonLayout.GetLandmarkCount(), CanyonLayout.GetRouteCount(),
        bValid ? TEXT("YES") : TEXT("NO"), *Reason);
    if (!bValid) UE_LOG(LogTemp, Error, TEXT("TREASURE_CANYON_LAYOUT_INVALID Seed=%d Reason=%s"), Seed, *Reason);
}

bool AProceduralIsland::ValidateCanyonRoutes() const
{
    if (Theme != EIslandTheme::Canyon || !CanyonLayout.ValidateGraph()) return false;
    // Terrain is the realization of the graph: sample every route, not just endpoints.
    for (const FCanyonEdge& Edge : CanyonEdges)
    {
        const FCanyonNode& A = CanyonNodes[Edge.A];
        const FCanyonNode& B = CanyonNodes[Edge.B];
        for (int32 Step = 0; Step <= 8; ++Step)
        {
            const FVector2D Point = FMath::Lerp(A.Position, B.Position, Step / 8.f);
            if (SlopeAt(Point.X, Point.Y) > 0.45f)
            {
                UE_LOG(LogTemp, Warning, TEXT("TREASURE_CANYON_VALIDATION Seed=%d Failed=Slope Edge=%d-%d Point=%s"),
                    Seed, Edge.A, Edge.B, *Point.ToString());
                return false;
            }
        }
    }
    const FVector2D Start = CanyonLayout.Nodes[CanyonLayout.SpawnNode].Position;
    const FVector2D Target = CanyonLayout.TreasurePosition;
    const float StartEye = HeightAt(Start.X, Start.Y) + 180.f;
    const float TargetEye = HeightAt(Target.X, Target.Y) + 180.f;
    for (int32 Step = 2; Step <= 30; ++Step)
    {
        const float T = Step / 32.f;
        const FVector2D Point = FMath::Lerp(Start, Target, T);
        if (HeightAt(Point.X, Point.Y) > FMath::Lerp(StartEye, TargetEye, T) + 100.f) return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("TREASURE_CANYON_VALIDATION Seed=%d Failed=Sightline Topology=%s Treasure=%s"),
        Seed, FTreasureGameplayLayout::TopologyName(CanyonLayout.Topology), *Target.ToString());
    return false;
}

uint32 AProceduralIsland::GetCanyonLayoutHash() const
{
    return CanyonLayout.GetSignature();
}

float AProceduralIsland::NormalizedIslandDistance(float X, float Y) const
{
    const float Radius = CellSize * (GridSize - 1) * 0.46f;
    FRandomStream Profile(Seed ^ 0x51A7D);
    const float Rotation = Profile.FRandRange(-PI, PI);
    const float CosR = FMath::Cos(Rotation);
    const float SinR = FMath::Sin(Rotation);
    const float RX = X * CosR - Y * SinR;
    const float RY = X * SinR + Y * CosR;
    const float Angle = FMath::Atan2(RY, RX);
    const EIslandShape Shape = ShapeFromSeed(Seed);

    if (Shape == EIslandShape::LongSpine)
    {
        const float Wobble = 1.f + 0.10f * FMath::Sin(4.f * Angle + Seed * 0.013f);
        return FMath::Sqrt(FMath::Square(RX / 1.34f) + FMath::Square(RY / 0.72f)) / (Radius * Wobble);
    }
    if (Shape == EIslandShape::TwinCove)
    {
        const float Left = FMath::Sqrt(FMath::Square((RX + Radius * 0.27f) / 0.88f) + FMath::Square(RY / 0.88f)) / Radius;
        const float Right = FMath::Sqrt(FMath::Square((RX - Radius * 0.27f) / 0.88f) + FMath::Square(RY / 0.88f)) / Radius;
        const float Bridge = FMath::Sqrt(FMath::Square(RX / 1.08f) + FMath::Square(RY / 0.42f)) / Radius;
        return FMath::Min(FMath::Min(Left, Right), Bridge);
    }
    if (Shape == EIslandShape::Hook)
    {
        const float Main = FMath::Sqrt(FMath::Square((RX + Radius * 0.16f) / 1.05f) + FMath::Square(RY / 0.76f)) / Radius;
        const float Tip = FMath::Sqrt(FMath::Square((RX - Radius * 0.58f) / 0.48f) + FMath::Square((RY + Radius * 0.42f) / 0.42f)) / Radius;
        const float BayX = (RX - Radius * 0.43f) / (Radius * 0.34f);
        const float BayY = (RY - Radius * 0.02f) / (Radius * 0.34f);
        return FMath::Min(Main, Tip) + FMath::Exp(-(BayX * BayX + BayY * BayY) * 1.8f) * 0.78f;
    }
    if (Shape == EIslandShape::MainAndSatellite)
    {
        const float MainOffset = Profile.FRandRange(0.19f, 0.27f);
        const float MainX = Profile.FRandRange(0.64f, 0.72f);
        const float MainY = Profile.FRandRange(0.72f, 0.83f);
        const float SatelliteOffset = Profile.FRandRange(0.68f, 0.76f);
        const float SatelliteX = Profile.FRandRange(0.20f, 0.26f);
        const float SatelliteY = Profile.FRandRange(0.26f, 0.34f);
        const float SatelliteSide = Profile.FRandRange(-0.18f, 0.18f);
        const float Coast = 1.f + 0.065f * FMath::Sin(5.f * Angle + Seed * 0.016f)
            + 0.035f * FMath::Sin(3.f * Angle - Seed * 0.009f);
        const float Main = FMath::Sqrt(FMath::Square((RX + Radius * MainOffset) / MainX) + FMath::Square(RY / MainY)) / Radius;
        const float Satellite = FMath::Sqrt(FMath::Square((RX - Radius * SatelliteOffset) / SatelliteX)
            + FMath::Square((RY - Radius * SatelliteSide) / SatelliteY)) / Radius;
        return FMath::Min(Main, Satellite) / Coast;
    }
    if (Shape == EIslandShape::TwinIslands)
    {
        const float Separation = Profile.FRandRange(0.45f, 0.52f);
        const float WidthA = Profile.FRandRange(0.39f, 0.45f);
        const float WidthB = Profile.FRandRange(0.39f, 0.45f);
        const float HeightA = Profile.FRandRange(0.58f, 0.69f);
        const float HeightB = Profile.FRandRange(0.58f, 0.69f);
        const float Stagger = Profile.FRandRange(0.02f, 0.13f);
        const float Coast = 1.f + 0.06f * FMath::Sin(5.f * Angle + Seed * 0.015f);
        const float A = FMath::Sqrt(FMath::Square((RX + Radius * Separation) / WidthA)
            + FMath::Square((RY + Radius * Stagger) / HeightA)) / Radius;
        const float B = FMath::Sqrt(FMath::Square((RX - Radius * Separation) / WidthB)
            + FMath::Square((RY - Radius * Stagger) / HeightB)) / Radius;
        return FMath::Min(A, B) / Coast;
    }
    if (Shape == EIslandShape::ThreeIslets)
    {
        const float Side = Profile.FRandRange(0.40f, 0.47f);
        const float Bottom = Profile.FRandRange(0.21f, 0.29f);
        const float Top = Profile.FRandRange(0.44f, 0.53f);
        const float ScaleA = Profile.FRandRange(0.33f, 0.39f);
        const float ScaleB = Profile.FRandRange(0.33f, 0.39f);
        const float ScaleC = Profile.FRandRange(0.34f, 0.41f);
        const float Coast = 1.f + 0.07f * FMath::Sin(5.f * Angle + Seed * 0.014f)
            + 0.025f * FMath::Cos(3.f * Angle);
        const float A = FMath::Sqrt(FMath::Square((RX + Radius * Side) / ScaleA)
            + FMath::Square((RY + Radius * Bottom) / (ScaleA * 1.08f))) / Radius;
        const float B = FMath::Sqrt(FMath::Square((RX - Radius * Side) / ScaleB)
            + FMath::Square((RY + Radius * Bottom) / (ScaleB * 1.08f))) / Radius;
        const float C = FMath::Sqrt(FMath::Square(RX / ScaleC)
            + FMath::Square((RY - Radius * Top) / (ScaleC * 0.90f))) / Radius;
        return FMath::Min(FMath::Min(A, B), C) / Coast;
    }

    float Wobble = 1.f + 0.09f * FMath::Sin(5.f * Angle + Seed * 0.017f)
        + 0.055f * FMath::Sin(3.f * Angle - Seed * 0.011f);
    if (Shape == EIslandShape::TriCape)
        Wobble += 0.17f * FMath::Cos(3.f * Angle + Seed * 0.021f);
    else if (Shape == EIslandShape::StarCove)
        Wobble += 0.20f * FMath::Cos(4.f * Angle + Seed * 0.009f);

    const float AxisX = Shape == EIslandShape::RoundBay ? 1.05f : 1.12f;
    const float AxisY = Shape == EIslandShape::RoundBay ? 0.94f : 0.88f;
    float Distance = FMath::Sqrt(FMath::Square(RX / AxisX) + FMath::Square(RY / AxisY)) / (Radius * Wobble);
    if (Shape == EIslandShape::Crescent)
    {
        const float BayX = (RX - Radius * 0.50f) / (Radius * 0.47f);
        const float BayY = RY / (Radius * 0.52f);
        Distance += FMath::Exp(-(BayX * BayX + BayY * BayY) * 1.6f) * 0.73f;
    }
    else if (Shape == EIslandShape::RoundBay)
    {
        const float BayX = (RX + Radius * 0.72f) / (Radius * 0.28f);
        const float BayY = (RY - Radius * 0.08f) / (Radius * 0.34f);
        Distance += FMath::Exp(-(BayX * BayX + BayY * BayY) * 1.7f) * 0.58f;
    }
    else if (Shape == EIslandShape::StarCove)
    {
        const float BayX = (RX - Radius * 0.25f) / (Radius * 0.29f);
        const float BayY = (RY + Radius * 0.70f) / (Radius * 0.25f);
        Distance += FMath::Exp(-(BayX * BayX + BayY * BayY) * 1.8f) * 0.65f;
    }
    return Distance;
}

float AProceduralIsland::HeightAt(float X, float Y) const
{
    if (Theme == EIslandTheme::Canyon)
    {
        float BestDistanceSquared = TNumericLimits<float>::Max();
        float FloorHeight = 480.f;
        const FVector2D Point(X, Y);
        for (const FCanyonEdge& Edge : CanyonEdges)
        {
            const FCanyonNode& A = CanyonNodes[Edge.A];
            const FCanyonNode& B = CanyonNodes[Edge.B];
            const FVector2D Delta = B.Position - A.Position;
            const float T = FMath::Clamp(FVector2D::DotProduct(Point - A.Position, Delta) / Delta.SizeSquared(), 0.f, 1.f);
            const float DistanceSquared = FVector2D::DistSquared(Point, A.Position + T * Delta);
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                FloorHeight = FMath::Lerp(A.Height, B.Height, T);
            }
        }
        const float Distance = FMath::Sqrt(BestDistanceSquared);
        // The 19.4 m floor matches the module sockets; the rim rises beyond it.
        const float Wall = FMath::SmoothStep(970.f, 1600.f, Distance);
        return FloorHeight + 6500.f * Wall
            + 650.f * Wall * SeedNoise(X, Y, Seed + 809);
    }
    const float Edge = NormalizedIslandDistance(X, Y);
    if (Edge >= 1.f)
    {
        const EIslandShape Shape = ShapeFromSeed(Seed);
        const bool bSplitIsland = Shape == EIslandShape::MainAndSatellite
            || Shape == EIslandShape::TwinIslands || Shape == EIslandShape::ThreeIslets;
        // Keep the narrow gaps walkable as waist-deep water; the outer ocean still drops away quickly.
        if (bSplitIsland && Edge < 1.22f)
            return -55.f;
        return -180.f - (Edge - 1.f) * 520.f;
    }

    const float Radius = CellSize * (GridSize - 1) * 0.46f;
    if (Theme == EIslandTheme::MistForest)
    {
        const float ReliefScale = GetThemeDefinition(Theme).TerrainRelief;
        const float Interior = FMath::Clamp((1.f - Edge) * 3.2f, 0.f, 1.f);
        const float BroadRoll = 165.f * FMath::Sin(X / (Radius * 0.22f) + Seed * 0.013f)
            * FMath::Cos(Y / (Radius * 0.27f) - Seed * 0.017f);
        const float HighHill = 610.f * FMath::Exp(-(FMath::Square((X + Radius * 0.27f) / (Radius * 0.20f))
            + FMath::Square((Y - Radius * 0.20f) / (Radius * 0.24f))));
        const float LongRidge = 390.f * FMath::Exp(-(FMath::Square((X - Radius * 0.24f) / (Radius * 0.13f))
            + FMath::Square((Y + Radius * 0.08f) / (Radius * 0.48f))));
        const float Valley = -245.f * FMath::Exp(-(FMath::Square((X + Radius * 0.02f) / (Radius * 0.24f))
            + FMath::Square((Y + Radius * 0.30f) / (Radius * 0.18f))));
        const float RollingNoise = SeedNoise(X, Y, Seed + 1733) * 105.f;
        return 105.f + (BroadRoll + HighHill + LongRidge + Valley + RollingNoise)
            * Interior * ReliefScale;
    }
    if (Theme == EIslandTheme::JungleRuins)
    {
        const float ReliefScale = GetThemeDefinition(Theme).TerrainRelief;
        const float Interior = FMath::Clamp((1.f - Edge) * 2.7f, 0.f, 1.f);
        const float TerraceA = 115.f * FMath::Exp(-(FMath::Square((X + Radius * 0.23f) / (Radius * 0.34f))
            + FMath::Square((Y - Radius * 0.08f) / (Radius * 0.30f))));
        const float TerraceB = 75.f * FMath::Exp(-(FMath::Square((X - Radius * 0.34f) / (Radius * 0.28f))
            + FMath::Square((Y + Radius * 0.22f) / (Radius * 0.25f))));
        return 72.f + (TerraceA + TerraceB + SeedNoise(X, Y, Seed + 911) * 42.f)
            * Interior * ReliefScale;
    }
    FRandomStream Profile(Seed ^ 0x2F6E2B1);
    const ETerrainProfile Terrain = TerrainFromSeed(Seed);
    const float Interior = FMath::Clamp((1.f - Edge) * 2.4f, 0.f, 1.f);
    const float ShoreRise = 42.f + 95.f * FMath::Pow(FMath::Max(0.f, 1.f - Edge), 0.75f);
    auto Hill = [&](float CX, float CY, float SX, float SY, float Height)
    {
        const float DX = (X - CX) / SX;
        const float DY = (Y - CY) / SY;
        return Height * FMath::Exp(-(DX * DX + DY * DY));
    };

    float Relief = 0.f;
    switch (Terrain)
    {
    case ETerrainProfile::Flat:
        Relief = 90.f + SeedNoise(X, Y, Seed) * 45.f;
        break;
    case ETerrainProfile::SinglePeak:
        Relief = Hill(Profile.FRandRange(-0.58f, 0.58f) * Radius, Profile.FRandRange(-0.58f, 0.58f) * Radius,
            Radius * 0.25f, Radius * 0.25f, 620.f);
        break;
    case ETerrainProfile::TwinPeaks:
        Relief = Hill(-Radius * 0.34f, Radius * 0.18f, Radius * 0.22f, Radius * 0.25f, 510.f)
            + Hill(Radius * 0.34f, -Radius * 0.20f, Radius * 0.24f, Radius * 0.21f, 470.f);
        break;
    case ETerrainProfile::Ridge:
        Relief = Hill(0.f, 0.f, Radius * 0.62f, Radius * 0.13f, 500.f);
        break;
    case ETerrainProfile::EdgeCliff:
        Relief = Hill(Radius * 0.52f, Radius * -0.18f, Radius * 0.28f, Radius * 0.48f, 610.f);
        break;
    case ETerrainProfile::Basin:
        Relief = 330.f * Interior - Hill(0.f, 0.f, Radius * 0.25f, Radius * 0.25f, 285.f);
        break;
    default:
        Relief = Hill(-Radius * 0.42f, -Radius * 0.28f, Radius * 0.20f, Radius * 0.19f, 270.f)
            + Hill(Radius * 0.08f, Radius * 0.38f, Radius * 0.18f, Radius * 0.22f, 310.f)
            + Hill(Radius * 0.45f, -Radius * 0.06f, Radius * 0.17f, Radius * 0.18f, 245.f);
        break;
    }
    const float FineVariation = SeedNoise(X, Y, Seed + 419) * (Terrain == ETerrainProfile::Flat ? 35.f : 80.f) * Interior;
    return ShoreRise + Relief * Interior + FineVariation;
}

float AProceduralIsland::SlopeAt(float X, float Y) const
{
    constexpr float Step = 90.f;
    const float DX = (HeightAt(X + Step, Y) - HeightAt(X - Step, Y)) / (Step * 2.f);
    const float DY = (HeightAt(X, Y + Step) - HeightAt(X, Y - Step)) / (Step * 2.f);
    return FMath::Sqrt(DX * DX + DY * DY);
}

bool AProceduralIsland::IsClearOfDecorations(float X, float Y, float Radius) const
{
    constexpr float BucketSize = 1000.f;
    const int32 MinX = FMath::FloorToInt((X - Radius) / BucketSize);
    const int32 MaxX = FMath::FloorToInt((X + Radius) / BucketSize);
    const int32 MinY = FMath::FloorToInt((Y - Radius) / BucketSize);
    const int32 MaxY = FMath::FloorToInt((Y + Radius) / BucketSize);
    for (int32 BX = MinX; BX <= MaxX; ++BX)
        for (int32 BY = MinY; BY <= MaxY; ++BY)
            if (const TArray<FVector2D>* Points = OccupiedBuckets.Find(FIntPoint(BX, BY)))
                for (const FVector2D& Point : *Points)
                    if (FVector2D::DistSquared(Point, FVector2D(X, Y)) < Radius * Radius) return false;
    return true;
}

void AProceduralIsland::RecordDecoration(float X, float Y)
{
    OccupiedBuckets.FindOrAdd(FIntPoint(FMath::FloorToInt(X / 1000.f), FMath::FloorToInt(Y / 1000.f)))
        .Add(FVector2D(X, Y));
}

FVector AProceduralIsland::FindRandomLandPoint(FRandomStream& Stream, float MinimumHeight) const
{
    if (Theme == EIslandTheme::Canyon && CanyonNodes.IsValidIndex(CanyonGoalNode))
    {
        const FVector2D Point = CanyonLayout.TreasurePosition;
        return GetActorLocation() + FVector(Point, HeightAt(Point.X, Point.Y));
    }
    const float Extent = CellSize * (GridSize - 1) * 0.42f;
    const float Radius = CellSize * (GridSize - 1) * 0.46f;
    for (int32 Attempt = 0; Attempt < 500; ++Attempt)
    {
        const float X = Stream.FRandRange(-Extent, Extent);
        const float Y = Stream.FRandRange(-Extent, Extent);
        const float Z = HeightAt(X, Y);
        const bool bAwayFromSpawn = FVector2D::DistSquared(FVector2D(X, Y), FVector2D(-Radius * 0.68f, 0.f))
            > FMath::Square(1900.f);
        if (Z >= MinimumHeight && Z <= 760.f && SlopeAt(X, Y) < 0.42f
            && NormalizedIslandDistance(X, Y) < 0.84f && bAwayFromSpawn && IsClearOfDecorations(X, Y, 520.f))
            return GetActorLocation() + FVector(X, Y, Z);
    }
    return GetActorLocation() + FVector(0.f, 0.f, HeightAt(0.f, 0.f));
}

FVector AProceduralIsland::FindSpawnPoint(float LateralOffset) const
{
    if (Theme == EIslandTheme::Canyon && CanyonNodes.IsValidIndex(CanyonLayout.SpawnNode))
    {
        const FCanyonNode& Start = CanyonNodes[CanyonLayout.SpawnNode];
        return GetActorLocation() + FVector(Start.Position.X, Start.Position.Y + FMath::Clamp(LateralOffset, -300.f, 300.f), Start.Height + 180.f);
    }
    const float Radius = CellSize * (GridSize - 1) * 0.46f;
    const FVector2D Desired(-Radius * 0.68f, LateralOffset);
    FVector Best(0.f, 0.f, HeightAt(0.f, 0.f));
    float BestScore = TNumericLimits<float>::Max();
    for (int32 YStep = -18; YStep <= 18; ++YStep)
    {
        for (int32 XStep = -18; XStep <= 18; ++XStep)
        {
            const float X = Radius * XStep / 18.f;
            const float Y = Radius * YStep / 18.f;
            const float Z = HeightAt(X, Y);
            if (Z < 115.f || NormalizedIslandDistance(X, Y) > 0.84f || SlopeAt(X, Y) > 0.34f)
                continue;
            const float Score = FVector2D::DistSquared(FVector2D(X, Y), Desired);
            if (Score < BestScore)
            {
                BestScore = Score;
                Best = FVector(X, Y, Z);
            }
        }
    }
    Best.Z += 180.f;
    return GetActorLocation() + Best;
}

void AProceduralIsland::BuildIsland()
{
    if (Theme == EIslandTheme::Canyon) BuildCanyonTopology();
    IslandMesh->ClearAllMeshSections();
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
    TArray<FLinearColor> Colors;
    TArray<int32> SectionTriangles[6];
    const float Half = (GridSize - 1) * CellSize * 0.5f;

    for (int32 Y = 0; Y < GridSize; ++Y)
    {
        for (int32 X = 0; X < GridSize; ++X)
        {
            const float WX = X * CellSize - Half;
            const float WY = Y * CellSize - Half;
            const float Z = HeightAt(WX, WY);
            const float DX = HeightAt(WX + 60.f, WY) - HeightAt(WX - 60.f, WY);
            const float DY = HeightAt(WX, WY + 60.f) - HeightAt(WX, WY - 60.f);
            Vertices.Add(FVector(WX, WY, Z));
            Normals.Add(FVector(-DX / 120.f, -DY / 120.f, 1.f).GetSafeNormal());
            UVs.Add(FVector2D(static_cast<float>(X) / (GridSize - 1), static_cast<float>(Y) / (GridSize - 1)));
            Colors.Add(FLinearColor::White);
            Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
        }
    }

    for (int32 Y = 0; Y < GridSize - 1; ++Y)
    {
        for (int32 X = 0; X < GridSize - 1; ++X)
        {
            const int32 I = Y * GridSize + X;
            const float WX = (X + 0.5f) * CellSize - Half;
            const float WY = (Y + 0.5f) * CellSize - Half;
            const float Z = HeightAt(WX, WY);
            const float Slope = SlopeAt(WX, WY);
            const float Moisture = SeedNoise(WX + 1700.f, WY - 900.f, Seed + 73);
            int32 Section = 1;
            if (Theme == EIslandTheme::Canyon)
            {
                float DistanceSquared = TNumericLimits<float>::Max();
                ETreasureRegion ClosestRegion = ETreasureRegion::CanyonFloor;
                const FVector2D Point(WX, WY);
                for (const FCanyonEdge& Edge : CanyonEdges)
                {
                    const FVector2D A = CanyonNodes[Edge.A].Position;
                    const FVector2D Delta = CanyonNodes[Edge.B].Position - A;
                    const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, Delta) / Delta.SizeSquared(), 0.f, 1.f);
                    const float Candidate = FVector2D::DistSquared(Point, A + T * Delta);
                    if (Candidate < DistanceSquared)
                    {
                        DistanceSquared = Candidate;
                        ClosestRegion = CanyonLayout.Nodes[T < 0.5f ? Edge.A : Edge.B].Region;
                    }
                }
                const float Band = Z + SeedNoise(WX, WY, Seed + 317) * 360.f;
                Section = DistanceSquared < FMath::Square(970.f)
                    ? ClosestRegion == ETreasureRegion::Mesa ? 4
                        : ClosestRegion == ETreasureRegion::RockField ? 5 : 0
                    : Band < 2600.f ? 1 : Band < 4900.f ? 2 : 3;
            }
            else if (Theme == EIslandTheme::MistForest)
            {
                const float Edge = NormalizedIslandDistance(WX, WY);
                if (Edge > 0.94f) Section = 3;
                else if (Slope > 0.40f) Section = 3;
                else if (Moisture > 0.10f) Section = 2;
            }
            else if (Theme == EIslandTheme::JungleRuins)
            {
                // Ruins are a jungle island: sand is only a narrow outer shoreline.
                const float Edge = NormalizedIslandDistance(WX, WY);
                if (Edge > 0.91f) Section = 0;
                else if (Slope > 0.46f) Section = 3;
                else if (Moisture > 0.06f) Section = 2;
            }
            else
            {
                if (Z < 125.f) Section = 0;
                else if (Slope > 0.48f || Z > 650.f) Section = 3;
                else if (Moisture > 0.18f) Section = 2;
            }
            SectionTriangles[Section].Append({ I, I + GridSize, I + 1, I + 1, I + GridSize, I + GridSize + 1 });
        }
    }

    const int32 Palette = FMath::Abs(Seed / 5) % 3;
    const FLinearColor SandColors[] = { FLinearColor(0.72f, 0.56f, 0.30f), FLinearColor(0.80f, 0.69f, 0.45f), FLinearColor(0.64f, 0.49f, 0.27f) };
    const FLinearColor GrassColors[] = { FLinearColor(0.16f, 0.40f, 0.08f), FLinearColor(0.24f, 0.46f, 0.12f), FLinearColor(0.31f, 0.39f, 0.09f) };
    const FLinearColor DarkGrassColors[] = { FLinearColor(0.07f, 0.25f, 0.06f), FLinearColor(0.10f, 0.31f, 0.12f), FLinearColor(0.18f, 0.27f, 0.06f) };
    const FLinearColor RockColors[] = { FLinearColor(0.27f, 0.25f, 0.21f), FLinearColor(0.34f, 0.32f, 0.28f), FLinearColor(0.29f, 0.25f, 0.20f) };
    const FLinearColor JungleColors[] = { FLinearColor(0.62f,0.48f,0.25f), FLinearColor(0.12f,0.34f,0.07f), FLinearColor(0.035f,0.20f,0.045f), FLinearColor(0.19f,0.27f,0.15f) };
    const FLinearColor ForestColors[] = { FLinearColor(0.08f,0.17f,0.045f), FLinearColor(0.07f,0.22f,0.055f), FLinearColor(0.025f,0.13f,0.035f), FLinearColor(0.13f,0.18f,0.12f) };
    const FLinearColor CanyonColors[] = {
        FLinearColor(0.57f,0.27f,0.12f), FLinearColor(0.38f,0.19f,0.11f),
        FLinearColor(0.61f,0.35f,0.19f), FLinearColor(0.35f,0.21f,0.15f),
        FLinearColor(0.72f,0.49f,0.28f), FLinearColor(0.40f,0.31f,0.25f) };
    const FLinearColor BeachColors[] = { SandColors[Palette], GrassColors[Palette], DarkGrassColors[Palette], RockColors[Palette] };
    const FLinearColor* SurfaceColors = Theme == EIslandTheme::Canyon ? CanyonColors
        : Theme == EIslandTheme::MistForest ? ForestColors
        : Theme == EIslandTheme::JungleRuins ? JungleColors : BeachColors;

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    for (int32 Section = 0; Section < 6; ++Section)
    {
        IslandMesh->CreateMeshSection_LinearColor(Section, Vertices, SectionTriangles[Section], Normals, UVs, Colors, Tangents, true);
        if (BaseMaterial)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            Material->SetVectorParameterValue(TEXT("Color"), SurfaceColors[Theme == EIslandTheme::Canyon ? Section : FMath::Min(Section, 3)]);
            IslandMesh->SetMaterial(Section, Material);
        }
    }
    IslandMesh->ContainsPhysicsTriMeshData(true);
}

void AProceduralIsland::BuildCanyonDecorations()
{
    const TCHAR* Variants[] = {
        TEXT("Straight_A"), TEXT("Straight_B_Wide"), TEXT("Straight_C_Narrow"),
        TEXT("Rise_3m"), TEXT("Fall_3m")
    };
    for (int32 Variant = 0; Variant < 5; ++Variant)
    {
        const FString Suffix(Variants[Variant]);
        const FString Names[] = {
            TEXT("SM_CanyonFloor_") + Suffix,
            TEXT("SM_CanyonWall_L_") + Suffix,
            TEXT("SM_CanyonWall_R_") + Suffix
        };
        for (int32 Part = 0; Part < 3; ++Part)
        {
            UHierarchicalInstancedStaticMeshComponent* Component = CanyonInstances[Variant * 3 + Part];
            if (!Component->GetStaticMesh())
            {
                const FString Path = FString::Printf(TEXT("/Game/IslandAssets/CanyonModules/%s/StaticMeshes/%s.%s"),
                    *Names[Part], *Names[Part], *Names[Part]);
                Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Path));
                if (!Component->GetStaticMesh()) UE_LOG(LogTemp, Error, TEXT("TREASURE_CANYON_ASSET_MISSING %s"), *Path);
            }
        }
    }
    for (int32 Rock = 0; Rock < 3; ++Rock)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = CanyonInstances[15 + Rock];
        if (!Component->GetStaticMesh())
        {
            const FString Name = FString::Printf(TEXT("SM_CanyonTalus_%02d"), Rock + 1);
            const FString Path = FString::Printf(TEXT("/Game/IslandAssets/CanyonModules/%s/StaticMeshes/%s.%s"), *Name, *Name, *Name);
            Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Path));
        }
    }
    if (UStaticMesh* FloorMesh = CanyonInstances[0]->GetStaticMesh())
    {
        const FBoxSphereBounds Bounds = FloorMesh->GetBounds();
        UE_LOG(LogTemp, Display, TEXT("TREASURE_CANYON_ASSET_BOUNDS FloorLength=%.0f FloorWidth=%.0f"),
            Bounds.BoxExtent.X * 2.f, Bounds.BoxExtent.Y * 2.f);
    }

    auto HasSideExit = [&](int32 Node, int32 Other, const FVector2D& Direction, bool bLeft)
    {
        for (const FCanyonEdge& Incident : CanyonEdges)
        {
            const int32 Neighbor = Incident.A == Node ? Incident.B : Incident.B == Node ? Incident.A : INDEX_NONE;
            if (Neighbor != INDEX_NONE && Neighbor != Other)
            {
                const FVector2D Turn = CanyonNodes[Neighbor].Position - CanyonNodes[Node].Position;
                const float Cross = Direction.X * Turn.Y - Direction.Y * Turn.X;
                if (bLeft ? Cross > 0.f : Cross < 0.f) return true;
            }
        }
        return false;
    };

    FRandomStream Stream(Seed ^ 0x6C2852);
    int32 WallPieces = 0;
    for (const FCanyonEdge& Edge : CanyonEdges)
    {
        const FCanyonNode& A = CanyonNodes[Edge.A];
        const FCanyonNode& B = CanyonNodes[Edge.B];
        const float Rise = B.Height - A.Height;
        const int32 Variant = Rise > 150.f ? 3 : Rise < -150.f ? 4 : Stream.RandRange(0, 2);
        const FVector2D Delta = B.Position - A.Position;
        const FRotator Rotation(0.f, FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0.f);
        const FTransform Transform(Rotation, FVector(A.Position.X, A.Position.Y, A.Height + 8.f));
        CanyonInstances[Variant * 3]->AddInstance(Transform);
        // Only omit the wall on the side where another route enters a junction.
        const FVector2D Direction = Delta.GetSafeNormal();
        if (!HasSideExit(Edge.A, Edge.B, Direction, true) && !HasSideExit(Edge.B, Edge.A, Direction, true))
        {
            CanyonInstances[Variant * 3 + 1]->AddInstance(Transform);
            ++WallPieces;
        }
        if (!HasSideExit(Edge.A, Edge.B, Direction, false) && !HasSideExit(Edge.B, Edge.A, Direction, false))
        {
            CanyonInstances[Variant * 3 + 2]->AddInstance(Transform);
            ++WallPieces;
        }
    }
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UHierarchicalInstancedStaticMeshComponent* Components[] = {
            CanyonLandmarkCubes.Get(), CanyonLandmarkColumns.Get(), CanyonLandmarkBoulders.Get() };
        const FLinearColor Colors[] = {
            FLinearColor(0.48f, 0.21f, 0.11f), FLinearColor(0.70f, 0.41f, 0.24f), FLinearColor(0.31f, 0.19f, 0.14f) };
        for (int32 Index = 0; Index < 3; ++Index)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
            Material->SetVectorParameterValue(TEXT("Color"), Colors[Index]);
            Components[Index]->SetMaterial(0, Material);
        }
    }
    int32 LandmarkCount = 0;
    for (const FTreasureLayoutNode& Node : CanyonLayout.Nodes)
    {
        if (Node.Landmark == ETreasureLandmark::None) continue;
        ++LandmarkCount;
        const bool bSide = Node.Landmark != ETreasureLandmark::StoneArch && Node.Landmark != ETreasureLandmark::BrokenBridge;
        const FVector2D Center = Node.Position + FVector2D(0.f, bSide ? (Node.Cell.Y & 1 ? 680.f : -680.f) : 0.f);
        const float Ground = HeightAt(Center.X, Center.Y);
        auto Part = [&](UHierarchicalInstancedStaticMeshComponent* Component,
            float X, float Y, float Z, float SX, float SY, float SZ, float Yaw = 0.f)
        {
            Component->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f),
                FVector(Center.X + X, Center.Y + Y, Ground + Z), FVector(SX, SY, SZ)));
        };
        switch (Node.Landmark)
        {
        case ETreasureLandmark::StoneArch:
            Part(CanyonLandmarkColumns, 0, -500, 700, 2.6f, 2.6f, 14.f);
            Part(CanyonLandmarkColumns, 0, 500, 700, 2.6f, 2.6f, 14.f);
            Part(CanyonLandmarkCubes, 0, 0, 1490, 3.5f, 13.f, 2.2f); break;
        case ETreasureLandmark::TwinSpires:
            Part(CanyonLandmarkColumns, -350, -160, 900, 3.3f, 3.3f, 18.f);
            Part(CanyonLandmarkColumns, 350, 160, 650, 2.5f, 2.5f, 13.f); break;
        case ETreasureLandmark::BrokenBridge:
            Part(CanyonLandmarkCubes, -450, -520, 550, 2.8f, 3.f, 11.f);
            Part(CanyonLandmarkCubes, 450, 520, 550, 2.8f, 3.f, 11.f);
            Part(CanyonLandmarkCubes, -150, 0, 1110, 7.f, 3.5f, 1.2f, 18.f); break;
        case ETreasureLandmark::GiantSkull:
            Part(CanyonLandmarkBoulders, 0, 0, 550, 9.f, 7.f, 9.f);
            Part(CanyonLandmarkCubes, 0, 0, 180, 6.5f, 5.f, 2.7f);
            Part(CanyonLandmarkColumns, -260, -250, 160, 0.8f, 0.8f, 3.2f);
            Part(CanyonLandmarkColumns, -260, 250, 160, 0.8f, 0.8f, 3.2f); break;
        case ETreasureLandmark::StoneRing:
            for (int32 I = 0; I < 6; ++I)
            {
                const float Angle = 2.f * PI * I / 6.f;
                Part(CanyonLandmarkColumns, 470.f * FMath::Cos(Angle), 470.f * FMath::Sin(Angle),
                    340, 1.5f, 1.5f, 6.8f);
            }
            break;
        case ETreasureLandmark::Watchtower:
            Part(CanyonLandmarkCubes, 0, 0, 880, 4.5f, 4.5f, 17.6f);
            Part(CanyonLandmarkCubes, 0, 0, 1820, 7.f, 7.f, 2.f);
            Part(CanyonLandmarkColumns, -240, -240, 2150, 1.5f, 1.5f, 6.f);
            Part(CanyonLandmarkColumns, 240, 240, 2150, 1.5f, 1.5f, 6.f); break;
        case ETreasureLandmark::Cairn:
            Part(CanyonLandmarkCubes, 0, 0, 200, 7.f, 5.f, 4.f, 10.f);
            Part(CanyonLandmarkCubes, 80, 0, 540, 5.f, 4.f, 3.f, -17.f);
            Part(CanyonLandmarkCubes, -50, 20, 810, 3.7f, 3.f, 2.8f, 26.f); break;
        case ETreasureLandmark::BalancedBoulder:
            Part(CanyonLandmarkColumns, 0, 0, 450, 2.8f, 2.8f, 9.f);
            Part(CanyonLandmarkBoulders, 120, 0, 1180, 7.f, 6.f, 5.5f); break;
        default: break;
        }
    }
    // Small talus remains secondary detail and follows the graph's rock-field regions.
    for (int32 Index = 0; Index < CanyonEdges.Num(); ++Index)
    {
        const FCanyonEdge& Edge = CanyonEdges[Index];
        if (CanyonLayout.Nodes[Edge.A].Region != ETreasureRegion::RockField
            && CanyonLayout.Nodes[Edge.B].Region != ETreasureRegion::RockField) continue;
        const FVector2D Mid = (CanyonNodes[Edge.A].Position + CanyonNodes[Edge.B].Position) * 0.5f;
        const FVector2D Direction = (CanyonNodes[Edge.B].Position - CanyonNodes[Edge.A].Position).GetSafeNormal();
        const FVector2D Side(-Direction.Y, Direction.X);
        const FVector2D Spot = Mid + Side * (Index & 1 ? 1250.f : -1250.f);
        const int32 Variant = (Seed + Index * 7) % 3;
        CanyonInstances[15 + FMath::Abs(Variant)]->AddInstance(FTransform(
            FRotator(0.f, Index * 37.f, 0.f), FVector(Spot, HeightAt(Spot.X, Spot.Y) - 70.f), FVector(0.65f)));
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_CANYON_MODULES Seed=%d FloorTiles=%d WallPieces=%d"),
        Seed, CanyonEdges.Num(), WallPieces);
    UE_LOG(LogTemp, Display, TEXT("TREASURE_CANYON_LANDMARKS Seed=%d Count=%d Treasure=%s"),
        Seed, LandmarkCount, *CanyonLayout.TreasurePosition.ToString());
}

void AProceduralIsland::BuildDecorations()
{
    PalmInstances->ClearInstances();
    PalmCollisionInstances->ClearInstances();
    RockInstances->ClearInstances();
    BushInstances->ClearInstances();
    DriftwoodInstances->ClearInstances();
    SkullIdolInstances->ClearInstances();
    FaceIdolInstances->ClearInstances();
    GiantAnchorInstances->ClearInstances();
    ShipwreckInstances->ClearInstances();
    BrokenMastInstances->ClearInstances();
    StoneRingInstances->ClearInstances();
    CampfireInstances->ClearInstances();
    for (UHierarchicalInstancedStaticMeshComponent* Component : JungleInstances) Component->ClearInstances();
    for (UHierarchicalInstancedStaticMeshComponent* Component : ForestInstances) Component->ClearInstances();
    for (UHierarchicalInstancedStaticMeshComponent* Component : CanyonInstances) Component->ClearInstances();
    CanyonLandmarkCubes->ClearInstances();
    CanyonLandmarkColumns->ClearInstances();
    CanyonLandmarkBoulders->ClearInstances();
    JungleTreeCollisionInstances->ClearInstances();
    OccupiedBuckets.Reset();
    if (Theme == EIslandTheme::Canyon)
    {
        BuildCanyonDecorations();
        return;
    }
    if (Theme == EIslandTheme::JungleRuins)
    {
        BuildJungleDecorations();
        return;
    }
    if (Theme == EIslandTheme::MistForest)
    {
        BuildForestDecorations();
        return;
    }
    ApplyDecorationMaterials();

    FRandomStream Stream(Seed ^ 0x79B4A31);
    const float Extent = CellSize * (GridSize - 1) * 0.43f;
    auto TryPlace = [&](UHierarchicalInstancedStaticMeshComponent* Component, int32 TargetCount,
        float MinHeight, float MaxHeight, float MaxSlope, float MinSpacing, FVector2D ScaleRange, bool bBeachOnly)
    {
        TargetCount = ScaledDecorationCount(TargetCount);
        int32 Placed = 0;
        for (int32 Attempt = 0; Attempt < TargetCount * 35 && Placed < TargetCount; ++Attempt)
        {
            const float X = Stream.FRandRange(-Extent, Extent);
            const float Y = Stream.FRandRange(-Extent, Extent);
            const float Z = HeightAt(X, Y);
            if (Z < MinHeight || Z > MaxHeight || SlopeAt(X, Y) > MaxSlope || NormalizedIslandDistance(X, Y) >= 0.93f)
                continue;
            if (X < -3150.f && FMath::Abs(Y) < 1050.f)
                continue;
            if (!IsClearOfDecorations(X, Y, MinSpacing))
                continue;
            if (!bBeachOnly && SeedNoise(X - 600.f, Y + 1200.f, Seed + 211) < -0.22f)
                continue;

            const float UniformScale = Stream.FRandRange(ScaleRange.X, ScaleRange.Y);
            const FRotator Rotation(0.f, Stream.FRandRange(0.f, 360.f), 0.f);
            Component->AddInstance(FTransform(Rotation, FVector(X, Y, Z), FVector(UniformScale)));
            if (Component == PalmInstances)
            {
                const FVector CollisionLocation(X, Y, Z + 380.f * UniformScale);
                const FVector CollisionScale(0.70f * UniformScale, 0.70f * UniformScale, 7.60f * UniformScale);
                PalmCollisionInstances->AddInstance(FTransform(Rotation, CollisionLocation, CollisionScale));
            }
            RecordDecoration(X, Y);
            ++Placed;
        }
    };

    // Reserve the large silhouettes first so every seed reliably contains all seven landmarks.
    BuildLandmarks(Stream);
    TryPlace(RockInstances, 15, 135.f, 760.f, 0.58f, 430.f, FVector2D(0.78f, 1.45f), false);
    TryPlace(PalmInstances, 28, 125.f, 530.f, 0.32f, 500.f, FVector2D(0.82f, 1.22f), false);
    TryPlace(BushInstances, 38, 120.f, 570.f, 0.40f, 260.f, FVector2D(0.72f, 1.28f), false);
    TryPlace(DriftwoodInstances, 10, 45.f, 145.f, 0.30f, 520.f, FVector2D(0.82f, 1.25f), true);

    UE_LOG(LogTemp, Display, TEXT("TREASURE_ISLAND_BUILT Seed=%d Shape=%s Palms=%d Rocks=%d Bushes=%d Driftwood=%d Landmarks=7"),
        Seed, *GetShapeName(), PalmInstances->GetInstanceCount(), RockInstances->GetInstanceCount(), BushInstances->GetInstanceCount(), DriftwoodInstances->GetInstanceCount());
}

void AProceduralIsland::BuildJungleDecorations()
{
    FRandomStream Stream(Seed ^ 0x4A71C9);
    const FIslandThemeDefinition& Definition = GetThemeDefinition(Theme);
    const float Extent = CellSize * (GridSize - 1) * 0.39f;
    auto Place = [&](int32 Asset, int32 Count, float Spacing, float MaxSlope, FVector2D ScaleRange)
    {
        Count = ScaledDecorationCount(Count);
        int32 Placed = 0;
        for (int32 Attempt = 0; Attempt < Count * 70 && Placed < Count; ++Attempt)
        {
            const float X = Stream.FRandRange(-Extent, Extent), Y = Stream.FRandRange(-Extent, Extent);
            const float Z = HeightAt(X, Y);
            if (Z < 80.f || NormalizedIslandDistance(X,Y) > 0.86f || SlopeAt(X,Y) > MaxSlope
                || (X < -3150.f && FMath::Abs(Y) < 1050.f) || !IsClearOfDecorations(X,Y,Spacing)) continue;
            const float S = Stream.FRandRange(ScaleRange.X, ScaleRange.Y);
            const FRotator Rotation(0, Stream.FRandRange(0,360), 0);
            JungleInstances[Asset]->AddInstance(FTransform(Rotation, FVector(X,Y,Z), FVector(S)));
            if (Asset == ButtressTree || Asset == ForkedTree)
            {
                const float Height = Asset == ButtressTree ? 760.f : 620.f;
                const float RadiusScale = Asset == ButtressTree ? 1.05f : 0.80f;
                const FVector CollisionLocation(X, Y, Z + Height * S * 0.5f);
                const FVector CollisionScale(RadiusScale * S, RadiusScale * S, Height * S / 100.f);
                JungleTreeCollisionInstances->AddInstance(FTransform(Rotation, CollisionLocation, CollisionScale));
            }
            RecordDecoration(X, Y); ++Placed;
        }
    };
    Place(Temple, 1, 1800.f, 0.16f, FVector2D(0.95f,1.08f));
    Place(Watchtower, 1, 1500.f, 0.18f, FVector2D(0.92f,1.05f));
    Place(ExplorerHut, 1, 1300.f, 0.20f, FVector2D(0.92f,1.08f));
    Place(ShrineHall, 1, 1550.f, 0.17f, FVector2D(0.92f,1.04f));
    Place(Crypt, 1, 1300.f, 0.22f, FVector2D(0.92f,1.08f));
    Place(ButtressTree, FMath::RoundToInt(20 * Definition.TreeDensity), 560.f, 0.30f, FVector2D(0.82f,1.16f));
    Place(ForkedTree, FMath::RoundToInt(25 * Definition.TreeDensity), 480.f, 0.34f, FVector2D(0.80f,1.18f));
    Place(JungleBush, FMath::RoundToInt(70 * Definition.BushDensity), 210.f, 0.42f, FVector2D(0.72f,1.30f));
    Place(Fern, FMath::RoundToInt(95 * Definition.BushDensity), 125.f, 0.48f, FVector2D(0.65f,1.25f));
    Place(FallenLog, 8, 620.f, 0.28f, FVector2D(0.85f,1.20f));
    UE_LOG(LogTemp, Display, TEXT("TREASURE_JUNGLE_BUILT Seed=%d Theme=%s"), Seed, *GetThemeName());
}

void AProceduralIsland::BuildForestDecorations()
{
    FRandomStream Stream(Seed ^ 0x71F09B);
    const float Radius = CellSize * (GridSize - 1) * 0.46f;
    const float Extent = Radius * 0.90f;
    const FVector2D Clearings[] = {
        FVector2D(-Radius * 0.28f, Radius * 0.18f),
        FVector2D(Radius * 0.08f, -Radius * 0.22f),
        FVector2D(Radius * 0.34f, Radius * 0.20f)
    };
    auto IsForestOpening = [&](float X, float Y, float ExtraRadius)
    {
        const float PathY = FMath::Sin(X * 0.00055f + Seed * 0.031f) * Radius * 0.11f;
        if (FMath::Abs(Y - PathY) < 380.f + ExtraRadius) return true;
        for (const FVector2D& Clearing : Clearings)
            if (FVector2D::DistSquared(FVector2D(X,Y), Clearing) < FMath::Square(1050.f + ExtraRadius))
                return true;
        return false;
    };

    auto Place = [&](UHierarchicalInstancedStaticMeshComponent* Component, int32 AssetIndex,
        int32 Count, float Spacing, float MaxSlope, FVector2D ScaleRange, bool bAvoidOpenings, bool bTree)
    {
        Count = ScaledDecorationCount(Count);
        int32 Placed = 0;
        for (int32 Attempt = 0; Attempt < Count * 80 && Placed < Count; ++Attempt)
        {
            const float X = Stream.FRandRange(-Extent, Extent);
            const float Y = Stream.FRandRange(-Extent, Extent);
            const float Z = HeightAt(X, Y);
            if (Z < 65.f || NormalizedIslandDistance(X,Y) > 0.89f || SlopeAt(X,Y) > MaxSlope
                || (bAvoidOpenings && IsForestOpening(X,Y,0.f)) || !IsClearOfDecorations(X,Y,Spacing)) continue;
            const float S = Stream.FRandRange(ScaleRange.X, ScaleRange.Y);
            const float Yaw = Stream.FRandRange(0.f,360.f);
            FRotator Rotation(0.f, Yaw, 0.f);
            // Trees remain upright; low ground props follow the local slope so they do not float or cut across it.
            if (!bTree && AssetIndex != HollowStump)
            {
                constexpr float NormalStep = 120.f;
                const FVector SurfaceNormal(
                    HeightAt(X - NormalStep, Y) - HeightAt(X + NormalStep, Y),
                    HeightAt(X, Y - NormalStep) - HeightAt(X, Y + NormalStep),
                    NormalStep * 2.f);
                Rotation = FRotationMatrix::MakeFromZ(SurfaceNormal.GetSafeNormal()).Rotator();
                Rotation.Yaw += Yaw;
            }
            Component->AddInstance(FTransform(Rotation, FVector(X,Y,Z), FVector(S)));
            if (bTree)
            {
                const FVector CollisionLocation(X, Y, Z + 400.f * S);
                JungleTreeCollisionInstances->AddInstance(FTransform(Rotation, CollisionLocation,
                    FVector(1.05f * S, 1.05f * S, 8.0f * S)));
            }
            RecordDecoration(X, Y);
            ++Placed;
        }
    };

    // Large landmarks first, then dense canopy. Clearings and the winding path remain readable.
    Place(ForestInstances[SmallPond], SmallPond, 2, 2600.f, 0.12f, FVector2D(1.05f,1.45f), false, false);
    Place(ForestInstances[HollowStump], HollowStump, 7, 1250.f, 0.24f, FVector2D(0.90f,1.30f), false, false);
    Place(ForestInstances[MushroomRing], MushroomRing, 10, 950.f, 0.28f, FVector2D(0.85f,1.20f), false, false);
    Place(ForestInstances[MossBoulder], MossBoulder, 18, 720.f, 0.34f, FVector2D(0.85f,1.45f), false, false);
    Place(ForestInstances[SplitBoulder], SplitBoulder, 12, 900.f, 0.30f, FVector2D(0.90f,1.35f), false, false);
    Place(ForestInstances[ThreeStoneStack], ThreeStoneStack, 9, 1100.f, 0.25f, FVector2D(0.90f,1.25f), false, false);
    Place(ForestInstances[FlatStoneSlab], FlatStoneSlab, 13, 900.f, 0.30f, FVector2D(0.90f,1.35f), false, false);
    Place(ForestInstances[AncientOak], AncientOak, 105, 520.f, 0.30f, FVector2D(0.88f,1.28f), true, true);
    Place(ForestInstances[TwinTrunk], TwinTrunk, 115, 460.f, 0.32f, FVector2D(0.85f,1.25f), true, true);
    Place(ForestInstances[SpreadingBeech], SpreadingBeech, 125, 440.f, 0.30f, FVector2D(0.82f,1.22f), true, true);
    Place(ForestInstances[WeepingTree], WeepingTree, 90, 560.f, 0.28f, FVector2D(0.88f,1.24f), true, true);
    Place(ForestInstances[ReedCluster], ReedCluster, 80, 190.f, 0.38f, FVector2D(0.75f,1.35f), false, false);
    Place(JungleInstances[JungleBush], -1, 180, 180.f, 0.40f, FVector2D(0.70f,1.25f), true, false);
    Place(JungleInstances[Fern], -1, 240, 120.f, 0.44f, FVector2D(0.65f,1.25f), true, false);
    ApplyDecorationMaterials();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_FOREST_BUILT Seed=%d Trees=%d MapDiameter=%.0fm"),
        Seed, ForestInstances[AncientOak]->GetInstanceCount() + ForestInstances[TwinTrunk]->GetInstanceCount()
        + ForestInstances[SpreadingBeech]->GetInstanceCount() + ForestInstances[WeepingTree]->GetInstanceCount(),
        Radius * 2.f / 100.f);
}

void AProceduralIsland::BuildLandmarks(FRandomStream& Stream)
{
    UHierarchicalInstancedStaticMeshComponent* LandmarkComponents[] = {
        SkullIdolInstances, FaceIdolInstances, GiantAnchorInstances, ShipwreckInstances,
        BrokenMastInstances, StoneRingInstances, CampfireInstances
    };
    const float Extent = CellSize * (GridSize - 1) * 0.40f;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(LandmarkComponents); ++Index)
    {
        for (int32 Copy = 0; Copy < ScaledDecorationCount(1); ++Copy)
        {
            for (int32 Attempt = 0; Attempt < 180; ++Attempt)
            {
                const float X = Stream.FRandRange(-Extent, Extent);
                const float Y = Stream.FRandRange(-Extent, Extent);
                const float Z = HeightAt(X, Y);
                if (Z < 115.f || Z > 610.f || SlopeAt(X, Y) > 0.30f || NormalizedIslandDistance(X, Y) > 0.80f)
                    continue;
                if (X < -3250.f && FMath::Abs(Y) < 1200.f)
                    continue;
                if (!IsClearOfDecorations(X, Y, 900.f))
                    continue;

                const float Scale = Stream.FRandRange(0.90f, 1.18f);
                const FRotator Rotation(0.f, Stream.FRandRange(0.f, 360.f), 0.f);
                LandmarkComponents[Index]->AddInstance(FTransform(Rotation, FVector(X, Y, Z), FVector(Scale)));
                RecordDecoration(X, Y);
                break;
            }
        }
    }
}

void AProceduralIsland::ApplyDecorationMaterials()
{
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!BaseMaterial) return;

    auto ApplyColors = [&](UHierarchicalInstancedStaticMeshComponent* Component)
    {
        UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
        if (!Mesh) return;
        const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
        for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
        {
            const FString SlotName = Slots[SlotIndex].MaterialSlotName.ToString();
            FColor SRGBColor(105, 112, 102);
            if (SlotName.Contains(TEXT("RuinInterior"))) SRGBColor = FColor(16, 21, 18);
            else if (SlotName.Contains(TEXT("RuinStoneLight"))) SRGBColor = FColor(142, 148, 119);
            else if (SlotName.Contains(TEXT("RuinStoneDark"))) SRGBColor = FColor(55, 65, 55);
            else if (SlotName.Contains(TEXT("RuinStone"))) SRGBColor = FColor(96, 108, 86);
            else if (SlotName.Contains(TEXT("RuinMoss")) || SlotName.Contains(TEXT("JungleMoss"))) SRGBColor = FColor(37, 103, 34);
            else if (SlotName.Contains(TEXT("RuinWood"))) SRGBColor = FColor(88, 47, 23);
            else if (SlotName.Contains(TEXT("RuinRoof"))) SRGBColor = FColor(70, 49, 31);
            else if (SlotName.Contains(TEXT("JungleBarkLight"))) SRGBColor = FColor(107, 64, 31);
            else if (SlotName.Contains(TEXT("JungleBark"))) SRGBColor = FColor(67, 39, 22);
            else if (SlotName.Contains(TEXT("JungleLeafDark"))) SRGBColor = FColor(18, 68, 27);
            else if (SlotName.Contains(TEXT("JungleLeaf"))) SRGBColor = FColor(34, 124, 47);
            else if (SlotName.Contains(TEXT("JungleFern"))) SRGBColor = FColor(45, 151, 55);
            else if (SlotName.Contains(TEXT("ForestBarkMoss"))) SRGBColor = FColor(55, 92, 34);
            else if (SlotName.Contains(TEXT("ForestBark")) || SlotName.Contains(TEXT("StumpBark"))) SRGBColor = FColor(57, 33, 20);
            else if (SlotName.Contains(TEXT("ForestLeafLight"))) SRGBColor = FColor(53, 137, 56);
            else if (SlotName.Contains(TEXT("ForestLeafDark"))) SRGBColor = FColor(12, 58, 23);
            else if (SlotName.Contains(TEXT("ForestLeaf"))) SRGBColor = FColor(25, 105, 39);
            else if (SlotName.Contains(TEXT("ForestMoss"))) SRGBColor = FColor(47, 116, 31);
            else if (SlotName.Contains(TEXT("ForestRockLight"))) SRGBColor = FColor(112, 122, 104);
            else if (SlotName.Contains(TEXT("ForestRockDark"))) SRGBColor = FColor(48, 55, 49);
            else if (SlotName.Contains(TEXT("ForestRock"))) SRGBColor = FColor(79, 89, 76);
            else if (SlotName.Contains(TEXT("ForestPondWater"))) SRGBColor = FColor(20, 104, 116);
            else if (SlotName.Contains(TEXT("ForestReed"))) SRGBColor = FColor(71, 126, 35);
            else if (SlotName.Contains(TEXT("ForestLily"))) SRGBColor = FColor(38, 137, 45);
            else if (SlotName.Contains(TEXT("ForestMushroomCap"))) SRGBColor = FColor(168, 35, 25);
            else if (SlotName.Contains(TEXT("ForestMushroomStem")) || SlotName.Contains(TEXT("StumpCut"))) SRGBColor = FColor(185, 147, 86);
            else if (SlotName.Contains(TEXT("Flame"))) SRGBColor = FColor(255, 132, 16);
            else if (SlotName.Contains(TEXT("Ember"))) SRGBColor = FColor(235, 48, 8);
            else if (SlotName.Contains(TEXT("TornSail"))) SRGBColor = FColor(92, 25, 21);
            else if (SlotName.Contains(TEXT("PirateMoss"))) SRGBColor = FColor(33, 93, 32);
            else if (SlotName.Contains(TEXT("PirateMetal"))) SRGBColor = FColor(45, 54, 58);
            else if (SlotName.Contains(TEXT("PirateStoneLight"))) SRGBColor = FColor(145, 151, 133);
            else if (SlotName.Contains(TEXT("PirateStoneDark"))) SRGBColor = FColor(61, 68, 64);
            else if (SlotName.Contains(TEXT("PirateStone"))) SRGBColor = FColor(99, 107, 98);
            else if (SlotName.Contains(TEXT("PirateWoodLight"))) SRGBColor = FColor(154, 88, 38);
            else if (SlotName.Contains(TEXT("PirateWood"))) SRGBColor = FColor(91, 43, 20);
            else if (SlotName.Contains(TEXT("LeafDark"))) SRGBColor = FColor(28, 82, 35);
            else if (SlotName.Contains(TEXT("Leaf"))) SRGBColor = FColor(52, 137, 61);
            else if (SlotName.Contains(TEXT("Driftwood"))) SRGBColor = FColor(142, 105, 70);
            else if (SlotName.Contains(TEXT("Wood"))) SRGBColor = FColor(92, 48, 24);
            else if (SlotName.Contains(TEXT("RockDark"))) SRGBColor = FColor(70, 76, 70);
            else if (SlotName.Contains(TEXT("Rock"))) SRGBColor = FColor(112, 119, 108);

            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            Material->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(SRGBColor));
            Component->SetMaterial(SlotIndex, Material);
        }
    };

    ApplyColors(PalmInstances);
    ApplyColors(RockInstances);
    ApplyColors(BushInstances);
    ApplyColors(DriftwoodInstances);
    ApplyColors(SkullIdolInstances);
    ApplyColors(FaceIdolInstances);
    ApplyColors(GiantAnchorInstances);
    ApplyColors(ShipwreckInstances);
    ApplyColors(BrokenMastInstances);
    ApplyColors(StoneRingInstances);
    ApplyColors(CampfireInstances);
    for (UHierarchicalInstancedStaticMeshComponent* JungleComponent : JungleInstances)
        ApplyColors(JungleComponent);
    for (UHierarchicalInstancedStaticMeshComponent* ForestComponent : ForestInstances)
        ApplyColors(ForestComponent);
}

void AProceduralIsland::BuildWater()
{
    WaterMesh->ClearAllMeshSections();
    if (Theme == EIslandTheme::Canyon) return;
    const float S = CellSize * GridSize * 0.9f;
    TArray<FVector> V = { {-S,-S,0.f}, {S,-S,0.f}, {-S,S,0.f}, {S,S,0.f} };
    TArray<int32> T = { 0,2,1, 1,2,3 };
    TArray<FVector> N; N.Init(FVector::UpVector, 4);
    TArray<FVector2D> UV = { {0,0},{1,0},{0,1},{1,1} };
    TArray<FLinearColor> C; C.Init(FLinearColor(0.02f, 0.18f, 0.32f, 0.82f), 4);
    TArray<FProcMeshTangent> Tan; Tan.Init(FProcMeshTangent(1,0,0), 4);
    WaterMesh->CreateMeshSection_LinearColor(0, V, T, N, UV, C, Tan, false);
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
        Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.015f, 0.16f, 0.34f, 1.f));
        WaterMesh->SetMaterial(0, Material);
    }
}
