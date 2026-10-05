#include "ProceduralIsland.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CanyonSolidMesh.h"
#include "Async/ParallelFor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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
    { EIslandTheme::CanyonGraybox, TEXT("CanyonGraybox"), 50, true, 0.f, 0.f, 1.f, 321, 39.1875f, 1.f },
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
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
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

    CanyonLandmarkBoxes = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CanyonLandmarkBoxes"));
    CanyonLandmarkBoxes->SetupAttachment(RootComponent);
    ConfigureInstances(CanyonLandmarkBoxes);
    CanyonLandmarkBoxes->SetStaticMesh(CubeMesh.Object);
    CanyonLandmarkCylinders = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CanyonLandmarkCylinders"));
    CanyonLandmarkCylinders->SetupAttachment(RootComponent);
    ConfigureInstances(CanyonLandmarkCylinders);
    CanyonLandmarkCylinders->SetStaticMesh(CylinderMesh.Object);
    CanyonCaveMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CanyonCaveMesh"));
    CanyonCaveMesh->SetupAttachment(RootComponent);
    CanyonCaveMesh->bUseComplexAsSimpleCollision = true;

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
}

void AProceduralIsland::ConfigureThemeParameters()
{
    if (Theme == EIslandTheme::CanyonGraybox)
    {
        MapScale = FMath::IsFinite(MapScale) ? FMath::Clamp(MapScale, 0.5f, 5.f) : 1.f;
        // Preserve map size, but use 16 times fewer surface cells.
        GridSize = 81;
        CellSize = 78.375f * FMath::Sqrt(MapScale);
        return;
    }
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
    if (Theme == EIslandTheme::CanyonGraybox)
    {
        TArray<FString> Types;
        for (int32 I = 0; I < CanyonLayout.CaveNetworks.Num(); ++I)
            Types.Add(CanyonLayout.CaveName(I));
        return FString::Printf(TEXT("%s / %s"), CanyonLayout.ProblemName(), *FString::Join(Types, TEXT(" + ")));
    }
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

float AProceduralIsland::GetFallRecoveryLimitZ() const
{
    float LowestWorldZ = GetActorLocation().Z;
    if (Theme == EIslandTheme::CanyonGraybox)
        for (const FCanyonGrayboxNode& Node : CanyonLayout.Nodes)
            LowestWorldZ = FMath::Min(LowestWorldZ, GetActorTransform().TransformPosition(
                Node.Position + FVector(0.f, 0.f, 600.f)).Z);
    // Keep recovery below every legal floor, including deep tunnels and mesh variation.
    return LowestWorldZ - 600.f;
}

float AProceduralIsland::HeightAt(float X, float Y) const
{
    if (Theme == EIslandTheme::CanyonGraybox) return CanyonLayout.SurfaceHeightAt(X, Y);
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

bool AProceduralIsland::IsCanyonRouteAt(float X, float Y) const
{
    if (Theme != EIslandTheme::CanyonGraybox || !CanyonLayout.Nodes.IsValidIndex(CanyonLayout.SpawnNode))
        return false;
    float Distance = 0.f;
    CanyonLayout.SurfaceHeightAt(X, Y, &Distance);
    return Distance < FMath::Max(320.f, 650.f * CanyonLayout.LengthScale);
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

bool AProceduralIsland::FindCanyonCaveFloor(const FVector& RoutePoint, FVector& Floor) const
{
    if (!GetWorld() || !CanyonCaveMesh) return false;
    const FVector Expected = GetActorTransform().TransformPosition(RoutePoint + FVector(0, 0, 600.f));
    FCollisionQueryParams Query;
    Query.bTraceComplex = true;
    FHitResult Ground, Roof;
    if (!GetWorld()->LineTraceSingleByChannel(Ground, Expected + FVector(0, 0, 160.f),
            Expected - FVector(0, 0, 180.f), ECC_Visibility, Query)
        || Ground.GetComponent() != CanyonCaveMesh || Ground.ImpactNormal.Z < 0.7f) return false;
    Floor = Ground.ImpactPoint;
    if (!GetWorld()->LineTraceSingleByChannel(Roof, Floor + FVector(0, 0, 200.f),
            Floor + FVector(0, 0, 1800.f), ECC_Visibility, Query)
        || Roof.GetComponent() != CanyonCaveMesh) return false;
    return !GetWorld()->OverlapBlockingTestByChannel(Floor + FVector(0, 0, 110.f),
        FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 96.f), Query);
}

FVector AProceduralIsland::FindTreasurePoint(FRandomStream& Stream, float MinimumHeight) const
{
    if (Theme == EIslandTheme::CanyonGraybox && CanyonLayout.Nodes.IsValidIndex(CanyonLayout.TreasureNode))
    {
        if (GetWorld() && Stream.FRand() < 0.4f)
        {
            TArray<int32> Candidates;
            for (int32 Index = 0; Index < CanyonLayout.Nodes.Num(); ++Index)
                if (CanyonLayout.Nodes[Index].bCaveInterior) Candidates.Add(Index);
            while (!Candidates.IsEmpty())
            {
                const int32 Choice = Stream.RandRange(0, Candidates.Num() - 1);
                const FVector ExpectedFloor = GetActorTransform().TransformPosition(
                    CanyonLayout.Nodes[Candidates[Choice]].Position + FVector(0.f, 0.f, 600.f));
                Candidates.RemoveAtSwap(Choice);
                FHitResult Ground, Ceiling;
                if (!GetWorld()->LineTraceSingleByChannel(Ground,
                        ExpectedFloor + FVector(0.f, 0.f, 200.f),
                        ExpectedFloor - FVector(0.f, 0.f, 250.f), ECC_Visibility)
                    || Ground.GetComponent() != CanyonCaveMesh || Ground.ImpactNormal.Z < 0.55f) continue;
                const FVector Floor = Ground.ImpactPoint;
                // Trace at tunnel height, so the overlying mountain cannot be mistaken for its floor.
                if (!GetWorld()->LineTraceSingleByChannel(Ceiling,
                        Floor + FVector(0.f, 0.f, 180.f), Floor + FVector(0.f, 0.f, 2200.f), ECC_Visibility)
                    || Ceiling.GetComponent() != CanyonCaveMesh) continue;
                if (GetWorld()->OverlapBlockingTestByChannel(Floor + FVector(0.f, 0.f, 130.f),
                        FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 96.f))) continue;
                return Floor;
            }
        }
        const FVector& Point = CanyonLayout.Nodes[CanyonLayout.TreasureNode].Position;
        return GetActorLocation() + FVector(Point.X, Point.Y, CanyonLayout.SurfaceHeightAt(Point.X, Point.Y));
    }
    return FindRandomLandPoint(Stream, MinimumHeight);
}

TArray<FVector> AProceduralIsland::FindSeparatedTreasurePoints(FRandomStream& Stream, int32 Count, float MinimumSpacing) const
{
    TArray<FVector> Candidates, CaveCandidates;
    // Enumerate real land positions. The single-treasure random helper can return
    // the same center fallback repeatedly, so it must not place multiple treasures.
    if (Theme == EIslandTheme::CanyonGraybox)
    {
        // Sample tunnel floors, never the mountain surface above them.
        for (const auto& Edge : CanyonLayout.Edges)
        {
            if (!Edge.bCave) continue;
            const FVector A = CanyonLayout.Nodes[Edge.A].Position, B = CanyonLayout.Nodes[Edge.B].Position;
            const int32 Steps = FMath::Max(2, FMath::CeilToInt(FVector::Dist2D(A, B) / 180.f));
            for (int32 Step = 0; Step <= Steps; ++Step)
            {
                FVector Floor;
                if (FindCanyonCaveFloor(FMath::Lerp(A, B, static_cast<float>(Step) / Steps), Floor))
                    CaveCandidates.Add(Floor);
            }
        }
        Candidates.Append(CaveCandidates);
        for (const auto& Node : CanyonLayout.Nodes)
        {
            if (Node.bCaveInterior || Node.Layer == ECanyonRouteLayer::Ramp) continue;
            const FVector& P = Node.Position;
            Candidates.Add(GetActorLocation() + FVector(P.X, P.Y, CanyonLayout.SurfaceHeightAt(P.X, P.Y)));
        }
        // Larger parties need more than the sparse route junctions. Sample
        // exterior route floors as well, without changing the three-prop layout.
        if (Count > 3)
            for (const auto& Edge : CanyonLayout.Edges)
            {
                if (Edge.bCave) continue;
                const auto& A = CanyonLayout.Nodes[Edge.A];
                const auto& B = CanyonLayout.Nodes[Edge.B];
                if (A.bCaveInterior || B.bCaveInterior) continue;
                const FVector Side = FVector::CrossProduct((B.Position - A.Position).GetSafeNormal2D(), FVector::UpVector);
                const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(A.Position, B.Position) / 150.f));
                for (int32 Step = 0; Step <= Steps; ++Step)
                    for (const float Offset : { -0.6f, 0.f, 0.6f })
                    {
                        const FVector P = FMath::Lerp(A.Position, B.Position, static_cast<float>(Step) / Steps)
                            + Side * (Offset * Edge.HalfWidth);
                        if (SlopeAt(P.X, P.Y) > 0.42f || !IsClearOfDecorations(P.X, P.Y, 100.f)) continue;
                        Candidates.Add(GetActorLocation() + FVector(P.X, P.Y, CanyonLayout.SurfaceHeightAt(P.X, P.Y)));
                    }
            }
    }
    else
    {
        const float Extent = CellSize * (GridSize - 1) * 0.46f;
        constexpr int32 Samples = 64;
        for (int32 Y = 0; Y < Samples; ++Y)
            for (int32 X = 0; X < Samples; ++X)
            {
                const float PX = FMath::Lerp(-Extent, Extent, (X + 0.5f) / Samples);
                const float PY = FMath::Lerp(-Extent, Extent, (Y + 0.5f) / Samples);
                const float PZ = HeightAt(PX, PY);
                // Allow clear, walkable land at any elevation, including high
                // forest hills; keep treasure above water and out of scenery.
                if (PZ < 30.f || NormalizedIslandDistance(PX, PY) > 0.9f
                    || SlopeAt(PX, PY) > 0.42f || !IsClearOfDecorations(PX, PY, 100.f)) continue;
                Candidates.Add(GetActorLocation() + FVector(PX, PY, PZ));
            }
    }
    TArray<FVector> Selected;
    if (Count <= 0 || Candidates.IsEmpty()) return Selected;
    // A greedy layout can get stuck even when another starting point fits.
    // Retry larger quotas; never relax spacing or use duplicate fallback points.
    const int32 Attempts = Theme == EIslandTheme::CanyonGraybox || Count > 3 ? 256 : 1;
    for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
    {
        Selected.Reset();
        // Prefer one cave treasure per hider; compact caves fall back to
        // fewer, but always at least one, without relaxing treasure spacing.
        const int32 CaveQuota = CaveCandidates.IsEmpty() ? 0 : FMath::Max(1, Count / 3 - Attempt / 86);
        if (CaveQuota > 0) Selected.Add(CaveCandidates[Stream.RandRange(0, CaveCandidates.Num() - 1)]);
        else Selected.Add(Candidates[Stream.RandRange(0, Candidates.Num() - 1)]);
        while (Selected.Num() < Count)
        {
            float BestSpacingSquared = FMath::Square(MinimumSpacing);
            int32 BestIndex = INDEX_NONE;
            int32 ValidCandidates = 0;
            const TArray<FVector>& Pool = Selected.Num() < CaveQuota ? CaveCandidates : Candidates;
            for (int32 Index = 0; Index < Pool.Num(); ++Index)
            {
                float NearestSquared = TNumericLimits<float>::Max();
                for (const FVector& Existing : Selected)
                    NearestSquared = FMath::Min(NearestSquared, FVector::DistSquared2D(Pool[Index], Existing));
                if (NearestSquared <= FMath::Square(MinimumSpacing)) continue;
                ++ValidCandidates;
                // Farthest-first is fast, but may waste space on compact routes.
                // Later retries vary all valid points to escape that same layout.
                if ((Attempt < 32 && NearestSquared > BestSpacingSquared)
                    || (Attempt >= 32 && Stream.RandRange(1, ValidCandidates) == 1))
                {
                    BestSpacingSquared = NearestSquared;
                    BestIndex = Index;
                }
            }
            // Never accept a duplicate or overlapping fallback to fill the quota.
            if (BestIndex == INDEX_NONE) break;
            Selected.Add(Pool[BestIndex]);
        }
        if (Selected.Num() == Count) return Selected;
    }
    return {};
}

