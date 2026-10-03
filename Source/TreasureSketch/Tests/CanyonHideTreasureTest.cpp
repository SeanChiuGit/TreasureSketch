#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
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
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
