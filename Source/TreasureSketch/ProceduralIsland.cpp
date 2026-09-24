#include "ProceduralIsland.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

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
}

void AProceduralIsland::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BuildIsland();
    BuildWater();
}

float AProceduralIsland::HeightAt(float X, float Y) const
{
    const float Radius = CellSize * (GridSize - 1) * 0.46f;
    const float Angle = FMath::Atan2(Y, X);
    const float Wobble = 1.f + 0.12f * FMath::Sin(3.f * Angle + Seed * 0.01f)
        + 0.08f * FMath::Sin(5.f * Angle - Seed * 0.017f);
    const float EllipseDistance = FMath::Sqrt(FMath::Square(X / 1.08f) + FMath::Square(Y / 0.88f));
    const float Edge = EllipseDistance / (Radius * Wobble);
    if (Edge >= 1.f) return -220.f - (Edge - 1.f) * 400.f;

    const float Base = 80.f + 620.f * FMath::Pow(1.f - Edge, 1.35f);
    const float Hills = 170.f * FMath::Sin(X * 0.00115f + Seed) * FMath::Cos(Y * 0.0010f - Seed * 0.3f)
        + 120.f * FMath::Sin((X + Y) * 0.0018f);
    const float LandmarkRidge = 360.f * FMath::Exp(-FMath::Square((X - Radius * 0.22f) / 1250.f)
        - FMath::Square((Y + Radius * 0.18f) / 900.f));
    return Base + Hills * (1.f - Edge) + LandmarkRidge;
}

FVector AProceduralIsland::FindRandomLandPoint(FRandomStream& Stream, float MinimumHeight) const
{
    const float Extent = CellSize * GridSize * 0.32f;
    for (int32 Attempt = 0; Attempt < 200; ++Attempt)
    {
        const float X = Stream.FRandRange(-Extent, Extent);
        const float Y = Stream.FRandRange(-Extent, Extent);
        const float Z = HeightAt(X, Y);
        if (Z >= MinimumHeight) return GetActorLocation() + FVector(X, Y, Z);
    }
    return GetActorLocation() + FVector(0.f, 0.f, HeightAt(0.f, 0.f));
}

void AProceduralIsland::BuildIsland()
{
    IslandMesh->ClearAllMeshSections();
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
    TArray<FLinearColor> Colors;
    const float Half = (GridSize - 1) * CellSize * 0.5f;

    for (int32 Y = 0; Y < GridSize; ++Y)
    {
        for (int32 X = 0; X < GridSize; ++X)
        {
            const float WX = X * CellSize - Half;
            const float WY = Y * CellSize - Half;
            const float Z = HeightAt(WX, WY);
            Vertices.Add(FVector(WX, WY, Z));
            UVs.Add(FVector2D((float)X / (GridSize - 1), (float)Y / (GridSize - 1)));
            const float Shade = FMath::GetMappedRangeValueClamped(FVector2D(-100.f, 900.f), FVector2D(0.f, 1.f), Z);
            Colors.Add(FLinearColor(0.07f + Shade * 0.10f, 0.24f + Shade * 0.24f, 0.06f, 1.f));
            Normals.Add(FVector::UpVector);
            Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
        }
    }
    for (int32 Y = 0; Y < GridSize - 1; ++Y)
    {
        for (int32 X = 0; X < GridSize - 1; ++X)
        {
            const int32 I = Y * GridSize + X;
            Triangles.Append({I, I + GridSize, I + 1, I + 1, I + GridSize, I + GridSize + 1});
        }
    }
    IslandMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
    IslandMesh->ContainsPhysicsTriMeshData(true);
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
        Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.32f, 0.07f, 1.f));
        IslandMesh->SetMaterial(0, Material);
    }
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
