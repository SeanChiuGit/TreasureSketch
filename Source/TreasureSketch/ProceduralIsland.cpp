#include "ProceduralIsland.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
enum class EIslandShape : uint8 { RoundBay, LongSpine, Crescent, TwinCove, TriCape, Hook, StarCove };
enum class ETerrainProfile : uint8 { Flat, SinglePeak, TwinPeaks, Ridge, EdgeCliff, Basin, Rolling };

EIslandShape ShapeFromSeed(int32 Seed)
{
    return static_cast<EIslandShape>(FMath::Abs(Seed) % 7);
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
}

void AProceduralIsland::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AProceduralIsland, Seed);
}

void AProceduralIsland::OnRep_Seed()
{
    BuildIsland();
    BuildWater();
    BuildDecorations();
}

void AProceduralIsland::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BuildIsland();
    BuildWater();
    BuildDecorations();
}

FString AProceduralIsland::GetShapeName() const
{
    switch (ShapeFromSeed(Seed))
    {
    case EIslandShape::RoundBay: return TEXT("RoundBay");
    case EIslandShape::LongSpine: return TEXT("LongSpine");
    case EIslandShape::Crescent: return TEXT("Crescent");
    case EIslandShape::TwinCove: return TEXT("TwinCove");
    case EIslandShape::TriCape: return TEXT("TriCape");
    case EIslandShape::Hook: return TEXT("Hook");
    default: return TEXT("StarCove");
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
    const float Edge = NormalizedIslandDistance(X, Y);
    if (Edge >= 1.f)
        return -180.f - (Edge - 1.f) * 520.f;

    const float Radius = CellSize * (GridSize - 1) * 0.46f;
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
        Relief = SeedNoise(X, Y, Seed) * 45.f;
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
    for (const FVector2D& Point : OccupiedPoints)
        if (FVector2D::DistSquared(Point, FVector2D(X, Y)) < Radius * Radius)
            return false;
    return true;
}

FVector AProceduralIsland::FindRandomLandPoint(FRandomStream& Stream, float MinimumHeight) const
{
    const float Extent = CellSize * (GridSize - 1) * 0.42f;
    for (int32 Attempt = 0; Attempt < 500; ++Attempt)
    {
        const float X = Stream.FRandRange(-Extent, Extent);
        const float Y = Stream.FRandRange(-Extent, Extent);
        const float Z = HeightAt(X, Y);
        const bool bAwayFromSpawn = FVector2D::DistSquared(FVector2D(X, Y), FVector2D(-3800.f, 0.f)) > FMath::Square(1900.f);
        if (Z >= MinimumHeight && Z <= 760.f && SlopeAt(X, Y) < 0.42f
            && NormalizedIslandDistance(X, Y) < 0.84f && bAwayFromSpawn && IsClearOfDecorations(X, Y, 520.f))
            return GetActorLocation() + FVector(X, Y, Z);
    }
    return GetActorLocation() + FVector(0.f, 0.f, HeightAt(0.f, 0.f));
}

void AProceduralIsland::BuildIsland()
{
    IslandMesh->ClearAllMeshSections();
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
            if (Z < 125.f) Section = 0;
            else if (Slope > 0.48f || Z > 650.f) Section = 3;
            else if (Moisture > 0.18f) Section = 2;
            SectionTriangles[Section].Append({ I, I + GridSize, I + 1, I + 1, I + GridSize, I + GridSize + 1 });
        }
    }

    const int32 Palette = FMath::Abs(Seed / 5) % 3;
    const FLinearColor SandColors[] = { FLinearColor(0.72f, 0.56f, 0.30f), FLinearColor(0.80f, 0.69f, 0.45f), FLinearColor(0.64f, 0.49f, 0.27f) };
    const FLinearColor GrassColors[] = { FLinearColor(0.16f, 0.40f, 0.08f), FLinearColor(0.24f, 0.46f, 0.12f), FLinearColor(0.31f, 0.39f, 0.09f) };
    const FLinearColor DarkGrassColors[] = { FLinearColor(0.07f, 0.25f, 0.06f), FLinearColor(0.10f, 0.31f, 0.12f), FLinearColor(0.18f, 0.27f, 0.06f) };
    const FLinearColor RockColors[] = { FLinearColor(0.27f, 0.25f, 0.21f), FLinearColor(0.34f, 0.32f, 0.28f), FLinearColor(0.29f, 0.25f, 0.20f) };
    const FLinearColor SurfaceColors[] = { SandColors[Palette], GrassColors[Palette], DarkGrassColors[Palette], RockColors[Palette] };

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
    OccupiedPoints.Reset();
    ApplyDecorationMaterials();

    FRandomStream Stream(Seed ^ 0x79B4A31);
    const float Extent = CellSize * (GridSize - 1) * 0.43f;
    auto TryPlace = [&](UHierarchicalInstancedStaticMeshComponent* Component, int32 TargetCount,
        float MinHeight, float MaxHeight, float MaxSlope, float MinSpacing, FVector2D ScaleRange, bool bBeachOnly)
    {
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
            OccupiedPoints.Add(FVector2D(X, Y));
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

void AProceduralIsland::BuildLandmarks(FRandomStream& Stream)
{
    UHierarchicalInstancedStaticMeshComponent* LandmarkComponents[] = {
        SkullIdolInstances, FaceIdolInstances, GiantAnchorInstances, ShipwreckInstances,
        BrokenMastInstances, StoneRingInstances, CampfireInstances
    };
    const float Extent = CellSize * (GridSize - 1) * 0.40f;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(LandmarkComponents); ++Index)
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
            OccupiedPoints.Add(FVector2D(X, Y));
            break;
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
            if (SlotName.Contains(TEXT("Flame"))) SRGBColor = FColor(255, 132, 16);
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
}

void AProceduralIsland::BuildWater()
{
    WaterMesh->ClearAllMeshSections();
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
