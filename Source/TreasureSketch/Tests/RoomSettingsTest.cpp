#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "IpNetDriver.h"
#include "../ProceduralIsland.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchPlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoomSettingsFlowTest, "TreasureSketch.RoomSettings.RoundFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRoomSettingsFlowTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    World->URL.AddOption(TEXT("listen"));
    // Only supply a server driver identity; no sockets, Steam, or second machine required.
    UIpNetDriver* Driver = NewObject<UIpNetDriver>(World);
    Driver->SetWorld(World);
    World->SetNetDriver(Driver);
    ATreasureSketchGameMode* GM = World->SpawnActor<ATreasureSketchGameMode>();
    ATreasureSketchGameState* GS = World->SpawnActor<ATreasureSketchGameState>();
    World->SetGameState(GS);
    GM->GameState = GS;
    ATreasureSketchPlayerState* Scout = World->SpawnActor<ATreasureSketchPlayerState>();
    ATreasureSketchPlayerState* Hunter = World->SpawnActor<ATreasureSketchPlayerState>();
    Scout->PlayerRole = ETreasurePlayerRole::Scout;
    Hunter->PlayerRole = ETreasurePlayerRole::Hunter;
    GS->PlayerArray.AddUnique(Scout);
    GS->PlayerArray.AddUnique(Hunter);

    TestEqual(TEXT("Host world"), World->GetNetMode(), NM_ListenServer);
    GM->AdjustRoomSetting(TEXT("DrawingTime"), -1);
    GM->AdjustRoomSetting(TEXT("SearchingTime"), 1);
    GM->StartHostedRound();
    TestTrue(TEXT("Two-player round starts"), GS->bGameStarted);
    TestEqual(TEXT("Drawing uses independent limit"), GS->GetSecondsRemaining(), 90);
    GM->AdjustRoomSetting(TEXT("DrawingTime"), 1);
    TestEqual(TEXT("Cannot change settings during round"), GS->DrawingDurationSeconds, 90);
    GM->HandoffToHunter({});
    TestEqual(TEXT("Handoff uses searching limit"), GS->GetSecondsRemaining(), 150);
    GS->RoundEndServerTime = -1.f;
    GM->Tick(0.f);
    TestEqual(TEXT("Searching timeout still finishes round"), GS->Phase, ETreasureRoundPhase::HunterTimedOut);
    GM->StartNewRound();
    TestEqual(TEXT("Replay returns to configured drawing time"), GS->GetSecondsRemaining(), 90);
    TestEqual(TEXT("Replay keeps searching setting"), GS->SearchingDurationSeconds, 150);
    GS->Phase = ETreasureRoundPhase::Won;
    GM->ReturnToSetup();
    TestFalse(TEXT("Return to setup stops round without removing party"), GS->bGameStarted);
    TestEqual(TEXT("Return keeps party"), GS->PlayerArray.Num(), 2);
    TestEqual(TEXT("Return keeps settings"), GS->SearchingDurationSeconds, 150);

    GS->bGameStarted = false;
    GS->Phase = ETreasureRoundPhase::ScoutDrawing;
    // Visit both size bounds and verify the actual generated terrain, not just lobby state.
    const int32 Directions[] = {-1, 1, 1, 1};
    const int32 ExpectedSizes[] = {25, 39, 51, 61};
    for (int32 I = 0; I < 4; ++I)
    {
        GM->AdjustRoomSetting(TEXT("MapSize"), Directions[I]);
        GM->StartHostedRound();
        AProceduralIsland* ActiveIsland = nullptr;
        for (TActorIterator<AProceduralIsland> It(World); It; ++It)
            if (!It->IsActorBeingDestroyed()) ActiveIsland = *It;
        TestNotNull(TEXT("Island generated"), ActiveIsland);
        if (ActiveIsland) TestEqual(TEXT("Terrain uses selected size"), ActiveIsland->GridSize, ExpectedSizes[I]);
        GS->bGameStarted = false;
    }
    for (int32 I = 0; I < 30; ++I)
    {
        GM->AdjustRoomSetting(TEXT("MapSize"), 1);
        GM->AdjustRoomSetting(TEXT("DrawingTime"), -1);
        GM->AdjustRoomSetting(TEXT("SearchingTime"), 1);
    }
    TestEqual(TEXT("Size upper bound"), GS->RoomGridSize, 61);
    TestEqual(TEXT("Drawing lower bound"), GS->DrawingDurationSeconds, 30);
    TestEqual(TEXT("Searching upper bound"), GS->SearchingDurationSeconds, 600);
    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    GM->StartSoloTest(-3);
    TestEqual(TEXT("Solo full flow uses drawing setting"), GS->GetSecondsRemaining(), 30);
    GM->HandoffToHunter({});
    TestEqual(TEXT("Solo handoff uses searching setting"), GS->GetSecondsRemaining(), 600);
    GM->StartNewRound();
    TestEqual(TEXT("Solo replay keeps drawing setting"), GS->GetSecondsRemaining(), 30);
    GM->StartSoloTest(-2);
    TestEqual(TEXT("Explorer test uses searching setting"), GS->GetSecondsRemaining(), 600);
    for (TActorIterator<AProceduralIsland> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed()) TestEqual(TEXT("Solo terrain uses selected size"), It->GridSize, 61);
    GM->StartSoloTest(0);
    TestEqual(TEXT("Map preview timer remains unchanged"), GS->GetSecondsRemaining(), 3600);
    for (TActorIterator<AProceduralIsland> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed()) TestEqual(TEXT("Map preview size remains unchanged"), It->GridSize, 39);
    World->DestroyWorld(false);
    return true;
}

#endif