FVector AProceduralIsland::FindRandomLandPoint(FRandomStream& Stream, float MinimumHeight) const
{
    if (Theme == EIslandTheme::CanyonGraybox && CanyonLayout.Validate())
    {
        TArray<int32> Candidates;
        for (int32 Index = 0; Index < CanyonLayout.Nodes.Num(); ++Index)
            if (Index != CanyonLayout.TreasureNode
                && CanyonLayout.Nodes[Index].Layer != ECanyonRouteLayer::Ramp
                && !CanyonLayout.Nodes[Index].bCaveInterior)
                Candidates.Add(Index);
        if (!Candidates.IsEmpty())
        {
            const FVector& Point = CanyonLayout.Nodes[Candidates[Stream.RandRange(0, Candidates.Num() - 1)]].Position;
            return GetActorLocation() + FVector(Point.X, Point.Y, CanyonLayout.SurfaceHeightAt(Point.X, Point.Y));
        }
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
    if (Theme == EIslandTheme::CanyonGraybox && CanyonLayout.Nodes.IsValidIndex(CanyonLayout.SpawnNode))
    {
        const FVector& Point = CanyonLayout.Nodes[CanyonLayout.SpawnNode].Position;
        const FVector Forward = (CanyonLayout.Nodes[1].Position - Point).GetSafeNormal2D();
        const FVector Right(-Forward.Y, Forward.X, 0.f);
        const int32 Slot = FMath::RoundToInt(FMath::Abs(LateralOffset) / 600.f);
        const float SideOffset = Slot == 0 ? 0.f
            : (LateralOffset < 0.f ? -1.f : 1.f) * (80.f + Slot * 20.f);
        const FVector Position = Point + Forward * (Slot * 180.f)
            + Right * SideOffset;
        return GetActorLocation() + FVector(Position.X, Position.Y,
            CanyonLayout.SurfaceHeightAt(Position.X, Position.Y) + 180.f);
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

FRotator AProceduralIsland::GetCanyonStartFacing() const
{
    if (Theme == EIslandTheme::CanyonGraybox && CanyonLayout.Nodes.Num() > 1)
        return (CanyonLayout.Nodes[1].Position - CanyonLayout.Nodes[CanyonLayout.SpawnNode].Position).Rotation();
    return FRotator::ZeroRotator;
}

void AProceduralIsland::BuildIsland()
{
    IslandMesh->ClearAllMeshSections();
    if (Theme == EIslandTheme::CanyonGraybox)
    {
        BuildCanyonGrayboxTerrain();
        return;
    }
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
    TArray<FLinearColor> Colors;
    TArray<int32> SectionTriangles[4];
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
            if (Theme == EIslandTheme::MistForest)
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
    const FLinearColor BeachColors[] = { SandColors[Palette], GrassColors[Palette], DarkGrassColors[Palette], RockColors[Palette] };
    const FLinearColor* SurfaceColors = Theme == EIslandTheme::MistForest ? ForestColors
        : Theme == EIslandTheme::JungleRuins ? JungleColors : BeachColors;

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    for (int32 Section = 0; Section < 4; ++Section)
    {
        IslandMesh->CreateMeshSection_LinearColor(Section, Vertices, SectionTriangles[Section], Normals, UVs, Colors, Tangents, true);
        if (BaseMaterial)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            Material->SetVectorParameterValue(TEXT("Color"), SurfaceColors[Section]);
            IslandMesh->SetMaterial(Section, Material);
        }
    }
    IslandMesh->ContainsPhysicsTriMeshData(true);
}

void AProceduralIsland::BuildDecorations()
{
    CanyonLandmarkBoxes->ClearInstances();
    CanyonLandmarkCylinders->ClearInstances();
    CanyonCaveMesh->ClearAllMeshSections();
    for (auto& Component : CanyonAssetInstances)
        if (Component) Component->DestroyComponent();
    CanyonAssetInstances.Reset();
    for (UPointLightComponent* Light : CanyonFillLights)
        if (Light) Light->DestroyComponent();
    CanyonFillLights.Empty();
    // Replicated Seed and Theme can arrive separately. A client may have built
    // the default beach first, so always discard its instances before Canyon.
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
    JungleTreeCollisionInstances->ClearInstances();
    OccupiedBuckets.Reset();
    if (Theme == EIslandTheme::CanyonGraybox)
    {
        BuildCanyonGrayboxLandmarks();
        BuildCanyonCaves();
        BuildCanyonAssets();
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
    if (Theme == EIslandTheme::CanyonGraybox) return;
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

void AProceduralIsland::BuildCanyonGrayboxTerrain()
{
    CanyonLayout = FCanyonGrayboxLayout::Generate(Seed, MapScale,
        FParse::Param(FCommandLine::Get(), TEXT("CanyonHallPreview")),
        FParse::Param(FCommandLine::Get(), TEXT("CanyonLoopPreview")),
        FParse::Param(FCommandLine::Get(), TEXT("CanyonBranchPreview")));
    CanyonLayout.ScaleForMap(MapScale);
    const bool bValid = CanyonLayout.Validate();
    UE_LOG(LogTemp, Display, TEXT("CANYON_CAVE_NETWORKS Seed=%d Count=%d Types=%s"),
        Seed, CanyonLayout.CaveNetworks.Num(), *GetShapeName());
    UE_LOG(LogTemp, Warning, TEXT("TREASURE_CANYON_GRAYBOX Seed=%d Scale=%.1f Problem=%s Cave=%s Upper=%d Nodes=%d Edges=%d Valid=%s"),
        Seed, MapScale, CanyonLayout.ProblemName(), CanyonLayout.CaveName(),
        static_cast<int32>(CanyonLayout.UpperPattern), CanyonLayout.Nodes.Num(), CanyonLayout.Edges.Num(),
        bValid ? TEXT("YES") : TEXT("NO"));
    if (!bValid) return;

    CanyonCavePatchCells = FIntRect(0, 0, 0, 0);
    if (CanyonLayout.CavePathNodes.Num() >= 4)
    {
        FVector2D Minimum(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
        FVector2D Maximum(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
        for (const FCanyonGrayboxEdge& Edge : CanyonLayout.Edges)
        {
            if (!Edge.bCave) continue;
            for (const int32 NodeIndex : { Edge.A, Edge.B })
            {
                const FVector& Point = CanyonLayout.Nodes[NodeIndex].Position;
                Minimum.X = FMath::Min(Minimum.X, Point.X);
                Minimum.Y = FMath::Min(Minimum.Y, Point.Y);
                Maximum.X = FMath::Max(Maximum.X, Point.X);
                Maximum.Y = FMath::Max(Maximum.Y, Point.Y);
            }
        }
        const float Padding = FMath::Max(900.f, 1100.f * CanyonLayout.LengthScale);
        const float GridHalf = (GridSize - 1) * CellSize * 0.5f;
        CanyonCavePatchCells.Min.X = FMath::Clamp(FMath::FloorToInt((Minimum.X - Padding + GridHalf) / CellSize), 0, GridSize - 2);
        CanyonCavePatchCells.Min.Y = FMath::Clamp(FMath::FloorToInt((Minimum.Y - Padding + GridHalf) / CellSize), 0, GridSize - 2);
        CanyonCavePatchCells.Max.X = FMath::Clamp(FMath::CeilToInt((Maximum.X + Padding + GridHalf) / CellSize), CanyonCavePatchCells.Min.X + 2, GridSize - 1);
        CanyonCavePatchCells.Max.Y = FMath::Clamp(FMath::CeilToInt((Maximum.Y + Padding + GridHalf) / CellSize), CanyonCavePatchCells.Min.Y + 2, GridSize - 1);
    }

    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles[4];
    const float Half = (GridSize - 1) * CellSize * 0.5f;
    const int32 VertexCount = GridSize * GridSize;
    Vertices.SetNumUninitialized(VertexCount);
    Normals.SetNumUninitialized(VertexCount);
    UVs.SetNumUninitialized(VertexCount);
    Colors.SetNumUninitialized(VertexCount);
    Tangents.SetNumUninitialized(VertexCount);
    ParallelFor(GridSize, [&](int32 Y)
    {
        for (int32 X = 0; X < GridSize; ++X)
        {
            const float WX = X * CellSize - Half, WY = Y * CellSize - Half;
            const float Z = CanyonLayout.SurfaceHeightAt(WX, WY);
            const int32 Index = Y * GridSize + X;
            Vertices[Index] = FVector(WX, WY, Z);
            UVs[Index] = FVector2D(X / 12.f, Y / 12.f);
            Colors[Index] = FLinearColor::White;
            Tangents[Index] = FProcMeshTangent(1.f, 0.f, 0.f);
        }
    });
    // Reuse sampled heights instead of evaluating the complete route graph
    // four more times per vertex just to derive the surface normal.
    ParallelFor(GridSize, [&](int32 Y)
    {
        for (int32 X = 0; X < GridSize; ++X)
        {
            const int32 X0 = FMath::Max(0, X - 1), X1 = FMath::Min(GridSize - 1, X + 1);
            const int32 Y0 = FMath::Max(0, Y - 1), Y1 = FMath::Min(GridSize - 1, Y + 1);
            const float DX = (Vertices[Y * GridSize + X1].Z - Vertices[Y * GridSize + X0].Z) / ((X1 - X0) * CellSize);
            const float DY = (Vertices[Y1 * GridSize + X].Z - Vertices[Y0 * GridSize + X].Z) / ((Y1 - Y0) * CellSize);
            Normals[Y * GridSize + X] = FVector(-DX, -DY, 1.f).GetSafeNormal();
        }
    });
    for (int32 Y = 0; Y < GridSize - 1; ++Y)
        for (int32 X = 0; X < GridSize - 1; ++X)
        {
            const int32 I = Y * GridSize + X;
            if (X >= CanyonCavePatchCells.Min.X && X < CanyonCavePatchCells.Max.X
                && Y >= CanyonCavePatchCells.Min.Y && Y < CanyonCavePatchCells.Max.Y) continue;
            float Distance = 0.f;
            ECanyonRouteLayer Layer = ECanyonRouteLayer::Lower;
            CanyonLayout.SurfaceHeightAt((X + 0.5f) * CellSize - Half,
                (Y + 0.5f) * CellSize - Half, &Distance, &Layer);
            const int32 Section = Distance < FMath::Max(320.f, 530.f * CanyonLayout.LengthScale)
                ? (Layer == ECanyonRouteLayer::Upper ? 1 : 0)
                : Distance < FMath::Max(650.f, 1000.f * CanyonLayout.LengthScale) ? 2 : 3;
            Triangles[Section].Append({ I, I + GridSize, I + 1, I + 1, I + GridSize, I + GridSize + 1 });
        }

    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    const FLinearColor Palette[] = {
        FLinearColor(0.60f, 0.49f, 0.34f), // lower route
        FLinearColor(0.72f, 0.62f, 0.42f), // upper wall-top route
        FLinearColor(0.46f, 0.23f, 0.16f), // steep banks
        FLinearColor(0.26f, 0.18f, 0.17f)  // high rock
    };
    for (int32 Section = 0; Section < 4; ++Section)
    {
        // CreateMeshSection rebuilds collision for every enabled section. Keep
        // the first three disabled until the final call, then cook all four
        // together. Collision is ready synchronously before construction ends.
        if (Section == 3)
            for (int32 Previous = 0; Previous < Section; ++Previous)
                IslandMesh->GetProcMeshSection(Previous)->bEnableCollision = true;
        IslandMesh->CreateMeshSection_LinearColor(Section, Vertices, Triangles[Section], Normals, UVs,
            Colors, Tangents, Section == 3);
        if (Base)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
            Material->SetVectorParameterValue(TEXT("Color"), Palette[Section]);
            IslandMesh->SetMaterial(Section, Material);
        }
    }
    IslandMesh->ContainsPhysicsTriMeshData(true);
}

void AProceduralIsland::BuildCanyonAssets()
{
    if (!CanyonLayout.Validate()) return;
    const TCHAR* Names[] = { TEXT("SM_SupplyCrate"), TEXT("SM_WaterBarrel"), TEXT("SM_MineCart"),
        TEXT("SM_Cactus"), TEXT("SM_OreCluster"), TEXT("SM_RockCluster"), TEXT("SM_ThreeStoneStack"),
        TEXT("SM_Campfire"), TEXT("SM_FallenLog"), TEXT("SM_SkullIdol") };
    TArray<UHierarchicalInstancedStaticMeshComponent*> Props;
    for (const TCHAR* Name : Names)
    {
        const FString Path = FString::Printf(TEXT("/Game/IslandAssets/Canyon/Props/%s/%s.%s"), Name, Name, Name);
        auto* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
        if (!Mesh) continue;
        auto* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        Component->SetupAttachment(RootComponent);
        Component->SetStaticMesh(Mesh);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        Component->ComponentTags.Add(TEXT("CanyonAsset"));
        Component->ComponentTags.Add(TEXT("CanyonProp"));
        Component->RegisterComponent();
        CanyonAssetInstances.Add(Component);
        Props.Add(Component);
    }
    if (Props.IsEmpty()) return;
    FRandomStream Stream(Seed ^ 0x4c178);
    TArray<FVector> Placed, PlacedInCaves;
    int32 NextProp = Stream.RandRange(0, Props.Num() - 1);
    for (const auto& Edge : CanyonLayout.Edges)
    {
        if (!Edge.bCave && Edge.Layer != ECanyonRouteLayer::Lower) continue;
        const FVector A = CanyonLayout.Nodes[Edge.A].Position, B = CanyonLayout.Nodes[Edge.B].Position;
        if (!Edge.bCave && (CanyonLayout.Nodes[Edge.A].bCaveInterior || CanyonLayout.Nodes[Edge.B].bCaveInterior)) continue;
        const float Length = FVector::Dist2D(A, B);
        const FVector Forward = (B - A).GetSafeNormal2D();
        const FVector Right(-Forward.Y, Forward.X, 0.f);
        // Keep authored sizes where possible; shrink larger props to fit narrow routes.
        for (float Along = Edge.bCave ? 80.f : 160.f; Along < Length - (Edge.bCave ? 80.f : 160.f); Along += Edge.bCave ? 220.f : 280.f)
            for (const float Side : { -1.f, 1.f })
            {
                auto* Prop = Props[NextProp % Props.Num()];
                const FBox Bounds = Prop->GetStaticMesh()->GetBoundingBox();
                const float NativeRadius = FMath::Max(Bounds.GetSize().X, Bounds.GetSize().Y) * 0.5f;
                float Scale = FMath::Min(Stream.FRandRange(0.85f, 1.1f), Edge.HalfWidth * 0.4f / FMath::Max(1.f, NativeRadius));
                if (Edge.bCave) Scale = FMath::Min(Scale, 180.f / FMath::Max(1.f, static_cast<float>(Bounds.GetSize().Z)));
                const float Radius = NativeRadius * Scale;
                const FVector Point = FMath::Lerp(A, B, Along / Length) + Right * (Side * (Edge.HalfWidth * (Edge.bCave ? 0.5f : 0.76f) - Radius));
                if (!Edge.bCave && SlopeAt(Point.X, Point.Y) > 0.3f) continue;
                bool bClear = true;
                for (const int32 Mouth : CanyonLayout.AllCaveMouthNodes)
                    if (FVector::Dist2D(Point, CanyonLayout.Nodes[Mouth].Position) < 220.f + Radius) bClear = false;
                for (const FVector& Previous : Edge.bCave ? PlacedInCaves : Placed)
                    if (FVector::Dist2D(Point, Previous) < (Edge.bCave ? 120.f : 220.f) + Radius) bClear = false;
                if (!bClear) continue;
                FVector CaveFloor;
                if (Edge.bCave && !FindCanyonCaveFloor(Point, CaveFloor)) continue;
                const float Z = Edge.bCave ? GetActorTransform().InverseTransformPosition(CaveFloor).Z
                    : CanyonLayout.SurfaceHeightAt(Point.X, Point.Y);
                // Bound the height variation under the footprint to avoid floating on banks.
                for (const FVector& Offset : { Right * Radius, -Right * Radius, Forward * Radius, -Forward * Radius })
                {
                    if (Edge.bCave)
                    {
                        FVector Foot;
                        if (!FindCanyonCaveFloor(Point + Offset, Foot)
                            || FMath::Abs(GetActorTransform().InverseTransformPosition(Foot).Z - Z) > 35.f) bClear = false;
                    }
                    else if (FMath::Abs(CanyonLayout.SurfaceHeightAt(Point.X + Offset.X, Point.Y + Offset.Y) - Z) > 35.f) bClear = false;
                }
                if (!bClear) continue;
                const FRotator Rotation(0.f, Stream.FRandRange(0.f, 360.f), 0.f);
                const FVector CenterOffset = Rotation.RotateVector(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, 0.f) * Scale);
                Prop->AddInstance(FTransform(Rotation, FVector(Point.X, Point.Y, Z - Bounds.Min.Z * Scale) - CenterOffset, FVector(Scale)));
                (Edge.bCave ? PlacedInCaves : Placed).Add(Point);
                ++NextProp;
            }
    }
}

void AProceduralIsland::BuildCanyonGrayboxLandmarks()
{
    if (!CanyonLayout.Validate()) return;
    const float S = CanyonLayout.LengthScale;
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Base)
    {
        UMaterialInstanceDynamic* BoxMaterial = UMaterialInstanceDynamic::Create(Base, this);
        BoxMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.83f, 0.72f, 0.44f));
        CanyonLandmarkBoxes->SetMaterial(0, BoxMaterial);
        UMaterialInstanceDynamic* CylinderMaterial = UMaterialInstanceDynamic::Create(Base, this);
        CylinderMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.92f, 0.80f, 0.53f));
        CanyonLandmarkCylinders->SetMaterial(0, CylinderMaterial);
    }
    auto Box = [&](const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        CanyonLandmarkBoxes->AddInstance(FTransform(Rotation, Center, Size * S / 100.f));
    };
    auto Cylinder = [&](const FVector& Center, float Diameter, float Height)
    {
        CanyonLandmarkCylinders->AddInstance(FTransform(FRotator::ZeroRotator, Center,
            FVector(Diameter * S / 100.f, Diameter * S / 100.f, Height * S / 100.f)));
    };
    for (int32 Index = 0; Index < CanyonLayout.Nodes.Num(); ++Index)
    {
        const FCanyonGrayboxNode& Node = CanyonLayout.Nodes[Index];
        if (Node.Landmark == ECanyonGrayboxLandmark::None) continue;
        float CaveT, CaveSide, CaveFloor;
        if (CanyonLayout.ProjectCave(Node.Position.X, Node.Position.Y,
            CaveT, CaveSide, CaveFloor) && FMath::Abs(CaveSide) < 1800.f * S
            && (CaveT > 0.001f && CaveT < 0.999f
                || CanyonLayout.AllCaveMouthNodes.Contains(Index))) continue;
        FVector Approach = FVector::ForwardVector;
        for (const FCanyonGrayboxEdge& Edge : CanyonLayout.Edges)
        {
            const int32 Other = Edge.A == Index ? Edge.B : Edge.B == Index ? Edge.A : INDEX_NONE;
            if (Other != INDEX_NONE)
            {
                Approach = (Node.Position - CanyonLayout.Nodes[Other].Position).GetSafeNormal2D();
                break;
            }
        }
        const FVector Right(-Approach.Y, Approach.X, 0.f);
        const FVector Ground(Node.Position.X, Node.Position.Y,
            CanyonLayout.SurfaceHeightAt(Node.Position.X, Node.Position.Y));
        const FRotator Across = Right.Rotation();
        switch (Node.Landmark)
        {
        case ECanyonGrayboxLandmark::Arch:
            for (int32 Side : { -1, 1 })
            {
                const FVector Foot = Ground + Right * (Side * 800.f * S);
                const float BaseZ = CanyonLayout.SurfaceHeightAt(Foot.X, Foot.Y);
                Box(FVector(Foot.X, Foot.Y, BaseZ + 900.f * S), FVector(380.f, 420.f, 1800.f));
            }
            Box(Ground + FVector(0.f, 0.f, 1950.f * S), FVector(2050.f, 500.f, 380.f), Across);
            break;
        case ECanyonGrayboxLandmark::TwinPillars:
            for (int32 Side : { -1, 1 })
            {
                const FVector Foot = Ground + Right * (Side * 1050.f * S);
                const float Height = Side < 0 ? 2200.f : 2900.f;
                Box(FVector(Foot.X, Foot.Y, CanyonLayout.SurfaceHeightAt(Foot.X, Foot.Y) + Height * S * 0.5f),
                    FVector(440.f, 440.f, Height), FRotator(0.f, 0.f, Side * 7.f));
            }
            break;
        case ECanyonGrayboxLandmark::SplitPeak:
        {
            const FVector Foot = Ground + Right * (1300.f * S);
            const float BaseZ = CanyonLayout.SurfaceHeightAt(Foot.X, Foot.Y);
            Box(FVector(Foot.X, Foot.Y, BaseZ + 360.f * S), FVector(900.f, 850.f, 720.f));
            Box(FVector(Foot.X, Foot.Y, BaseZ + 1400.f * S) + Right * (380.f * S),
                FVector(420.f, 430.f, 2100.f), FRotator(0.f, 0.f, 22.f));
            Box(FVector(Foot.X, Foot.Y, BaseZ + 1250.f * S) - Right * (390.f * S),
                FVector(420.f, 430.f, 1800.f), FRotator(0.f, 0.f, -25.f));
            break;
        }
        case ECanyonGrayboxLandmark::BrokenBridge:
        {
            for (int32 Side : { -1, 1 })
            {
                const FVector Foot = Ground + Right * (Side * 1250.f * S);
                Box(FVector(Foot.X, Foot.Y, CanyonLayout.SurfaceHeightAt(Foot.X, Foot.Y) + 900.f * S),
                    FVector(450.f, 480.f, 1800.f));
                Box(Ground + Right * (Side * 760.f * S) + FVector(0.f, 0.f, 1800.f * S),
                    FVector(950.f, 500.f, 330.f), Across);
            }
            break;
        }
        case ECanyonGrayboxLandmark::Needle:
        {
            const FVector Foot = Ground + Right * (1350.f * S);
            const float BaseZ = CanyonLayout.SurfaceHeightAt(Foot.X, Foot.Y);
            Cylinder(FVector(Foot.X, Foot.Y, BaseZ + 1450.f * S), 480.f, 2900.f);
            break;
        }
        case ECanyonGrayboxLandmark::StoneRing:
        {
            const FVector Center = Ground + Right * (1450.f * S);
            for (int32 I = 0; I < 6; ++I)
            {
                const float Angle = I * PI / 3.f;
                const FVector Foot = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (500.f * S);
                Cylinder(FVector(Foot.X, Foot.Y, CanyonLayout.SurfaceHeightAt(Foot.X, Foot.Y) + 360.f * S), 240.f, 720.f);
            }
            break;
        }
        case ECanyonGrayboxLandmark::CaveBeacon:
        {
            for (int32 Side : { -1, 1 })
            {
                const FVector Foot = Ground + Right * (Side * 1100.f * S);
                Cylinder(FVector(Foot.X, Foot.Y,
                    CanyonLayout.SurfaceHeightAt(Foot.X, Foot.Y) + 700.f * S), 260.f, 1400.f);
            }
            Box(Ground - Approach * (650.f * S) + FVector(0.f, 0.f, 720.f * S),
                FVector(350.f, 350.f, 1440.f), FRotator(0.f, 0.f, 13.f));
            break;
        }
        default: break;
        }
    }
}

