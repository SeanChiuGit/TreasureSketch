#include "TreasureSurfacePaint.h"

#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

ATreasureSurfacePaint::ATreasureSurfacePaint()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    NetUpdateFrequency = 10.f;
    PaintMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SurfacePaint"));
    SetRootComponent(PaintMesh);
    PaintMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PaintMesh->SetCastShadow(false);
}

void ATreasureSurfacePaint::BeginPlay()
{
    Super::BeginPlay();
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
        Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.35f, 1.f));
        PaintMesh->SetMaterial(0, Material);
    }
    RebuildPaint();
}

void ATreasureSurfacePaint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATreasureSurfacePaint, Stamps);
}

bool ATreasureSurfacePaint::AddStamp(const FHitResult& Hit)
{
    if (!HasAuthority() || Stamps.Num() >= MaxStamps) return false;
    if (!Stamps.IsEmpty() && FVector::DistSquared(Stamps.Last().Position, Hit.ImpactPoint) < FMath::Square(10.f)
        && FVector::DotProduct(Stamps.Last().Normal, Hit.ImpactNormal) > 0.95f) return false;
    FSurfacePaintStamp& Stamp = Stamps.AddDefaulted_GetRef();
    Stamp.Position = Hit.ImpactPoint;
    Stamp.Normal = Hit.ImpactNormal.GetSafeNormal();
    RebuildPaint();
    ForceNetUpdate();
    return true;
}

void ATreasureSurfacePaint::RebuildPaint()
{
    if (!PaintMesh || !GetWorld()) return;
    if (BuiltStampCount == Stamps.Num()) return;
    constexpr int32 Sides = 12;
    constexpr float Radius = 22.f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SurfacePaintProjection), true, this);
    FCollisionObjectQueryParams PaintObjects;
    PaintObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    PaintObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
    // Only project new stamps. Older paint stays fixed and does not require
    // thousands of additional scene traces on every network update.
    for (int32 StampIndex = BuiltStampCount; StampIndex < Stamps.Num(); ++StampIndex)
    {
        const FSurfacePaintStamp& Stamp = Stamps[StampIndex];
        FVector AxisX, AxisY;
        Stamp.Normal.FindBestAxisVectors(AxisX, AxisY);
        const int32 Base = Vertices.Num();
        Vertices.Add(Stamp.Position + Stamp.Normal * 1.f);
        Normals.Add(Stamp.Normal); UVs.Add(FVector2D(0.5f, 0.5f));
        Colors.Add(FLinearColor::White); Tangents.Add(FProcMeshTangent(AxisX, false));
        TArray<bool> Valid;
        for (int32 I = 0; I < Sides; ++I)
        {
            const float Angle = 2.f * PI * I / Sides;
            const FVector Offset = AxisX * FMath::Cos(Angle) + AxisY * FMath::Sin(Angle);
            const FVector Sample = Stamp.Position + Offset * Radius;
            FHitResult Hit;
            const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Sample + Stamp.Normal * 12.f,
                Sample - Stamp.Normal * 12.f, PaintObjects, Query);
            const bool bValid = bHit && FVector::DotProduct(Hit.ImpactNormal, Stamp.Normal) > 0.65f;
            Valid.Add(bValid);
            Vertices.Add(bValid ? Hit.ImpactPoint + Hit.ImpactNormal * 1.f : Sample + Stamp.Normal);
            Normals.Add(Stamp.Normal); UVs.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 0.5f + FVector2D(0.5f));
            Colors.Add(FLinearColor::White); Tangents.Add(FProcMeshTangent(AxisX, false));
        }
        for (int32 I = 0; I < Sides; ++I)
            if (Valid[I] && Valid[(I + 1) % Sides])
            { Triangles.Add(Base); Triangles.Add(Base + I + 1); Triangles.Add(Base + (I + 1) % Sides + 1); }
    }
    BuiltStampCount = Stamps.Num();
    PaintMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}
