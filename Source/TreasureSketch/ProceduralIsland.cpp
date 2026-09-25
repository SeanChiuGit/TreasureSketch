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
enum class EIslandShape : uint8 { Round, Long, Crescent, TwinCove, TriCape };

EIslandShape ShapeFromSeed(int32 Seed)
{
    return static_cast<EIslandShape>(FMath::Abs(Seed) % 5);
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
    PalmInstances->SetStaticMesh(PalmMesh.Object);
    PalmCollisionInstances->SetStaticMesh(CylinderMesh.Object);
    RockInstances->SetStaticMesh(RockMesh.Object);
    BushInstances->SetStaticMesh(BushMesh.Object);
    DriftwoodInstances->SetStaticMesh(DriftwoodMesh.Object);
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
    case EIslandShape::Round: return TEXT("Round");
    case EIslandShape::Long: return TEXT("Long");
    case EIslandShape::Crescent: return TEXT("Crescent");
    case EIslandShape::TwinCove: return TEXT("TwinCove");
    default: return TEXT("TriCape");
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

    if (Shape == EIslandShape::Long)
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

    float Wobble = 1.f + 0.09f * FMath::Sin(5.f * Angle + Seed * 0.017f)
        + 0.055f * FMath::Sin(3.f * Angle - Seed * 0.011f);
    if (Shape == EIslandShape::TriCape)
        Wobble += 0.17f * FMath::Cos(3.f * Angle + Seed * 0.021f);

    const float AxisX = Shape == EIslandShape::Round ? 1.05f : 1.12f;
    const float AxisY = Shape == EIslandShape::Round ? 0.94f : 0.88f;
    float Distance = FMath::Sqrt(FMath::Square(RX / AxisX) + FMath::Square(RY / AxisY)) / (Radius * Wobble);
    if (Shape == EIslandShape::Crescent)
    {
        const float BayX = (RX - Radius * 0.50f) / (Radius * 0.47f);
        const float BayY = RY / (Radius * 0.52f);
        Distance += FMath::Exp(-(BayX * BayX + BayY * BayY) * 1.6f) * 0.73f;
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
    const FVector2D HillA(Profile.FRandRange(-0.34f, 0.34f) * Radius, Profile.FRandRange(-0.32f, 0.32f) * Radius);
    const FVector2D HillB(Profile.FRandRange(-0.42f, 0.42f) * Radius, Profile.FRandRange(-0.38f, 0.38f) * Radius);
    const float HillASize = Profile.FRandRange(1050.f, 1700.f);
    const float HillBSize = Profile.FRandRange(900.f, 1450.f);
    const float HillAHeight = Profile.FRandRange(290.f, 520.f);
    const float HillBHeight = Profile.FRandRange(180.f, 390.f);
    const float DXA = (X - HillA.X) / HillASize;
    const float DYA = (Y - HillA.Y) / HillASize;
    const float DXB = (X - HillB.X) / HillBSize;
    const float DYB = (Y - HillB.Y) / HillBSize;
    const float ShoreRise = 45.f + 300.f * FMath::Pow(FMath::Max(0.f, 1.f - Edge), 1.25f);
    const float Hills = HillAHeight * FMath::Exp(-(DXA * DXA + DYA * DYA))
        + HillBHeight * FMath::Exp(-(DXB * DXB + DYB * DYB));
    const float Rolling = SeedNoise(X, Y, Seed) * 105.f * FMath::Clamp((1.f - Edge) * 2.2f, 0.f, 1.f);
    return ShoreRise + Hills + Rolling;
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
        const bool bAwayFromSpawn = FVector2D::DistSquared(FVector2D(X, Y), FVector2D(-2800.f, 0.f)) > FMath::Square(1700.f);
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
            if (X < -2250.f && FMath::Abs(Y) < 900.f)
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

    TryPlace(RockInstances, 15, 135.f, 760.f, 0.58f, 430.f, FVector2D(0.78f, 1.45f), false);
    TryPlace(PalmInstances, 28, 125.f, 530.f, 0.32f, 500.f, FVector2D(0.82f, 1.22f), false);
    TryPlace(BushInstances, 38, 120.f, 570.f, 0.40f, 260.f, FVector2D(0.72f, 1.28f), false);
    TryPlace(DriftwoodInstances, 10, 45.f, 145.f, 0.30f, 520.f, FVector2D(0.82f, 1.25f), true);

    UE_LOG(LogTemp, Display, TEXT("TREASURE_ISLAND_BUILT Seed=%d Shape=%s Palms=%d Rocks=%d Bushes=%d Driftwood=%d"),
        Seed, *GetShapeName(), PalmInstances->GetInstanceCount(), RockInstances->GetInstanceCount(), BushInstances->GetInstanceCount(), DriftwoodInstances->GetInstanceCount());
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
            if (SlotName.Contains(TEXT("LeafDark"))) SRGBColor = FColor(28, 82, 35);
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
