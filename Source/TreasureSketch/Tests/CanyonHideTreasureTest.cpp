#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/Material.h"
#include "../ProceduralIsland.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonHideTreasureTest, "TreasureSketch.Canyon.MergedAssetsAndScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonHideTreasureTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    for (const TCHAR* Name : { TEXT("SM_SupplyCrate"), TEXT("SM_WaterBarrel"), TEXT("SM_MineCart"),
        TEXT("SM_Cactus"), TEXT("SM_OreCluster"), TEXT("SM_RockCluster"), TEXT("SM_ThreeStoneStack"),
        TEXT("SM_Campfire"), TEXT("SM_FallenLog"), TEXT("SM_SkullIdol") })
    {
        const FString Path = FString::Printf(TEXT("/Game/IslandAssets/Canyon/Props/%s/%s.%s"), Name, Name, Name);
        TestNotNull(Name, LoadObject<UStaticMesh>(nullptr, *Path));
    }
    auto* Island = World->SpawnActor<AProceduralIsland>();
    Island->Theme = EIslandTheme::CanyonGraybox;
    Island->Seed = 1050;
    Island->MapScale = 1.f;
    Island->OnConstruction(FTransform::Identity);
    TestTrue(TEXT("New canyon 1x has one quarter of the old area"),
        FMath::IsNearlyEqual((Island->GridSize - 1) * Island->CellSize, 6270.f, 0.01f));
    for (const float Scale : { 0.5f, 1.f, 2.f })
    {
        Island->MapScale = Scale;
        Island->OnConstruction(FTransform::Identity);
        auto CountAssets = [&]()
        {
            TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
            Island->GetComponents(Components);
            int32 Props = 0, Pools = 0;
            for (auto* Component : Components)
                if (Component->ComponentHasTag(TEXT("CanyonAsset")))
                {
                    ++Pools;
                    TestEqual(TEXT("Asset skins retain the existing terrain collision"), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
                    TestTrue(TEXT("Canyon mesh preserves its imported material"),
                        Component->GetMaterial(0) == Component->GetStaticMesh()->GetMaterial(0));
                    for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
                        TestTrue(TEXT("Every imported material supports packaged instanced rendering"),
                            Component->GetMaterial(Slot) && Component->GetMaterial(Slot)->GetMaterial()->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
                    TestTrue(TEXT("Only the approved prop library is used"),
                        Component->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/IslandAssets/Canyon/Props/")));
                    Props += Component->GetInstanceCount();
                    for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
                    {
                        FTransform Transform;
                        Component->GetInstanceTransform(Index, Transform);
                        TestFalse(TEXT("Placed asset transforms are finite"), Transform.ContainsNaN());
                        TestTrue(TEXT("Placed asset scale stays positive"), Transform.GetScale3D().GetMin() > 0.f);
                    }
                }
            TestEqual(TEXT("Exactly ten prop pools are active"), Pools, 10);
            TestTrue(TEXT("Approved props appear in the generated map"), Props > 0);
            return Props;
        };
        const int32 FirstCount = CountAssets();
        int32 UndergroundProps = 0;
        TArray<UHierarchicalInstancedStaticMeshComponent*> PropComponents;
        Island->GetComponents(PropComponents);
        for (auto* Component : PropComponents)
            if (Component->ComponentHasTag(TEXT("CanyonAsset")))
                for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
                {
                    FTransform Transform;
                    Component->GetInstanceTransform(Index, Transform);
                    const FVector P = Transform.GetLocation();
                    if (P.Z < Island->GetCanyonLayout().SurfaceHeightAt(P.X, P.Y) - 200.f)
                        ++UndergroundProps;
                }
        TestTrue(TEXT("Approved props also populate covered cave floors"), UndergroundProps >= 1);
        for (const int32 Count : { 3, 6, 9 })
        {
            FRandomStream Stream(1050);
            const auto Treasures = Island->FindSeparatedTreasurePoints(Stream, Count, 950.f);
            TestEqual(TEXT("Surface and cave placement fills the party treasure quota"), Treasures.Num(), Count);
            int32 Underground = 0;
            for (const FVector& P : Treasures)
            {
                if (P.Z >= Island->GetCanyonLayout().SurfaceHeightAt(P.X, P.Y) - 200.f) continue;
                ++Underground;
                FHitResult Floor, Roof;
                TestTrue(TEXT("Cave treasure rests on actual collision ground"),
                    World->LineTraceSingleByChannel(Floor, P + FVector(0, 0, 10.f),
                        P - FVector(0, 0, 10.f), ECC_Visibility) && Floor.ImpactNormal.Z > 0.7f);
                TestTrue(TEXT("Cave treasure has a roof above the player"),
                    World->LineTraceSingleByChannel(Roof, P + FVector(0, 0, 200.f),
                        P + FVector(0, 0, 1800.f), ECC_Visibility));
            }
            TestTrue(TEXT("Every canyon round includes underground treasure"), Underground >= 1);
        }
        Island->OnConstruction(FTransform::Identity);
        TestEqual(TEXT("Rebuilding does not duplicate assets and preserves seeded placement"), CountAssets(), FirstCount);
    }
    Island->Theme = EIslandTheme::PirateBeach;
    Island->OnConstruction(FTransform::Identity);
    TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
    Island->GetComponents(Components);
    for (auto* Component : Components)
        TestFalse(TEXT("Switching theme removes every canyon asset pool"), Component->ComponentHasTag(TEXT("CanyonAsset")));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
