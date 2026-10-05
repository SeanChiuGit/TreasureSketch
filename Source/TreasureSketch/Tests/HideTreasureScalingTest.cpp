#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../ProceduralIsland.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHideTreasureScalingTest, "TreasureSketch.RoomSettings.HideTreasureScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHideTreasureScalingTest::RunTest(const FString& Parameters)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    for (const auto Theme : { EIslandTheme::PirateBeach, EIslandTheme::MistForest, EIslandTheme::CanyonGraybox })
        for (const float Scale : { 0.5f, 1.f })
            for (const int32 Seed : { 1001, 1050 })
            {
                auto* Island = World->SpawnActorDeferred<AProceduralIsland>(AProceduralIsland::StaticClass(), FTransform::Identity);
                Island->Theme = Theme;
                Island->MapScale = Scale;
                Island->Seed = Seed;
                Island->FinishSpawning(FTransform::Identity);
                for (const int32 Count : { 3, 6, 9 })
                    for (int32 PlacementSeed = 0; PlacementSeed < 5; ++PlacementSeed)
                    {
                        FRandomStream Stream(PlacementSeed);
                        const auto Points = Island->FindSeparatedTreasurePoints(Stream, Count, 950.f);
                        TestEqual(FString::Printf(TEXT("Theme %d scale %.1f seed %d placement %d provides %d treasures"),
                            static_cast<int32>(Theme), Scale, Seed, PlacementSeed, Count), Points.Num(), Count);
                        if (Theme == EIslandTheme::CanyonGraybox)
                        {
                            int32 CaveCount = 0;
                            for (const FVector& Point : Points)
                                if (Point.Z < Island->GetCanyonLayout().SurfaceHeightAt(Point.X, Point.Y) - 200.f) ++CaveCount;
                            TestTrue(TEXT("Every canyon placement includes covered cave treasure"), CaveCount > 0);
                        }
                        for (int32 Index = 0; Index < Points.Num(); ++Index)
                            for (int32 Other = 0; Other < Index; ++Other)
                                TestTrue(TEXT("Treasure dig ranges remain separated"),
                                    FVector::Dist2D(Points[Index], Points[Other]) > 950.f);
                    }
                Island->Destroy();
            }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