void AProceduralIsland::BuildCanyonCaves()
{
    if (CanyonLayout.CavePathNodes.Num() < 4) return;

    const float Half = (GridSize - 1) * CellSize * 0.5f;
    FCanyonSolidBounds Bounds;
    Bounds.Min = FVector2D(CanyonCavePatchCells.Min.X * CellSize - Half,
        CanyonCavePatchCells.Min.Y * CellSize - Half);
    Bounds.Max = FVector2D(CanyonCavePatchCells.Max.X * CellSize - Half,
        CanyonCavePatchCells.Max.Y * CellSize - Half);
    float LowestFloor = 600.f;
    for (const FCanyonGrayboxEdge& Edge : CanyonLayout.Edges)
        if (Edge.bCave)
            for (const int32 Index : { Edge.A, Edge.B })
                LowestFloor = FMath::Min(LowestFloor,
                    600.f + CanyonLayout.Nodes[Index].Position.Z);
    Bounds.BottomZ = LowestFloor - 250.f;
    // Narrow hall junctions need the original sampling to preserve their
    // floor connections. Ordinary tunnels use a much cheaper coarse shell.
    const bool bNarrowHall = CanyonLayout.CaveNetworks.ContainsByPredicate(
        [](const FCanyonCaveNetwork& Network) { return Network.Pattern == ECanyonCavePattern::ThreeMouthHall; });
    Bounds.StepXY = bNarrowHall ? CellSize * 0.25f : CellSize;
    Bounds.StepZ = bNarrowHall ? FMath::Max(45.f, 60.f * CanyonLayout.LengthScale) : 90.f;
    struct FCarveSegment { FVector A, B; float Width, Clearance; int32 Group; };
    TArray<FCarveSegment> CarveSegments;
    int32 CarveGroups = 0;
    for (int32 NetworkIndex = 0; NetworkIndex < CanyonLayout.CaveNetworks.Num(); ++NetworkIndex)
    {
        const FCanyonCaveNetwork& Network = CanyonLayout.CaveNetworks[NetworkIndex];
        TArray<TArray<int32>> Paths = Network.Branches;
        if (Paths.IsEmpty()) Paths.Add(Network.PathNodes);
        for (const FCanyonDeadEnd& DeadEnd : Network.DeadEnds) Paths.Append(DeadEnd.Paths);
        for (const TArray<int32>& Path : Paths)
        {
            float Total = 0.f, Along = 0.f;
            for (int32 I = 1; I < Path.Num(); ++I)
                Total += FVector::Dist2D(CanyonLayout.Nodes[Path[I - 1]].Position,
                    CanyonLayout.Nodes[Path[I]].Position);
            for (int32 I = 1; I < Path.Num(); ++I)
            {
                const FVector A = CanyonLayout.Nodes[Path[I - 1]].Position;
                const FVector B = CanyonLayout.Nodes[Path[I]].Position;
                const float Length = FVector::Dist2D(A, B);
                const float T = (Along + Length * 0.5f) / Total;
                CarveSegments.Add({ A, B,
                    Network.Pattern == ECanyonCavePattern::ThreeMouthHall ? 275.f : CanyonLayout.CaveHalfWidth(T, NetworkIndex),
                    Network.Pattern == ECanyonCavePattern::ThreeMouthHall ? 430.f : CanyonLayout.CaveClearance(T, NetworkIndex), CarveGroups });
                Along += Length;
            }
            ++CarveGroups;
        }
        for (const FCanyonGrayboxEdge& Edge : CanyonLayout.Edges)
            if (!Edge.bCave && (Network.MouthNodes.Contains(Edge.A) || Network.MouthNodes.Contains(Edge.B)))
                CarveSegments.Add({ CanyonLayout.Nodes[Edge.A].Position, CanyonLayout.Nodes[Edge.B].Position,
                    CanyonLayout.CaveHalfWidth(0.f, NetworkIndex), CanyonLayout.CaveClearance(0.f, NetworkIndex), CarveGroups++ });
    }
    const bool bGroupedTunnels = CanyonLayout.CavePattern == ECanyonCavePattern::ThreeMouthHall
        || CanyonLayout.CavePattern == ECanyonCavePattern::LongWindingThrough
        || CanyonLayout.CavePattern == ECanyonCavePattern::LongLoop
        || CanyonLayout.CavePattern == ECanyonCavePattern::BranchedThrough
        || CanyonLayout.CaveNetworks.Num() > 1;
    BuildCanyonSolidMesh(CanyonCaveMesh, Bounds, [&](float X, float Y)
    {
        FCanyonSolidColumn Column;
        Column.SurfaceZ = CanyonLayout.SurfaceHeightAt(X, Y);
        if (!bGroupedTunnels)
        {
            float T = 0.f, Floor = 0.f;
            int32 Network = INDEX_NONE;
            CanyonLayout.ProjectCave(X, Y, T, Column.Lateral, Floor, &Network);
            Column.FloorZ = 600.f + Floor;
            Column.HalfWidth = CanyonLayout.CaveHalfWidth(T, Network);
            Column.Clearance = CanyonLayout.CaveClearance(T, Network);
        }
        else
        {
            // Keep every branch in the same solid field. Choosing just the
            // nearest XY centerline closes a tunnel where passages cross at
            // different elevations.
            Column.HalfWidth = 0.f;
            const FVector2D Point(X, Y);
            struct FNearestTunnel { float Distance = TNumericLimits<float>::Max(); FCanyonSolidTunnel Tunnel; };
            TArray<FNearestTunnel, TInlineAllocator<32>> Nearest;
            Nearest.SetNum(CarveGroups);
            for (const FCarveSegment& Segment : CarveSegments)
            {
                const FVector& A = Segment.A;
                const FVector& B = Segment.B;
                const FVector2D Start(A.X, A.Y), Delta(B.X - A.X, B.Y - A.Y);
                const float Fraction = FMath::Clamp(FVector2D::DotProduct(Point - Start, Delta)
                    / Delta.SizeSquared(), 0.f, 1.f);
                const float Distance = FVector2D::Distance(Point, Start + Delta * Fraction);
                if (Distance > Segment.Width + Bounds.StepXY) continue;
                if (Distance < Nearest[Segment.Group].Distance)
                {
                    Nearest[Segment.Group].Distance = Distance;
                    Nearest[Segment.Group].Tunnel = { static_cast<float>(600.f + FMath::Lerp(A.Z, B.Z, Fraction)),
                        Distance, Segment.Width, Segment.Clearance };
                }
            }
            for (const FNearestTunnel& Tunnel : Nearest)
                if (Tunnel.Distance < TNumericLimits<float>::Max()) Column.AdditionalTunnels.Add(Tunnel.Tunnel);
        }
        for (const FCanyonCaveHall& Hall : CanyonLayout.AllCaveHalls)
        {
            const FVector& Center = CanyonLayout.Nodes[Hall.Node].Position;
            const float Distance = FVector2D::Distance(FVector2D(X, Y),
                FVector2D(Center.X, Center.Y));
            if (Distance <= Hall.Radius + Bounds.StepXY)
                Column.AdditionalTunnels.Add({ static_cast<float>(600.f + Center.Z), Distance,
                    Hall.Radius, Hall.Clearance });
        }
        return Column;
    });
    CanyonCaveMesh->ContainsPhysicsTriMeshData(true);
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Rock = UMaterialInstanceDynamic::Create(Base, this);
        Rock->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.29f, 0.21f, 0.17f));
        CanyonCaveMesh->SetMaterial(0, Rock);
    }

    // A fixed light budget avoids dozens of overlapping movable lights.
    // Cover interior nodes with at most eight broad, shadow-free lights.
    for (const auto& Node : CanyonLayout.Nodes)
    {
        if (!Node.bCaveInterior || CanyonFillLights.Num() >= 8) continue;
        const FVector Position = Node.Position + FVector(0, 0, 860.f);
        bool bCovered = false;
        for (const UPointLightComponent* Existing : CanyonFillLights)
            if (FVector::Dist(Existing->GetRelativeLocation(), Position) < 900.f) bCovered = true;
        if (bCovered) continue;
        UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
        Lamp->SetupAttachment(RootComponent);
        Lamp->SetMobility(EComponentMobility::Movable);
        Lamp->SetIntensity(18000.f);
        Lamp->SetAttenuationRadius(1400.f);
        Lamp->SetLightColor(FLinearColor(0.86f, 0.69f, 0.49f));
        Lamp->SetCastShadows(false);
        Lamp->RegisterComponent();
        Lamp->SetRelativeLocation(Position);
        CanyonFillLights.Add(Lamp);
    }
}
