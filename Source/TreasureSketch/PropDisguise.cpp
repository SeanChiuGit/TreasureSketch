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
    const FVector End = Origin + Direction.GetSafeNormal() * WORLD_MAX;
    float BestTime = 1.f;
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
                // Bounds picking includes grass, bushes and other decorative
                // instances with collision disabled. There is no skill range,
                // prop whitelist, cooldown, duration, or size clamp.
                if (!FMath::LineExtentBoxIntersection(Component->GetStaticMesh()->GetBoundingBox(),
                    Transform.InverseTransformPosition(Origin), Transform.InverseTransformPosition(End),
                    FVector::ZeroVector, Hit, Normal, Time) || Time >= BestTime) continue;
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
