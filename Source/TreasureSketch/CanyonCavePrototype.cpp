#include "CanyonCavePrototype.h"
#include "CanyonSolidMesh.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PointLightComponent.h"

namespace
{
constexpr float MinX = -5000.f, MaxX = 5000.f;
constexpr float MinY = -3100.f, MaxY = 3100.f;
constexpr float BottomZ = -350.f;
constexpr float GridStep = 80.f;
constexpr float TunnelHalfWidth = 320.f;
constexpr float TunnelHeight = 490.f;

float Smooth01(float T)
{
    T = FMath::Clamp(T, 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
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
    FCanyonSolidBounds Bounds;
    Bounds.Min = FVector2D(MinX, MinY);
    Bounds.Max = FVector2D(MaxX, MaxY);
    Bounds.BottomZ = BottomZ;
    Bounds.StepXY = GridStep;
    Bounds.StepZ = GridStep;
    BuildCanyonSolidMesh(MountainMesh, Bounds, [](float X, float Y)
    {
        FCanyonSolidColumn Column;
        Column.SurfaceZ = ACanyonCavePrototype::SurfaceHeight(X, Y);
        Column.FloorZ = 0.f;
        Column.Lateral = Y;
        Column.HalfWidth = TunnelHalfWidth;
        Column.Clearance = TunnelHeight;
        return Column;
    });
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Rock = UMaterialInstanceDynamic::Create(Base, this);
        Rock->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.40f, 0.30f, 0.25f));
        MountainMesh->SetMaterial(0, Rock);
    }
}
