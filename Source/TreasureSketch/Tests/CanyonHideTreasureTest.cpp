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
    for (const TCHAR* Variant : { TEXT("Straight_A"), TEXT("Straight_B_Wide"),
        TEXT("Straight_C_Narrow"), TEXT("Rise_3m"), TEXT("Fall_3m") })
        for (const TCHAR* Part : { TEXT("Floor"), TEXT("Wall_L"), TEXT("Wall_R") })
        {
            const FString Name = FString::Printf(TEXT("SM_Canyon%s_%s"), Part, Variant);
            const FString Path = FString::Printf(TEXT("/Game/IslandAssets/CanyonModules/%s/StaticMeshes/%s.%s"),
                *Name, *Name, *Name);
            TestNotNull(*FString::Printf(TEXT("Merged asset loads: %s"), *Name), LoadObject<UStaticMesh>(nullptr, *Path));
        }
    for (int32 Rock = 1; Rock <= 3; ++Rock)
    {
        const FString Name = FString::Printf(TEXT("SM_CanyonTalus_%02d"), Rock);
        const FString Path = FString::Printf(TEXT("/Game/IslandAssets/CanyonModules/%s/StaticMeshes/%s.%s"), *Name, *Name, *Name);
        TestNotNull(*Name, LoadObject<UStaticMesh>(nullptr, *Path));
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
            int32 Floors = 0, Walls = 0, Rocks = 0, Pools = 0;
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
                    if (Component->ComponentHasTag(TEXT("Floor"))) Floors += Component->GetInstanceCount();
                    else if (Component->ComponentHasTag(TEXT("Talus"))) Rocks += Component->GetInstanceCount();
                    else Walls += Component->GetInstanceCount();
                    for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
                    {
                        FTransform Transform;
                        Component->GetInstanceTransform(Index, Transform);
                        TestFalse(TEXT("Placed asset transforms are finite"), Transform.ContainsNaN());
                        TestTrue(TEXT("Placed asset scale stays positive"), Transform.GetScale3D().GetMin() > 0.f);
                    }
                }
            TestEqual(TEXT("Exactly twelve asset pools are active"), Pools, 12);
            TestTrue(TEXT("Imported floors appear in the generated map"), Floors > 0);
            TestTrue(TEXT("Imported canyon walls appear in the generated map"), Walls > 0);
            TestTrue(TEXT("Imported talus appears in the generated map"), Rocks > 0);
            return Floors + Walls + Rocks;
        };
        const int32 FirstCount = CountAssets();
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
