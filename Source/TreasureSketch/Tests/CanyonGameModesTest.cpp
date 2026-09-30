#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "IpNetDriver.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "../ProceduralIsland.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchPlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonGameModesTest, "TreasureSketch.Canyon.AllGameModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonGameModesTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    World->URL.AddOption(TEXT("listen"));
    UIpNetDriver* Driver = NewObject<UIpNetDriver>(World);
    Driver->SetWorld(World);
    World->SetNetDriver(Driver);
    ATreasureSketchGameMode* GM = World->SpawnActor<ATreasureSketchGameMode>();
    ATreasureSketchGameState* GS = World->SpawnActor<ATreasureSketchGameState>();
    World->SetGameState(GS);
    GM->GameState = GS;
    GS->bBeachInMapPool = false;
    GS->bForestInMapPool = false;
    GS->bCanyonInMapPool = true;
    ATreasureSketchPlayerState* Players[3];
    for (ATreasureSketchPlayerState*& Player : Players)
    {
        Player = World->SpawnActor<ATreasureSketchPlayerState>();
        GS->PlayerArray.AddUnique(Player);
    }

    const ETreasureRoomMode Modes[] = {
        ETreasureRoomMode::OneMapmaker, ETreasureRoomMode::OneExplorer,
        ETreasureRoomMode::ExplorerRace
    };
    const float Scales[] = { 0.5f, 1.f, 2.f };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        GS->RoomMode = Modes[Index];
        GS->RoomMapScale = Scales[Index];
        GS->Phase = ETreasureRoundPhase::ScoutDrawing;
        GS->bGameStarted = false;
        for (int32 PlayerIndex = 0; PlayerIndex < 3; ++PlayerIndex)
            Players[PlayerIndex]->PlayerRole = (Modes[Index] == ETreasureRoomMode::OneExplorer
                ? PlayerIndex != 2 : PlayerIndex == 0)
                ? ETreasurePlayerRole::Scout : ETreasurePlayerRole::Hunter;

        GM->StartHostedRound();
        TestTrue(TEXT("Canyon round starts in selected multiplayer mode"), GS->bGameStarted);
        AProceduralIsland* Island = nullptr;
        for (TActorIterator<AProceduralIsland> It(World); It; ++It)
            if (!It->IsActorBeingDestroyed()) Island = *It;
        if (!TestNotNull(TEXT("Canyon island exists"), Island)) continue;
        TestEqual(TEXT("Canyon is chosen from the room pool"), Island->Theme, EIslandTheme::CanyonGraybox);
        TestTrue(TEXT("Canyon respects room area multiplier"),
            FMath::IsNearlyEqual(Island->MapScale, Scales[Index])
            && FMath::IsNearlyEqual((Island->GridSize - 1) * Island->CellSize,
                12540.f * FMath::Sqrt(Scales[Index]), 0.1f));
        FRandomStream Stream(125 + Index);
        const FVector Treasure = Island->FindTreasurePoint(Stream);
        TestTrue(TEXT("Treasure uses the planned canyon goal"),
            FVector::Dist2D(Treasure, GM->GetTreasureLocation()) < 1.f);
        for (int32 Attempt = 0; Attempt < 12; ++Attempt)
        {
            const FVector Candidate = Island->FindRandomLandPoint(Stream, 115.f);
            TestTrue(TEXT("Spread spawn does not use the treasure node"),
                FVector::Dist2D(Candidate, Treasure) > 100.f);
        }
        const FVector Nearby[] = { Island->FindSpawnPoint(0.f), Island->FindSpawnPoint(600.f),
            Island->FindSpawnPoint(-600.f), Island->FindSpawnPoint(1200.f) };
        for (int32 A = 0; A < 4; ++A)
            for (int32 B = A + 1; B < 4; ++B)
                TestTrue(TEXT("Nearby players have separate canyon spawn positions"),
                    FVector::Dist2D(Nearby[A], Nearby[B]) >= 180.f);
        if (Index == 0)
        {
            TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
            Island->GetComponents(Components);
            UHierarchicalInstancedStaticMeshComponent* Palm = nullptr;
            UHierarchicalInstancedStaticMeshComponent* Anchor = nullptr;
            UHierarchicalInstancedStaticMeshComponent* ForestTree = nullptr;
            for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
            {
                if (Component->GetFName() == FName(TEXT("PalmInstances"))) Palm = Component;
                if (Component->GetFName() == FName(TEXT("GiantAnchorInstances"))) Anchor = Component;
                if (Component->GetFName() == FName(TEXT("ForestInstances_0"))) ForestTree = Component;
            }
            if (TestNotNull(TEXT("Beach palm component exists"), Palm)
                && TestNotNull(TEXT("Beach anchor component exists"), Anchor)
                && TestNotNull(TEXT("Forest tree component exists"), ForestTree))
            {
                Palm->AddInstance(FTransform::Identity);
                Anchor->AddInstance(FTransform::Identity);
                ForestTree->AddInstance(FTransform::Identity);
                Island->OnConstruction(Island->GetActorTransform());
                TestEqual(TEXT("Canyon rebuild removes stale palms"), Palm->GetInstanceCount(), 0);
                TestEqual(TEXT("Canyon rebuild removes stale anchors"), Anchor->GetInstanceCount(), 0);
                TestEqual(TEXT("Canyon rebuild removes stale forest trees"), ForestTree->GetInstanceCount(), 0);
            }
        }
    }
    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false);
    return true;
}

#endif
