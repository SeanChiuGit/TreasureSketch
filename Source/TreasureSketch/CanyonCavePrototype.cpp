#include "CanyonCavePrototype.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PointLightComponent.h"

namespace
{
constexpr float MinX = -5000.f, MaxX = 5000.f;
constexpr float MinY = -3100.f, MaxY = 3100.f;
constexpr float BottomZ = -350.f, TopZ = 2250.f;
constexpr float GridStep = 80.f;
constexpr float TunnelHalfWidth = 320.f;
constexpr float TunnelHeight = 490.f;

float Smooth01(float T)
{
    T = FMath::Clamp(T, 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
}

float SolidField(const FVector& P)
{
    const double Q = FMath::Clamp(P.Z / TunnelHeight, 0.0, 1.0);
    const double ArchWidth = TunnelHalfWidth * FMath::Sqrt(FMath::Max(0.0, 1.0 - Q * Q));
    const double TunnelVoid = FMath::Min(FMath::Min(ArchWidth - FMath::Abs(P.Y), P.Z),
        TunnelHeight - P.Z);
    return static_cast<float>(FMath::Min(FMath::Min(
        static_cast<double>(ACanyonCavePrototype::SurfaceHeight(P.X, P.Y)) - P.Z,
        P.Z - (BottomZ - 100.f)), -TunnelVoid));
}
}

ACanyonCavePrototype::ACanyonCavePrototype()
{
    MountainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MountainAndTunnel"));
    SetRootComponent(MountainMesh);
    MountainMesh->bUseComplexAsSimpleCollision = true;
    for (int32 I = 0; I < 3; ++I)
    {
        UPointLightComponent* Fill = CreateDefaultSubobject<UPointLightComponent>(
            *FString::Printf(TEXT("TunnelFill%d"), I));
        Fill->SetupAttachment(RootComponent);
        Fill->SetRelativeLocation(FVector(-1700.f + I * 1700.f, 0.f, 320.f));
        Fill->SetIntensity(2200.f);
        Fill->SetAttenuationRadius(1500.f);
        Fill->SetCastShadows(false);
    }
}

float ACanyonCavePrototype::BypassY(float X)
{
    return -1900.f;
}

float ACanyonCavePrototype::SurfaceHeight(float X, float Y)
{
    const float RadiusSquared = FMath::Square(X / 3300.f) + FMath::Square(Y / 2500.f);
    const float Mass = 1600.f * FMath::Pow(FMath::Max(0.f, 1.f - RadiusSquared), 1.4f);
    const float Bypass = Smooth01(FMath::Abs(Y - BypassY(X)) / 550.f);
    return Mass * Bypass;
}

void ACanyonCavePrototype::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    MountainMesh->ClearAllMeshSections();

    constexpr int32 NX = static_cast<int32>((MaxX - MinX) / GridStep);
    constexpr int32 NY = static_cast<int32>((MaxY - MinY) / GridStep);
    constexpr int32 NZ = static_cast<int32>((TopZ - BottomZ) / GridStep);
    constexpr int32 StrideY = NX + 1;
    constexpr int32 StrideZ = (NX + 1) * (NY + 1);
    auto GridIndex = [](int32 X, int32 Y, int32 Z)
    { return Z * StrideZ + Y * StrideY + X; };
    auto Point = [](int32 X, int32 Y, int32 Z)
    { return FVector(MinX + X * GridStep, MinY + Y * GridStep, BottomZ + Z * GridStep); };

    TArray<float> Samples;
    Samples.SetNumUninitialized((NX + 1) * (NY + 1) * (NZ + 1));
    for (int32 Z = 0; Z <= NZ; ++Z)
        for (int32 Y = 0; Y <= NY; ++Y)
            for (int32 X = 0; X <= NX; ++X)
                Samples[GridIndex(X, Y, Z)] = SolidField(Point(X, Y, Z));

    TArray<FVector> Vertices, Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    const int32 Corners[8][3] = { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} };
    const int32 Tets[6][4] = { {0,5,1,6}, {0,1,2,6}, {0,2,3,6},
        {0,3,7,6}, {0,7,4,6}, {0,4,5,6} };

    auto AddTriangle = [&](FVector A, FVector B, FVector C, const FVector& RockWitness)
    {
        FVector Geometric = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (Geometric.IsNearlyZero()) return;
        const FVector Center = (A + B + C) / 3.f;
        // Orient from the tetrahedron's known solid-side vertex. Sampling the
        // non-smooth field at a portal intersection can flip individual faces.
        if (FVector::DotProduct(Geometric, RockWitness - Center) < 0.f)
        {
            Swap(B, C);
            Geometric *= -1.f;
        }
        const FVector Outward = -Geometric;
        const int32 Base = Vertices.Num();
        for (const FVector& Vertex : { A, B, C })
        {
            Vertices.Add(Vertex);
            Normals.Add(Outward);
            UVs.Add(FVector2D(Vertex.X / 500.f, Vertex.Y / 500.f));
            Colors.Add(FLinearColor::White);
            Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
        }
        Triangles.Append({ Base, Base + 1, Base + 2 });
    };

    for (int32 Z = 0; Z < NZ; ++Z)
        for (int32 Y = 0; Y < NY; ++Y)
            for (int32 X = 0; X < NX; ++X)
            {
                FVector P[8]; float F[8];
                for (int32 C = 0; C < 8; ++C)
                {
                    const int32 CX = X + Corners[C][0];
                    const int32 CY = Y + Corners[C][1];
                    const int32 CZ = Z + Corners[C][2];
                    P[C] = Point(CX, CY, CZ);
                    F[C] = Samples[GridIndex(CX, CY, CZ)];
                }
                for (const auto& Tet : Tets)
                {
                    int32 Rock[4], Air[4], RockCount = 0, AirCount = 0;
                    for (int32 I = 0; I < 4; ++I)
                    {
                        const int32 Corner = Tet[I];
                        if (F[Corner] > 0.f) Rock[RockCount++] = Corner;
                        else Air[AirCount++] = Corner;
                    }
                    if (RockCount == 0 || AirCount == 0) continue;
                    auto Cut = [&](int32 A, int32 B)
                    { return FMath::Lerp(P[A], P[B], F[A] / (F[A] - F[B])); };
                    if (RockCount == 1)
                        AddTriangle(Cut(Rock[0], Air[0]), Cut(Rock[0], Air[1]),
                            Cut(Rock[0], Air[2]), P[Rock[0]]);
                    else if (AirCount == 1)
                        AddTriangle(Cut(Air[0], Rock[0]), Cut(Air[0], Rock[1]),
                            Cut(Air[0], Rock[2]), P[Rock[0]]);
                    else
                    {
                        const FVector A = Cut(Rock[0], Air[0]);
                        const FVector B = Cut(Rock[0], Air[1]);
                        const FVector C = Cut(Rock[1], Air[1]);
                        const FVector D = Cut(Rock[1], Air[0]);
                        AddTriangle(A, B, C, P[Rock[0]]);
                        AddTriangle(A, C, D, P[Rock[0]]);
                    }
                }
            }

    MountainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals,
        UVs, Colors, Tangents, true);
    MountainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MountainMesh->SetCollisionResponseToAllChannels(ECR_Block);
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Rock = UMaterialInstanceDynamic::Create(Base, this);
        Rock->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.40f, 0.30f, 0.25f));
        MountainMesh->SetMaterial(0, Rock);
    }
}
