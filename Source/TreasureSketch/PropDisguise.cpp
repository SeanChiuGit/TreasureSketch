#include "PropDisguise.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

bool PropDisguise::FindTarget(UWorld* World, const FVector& Origin, const FVector& Direction, FPropDisguise& Out,
    FTarget* OutTarget)
{
    if (OutTarget) *OutTarget = FTarget();
    if (!World || Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero()) return false;
    const FVector Aim = Direction.GetSafeNormal();
    const FVector End = Origin + Aim * WORLD_MAX;
    float BestTime = 1.f;
    double BestAimScore = TNumericLimits<double>::Max();
    // A small angular margin stays consistent with distance and is shared by
    // the local preview and authoritative server selection.
    const double AimMargin = FMath::Tan(FMath::DegreesToRadians(0.6));
    FCollisionQueryParams VisibilityQuery(SCENE_QUERY_STAT(PropSelection), true);
    for (TActorIterator<APawn> Pawn(World); Pawn; ++Pawn) VisibilityQuery.AddIgnoredActor(*Pawn);
    UStaticMeshComponent* BestComponent = nullptr;
    FTransform BestTransform;
    int32 BestInstanceIndex = INDEX_NONE;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->IsHidden() || It->IsA<APawn>()) continue;
        TArray<UStaticMeshComponent*> Components;
        It->GetComponents(Components);
        for (UStaticMeshComponent* Component : Components)
        {
            if (!Component->IsVisible() || Component->bHiddenInGame || !Component->GetStaticMesh()
                || Component->ComponentHasTag(TEXT("PropSelectionHighlight"))) continue;
            auto* Instances = Cast<UInstancedStaticMeshComponent>(Component);
            const int32 Count = Instances ? Instances->GetInstanceCount() : 1;
            for (int32 Index = 0; Index < Count; ++Index)
            {
                FTransform Transform = Component->GetComponentTransform();
                if (Instances && !Instances->GetInstanceTransform(Index, Transform, true)) continue;
                if (Transform.GetScale3D().IsNearlyZero()) continue;
                FVector Hit, Normal;
                float Time;
                const FBox Bounds = Component->GetStaticMesh()->GetBoundingBox();
                const FVector CenterDelta = Transform.TransformPosition(Bounds.GetCenter()) - Origin;
                const double ForwardDistance = FVector::DotProduct(CenterDelta, Aim);
                if (ForwardDistance <= UE_SMALL_NUMBER) continue;
                const double AimScore = (CenterDelta - Aim * ForwardDistance).SizeSquared()
                    / FMath::Square(ForwardDistance);
                // Rank by proximity to the reticle, not the front face of a
                // large decorative bounding box. Distance breaks angular ties.
                if (AimScore > BestAimScore + 1.e-8) continue;
                const FVector Scale = Transform.GetScale3D().GetAbs();
                if (Scale.GetMin() <= UE_SMALL_NUMBER) continue;
                const FVector Margin = FVector(ForwardDistance * AimMargin) / Scale;
                if (!FMath::LineExtentBoxIntersection(Bounds,
                    Transform.InverseTransformPosition(Origin), Transform.InverseTransformPosition(End),
                    Margin, Hit, Normal, Time)) continue;
                if (FMath::Abs(AimScore - BestAimScore) <= 1.e-8 && Time >= BestTime) continue;
                // Do not allow a centered prop to win through a colliding wall.
                // Decorative meshes with collision disabled remain selectable.
                FHitResult Obstruction;
                const FVector TargetPoint = Origin + Aim * (Time * WORLD_MAX);
                if (World->LineTraceSingleByChannel(Obstruction, Origin, TargetPoint,
                    ECC_Visibility, VisibilityQuery) && Obstruction.GetComponent() != Component
                    && Obstruction.Distance + 2.f < FVector::Distance(Origin, TargetPoint)) continue;
                BestAimScore = AimScore;
                BestTime = Time;
                BestComponent = Component;
                BestTransform = Transform;
                BestInstanceIndex = Instances ? Index : INDEX_NONE;
            }
        }
    }
    if (!BestComponent) return false;
    if (OutTarget)
    {
        OutTarget->Component = BestComponent;
        OutTarget->InstanceIndex = BestInstanceIndex;
        OutTarget->Transform = BestTransform;
    }
    Out = FPropDisguise();
    Out.Mesh = BestComponent->GetStaticMesh();
    Out.Scale = BestTransform.GetScale3D();
    Out.Rotation = BestTransform.Rotator();
    for (int32 Index = 0; Index < BestComponent->GetNumMaterials(); ++Index)
    {
        UMaterialInterface* Material = BestComponent->GetMaterial(Index);
        FLinearColor Color = FLinearColor::White;
        if (Material) Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), Color);
        // Dynamic instances have no shared network asset path. Replicate their
        // authored parent and the map's Color parameter instead.
        while (auto* Dynamic = Cast<UMaterialInstanceDynamic>(Material)) Material = Dynamic->Parent;
        Out.Materials.Add(Material);
        Out.Colors.Add(Color);
    }
    return true;
}
