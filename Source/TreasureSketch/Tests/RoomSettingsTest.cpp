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
    TestTrue(TEXT("Beach is in the default room map pool"), GS->bBeachInMapPool);
    TestTrue(TEXT("Forest is in the default room map pool"), GS->bForestInMapPool);
    TestTrue(TEXT("Host can remove beach from the map pool"), GM->ToggleRoomMapPool(EIslandTheme::PirateBeach));
    TestFalse(TEXT("Beach selection is removed"), GS->bBeachInMapPool);
    TestFalse(TEXT("Cannot remove the last available map"), GM->ToggleRoomMapPool(EIslandTheme::MistForest));
    TestEqual(TEXT("Beach-only pool always selects beach"),
        AProceduralIsland::SelectThemeFromTable(1000, false, 1u << static_cast<uint8>(EIslandTheme::PirateBeach)),
        EIslandTheme::PirateBeach);
    TestEqual(TEXT("Forest-only pool always selects forest"),
        AProceduralIsland::SelectThemeFromTable(1000, false, 1u << static_cast<uint8>(EIslandTheme::MistForest)),
        EIslandTheme::MistForest);
    GM->AdjustRoomSetting(TEXT("TreasureRange"), 1);
    GM->AdjustRoomSetting(TEXT("SpreadPlayerSpawns"), 1);
    GM->AdjustRoomSetting(TEXT("SketchSceneLock"), 1);
    GM->AdjustRoomSetting(TEXT("PreprintedIsland"), 1);
    GM->AdjustRoomSetting(TEXT("LimitedInk"), 1);
    GM->AdjustRoomSetting(TEXT("InkLimit"), -1);
    TestTrue(TEXT("Room can lock drawing to the board"), GS->bSketchSceneLock);
    TestTrue(TEXT("Room can preprint island outline"), GS->bPreprintedIsland);
    TestTrue(TEXT("Room can limit ink"), GS->bLimitedInk);
    TestEqual(TEXT("Ink limit adjusts by one hundred"), GS->InkLimit, 500);
    TestFalse(TEXT("Can hide treasure range without hiding marker"), GS->bTreasureRangeVisible);
    TestTrue(TEXT("Can choose spread out spawns"), GS->bSpreadPlayerSpawns);
    TestEqual(TEXT("Dig cooldown defaults to ten seconds"), GS->DigCooldownSeconds, 10);
    GM->AdjustRoomSetting(TEXT("DigCooldown"), -1);
    TestEqual(TEXT("Host can reduce dig cooldown in five-second steps"), GS->DigCooldownSeconds, 5);
    GM->AdjustRoomSetting(TEXT("DigCooldown"), 1);
    GM->AdjustRoomSetting(TEXT("DrawingTime"), -1);
    GM->AdjustRoomSetting(TEXT("SearchingTime"), 1);
    GM->ToggleSurfacePaint();
    GM->StartHostedRound();
    TestTrue(TEXT("Two-player round starts"), GS->bGameStarted);
    FHitResult PaintHit;
    PaintHit.ImpactPoint = FVector(100.f, 200.f, 300.f);
    PaintHit.ImpactNormal = FVector::UpVector;
    GM->SpraySurface(PaintHit);
    TestEqual(TEXT("Accepted spray stamp updates shared remaining amount"), GS->SurfacePaintStampsUsed, 1);
    GM->SpraySurface(PaintHit);
    TestEqual(TEXT("Duplicate spray stamp does not consume paint"), GS->SurfacePaintStampsUsed, 1);
    for (TActorIterator<AProceduralIsland> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed()) TestEqual(TEXT("Forest-only room generates forest"), It->Theme, EIslandTheme::MistForest);
    TestFalse(TEXT("Map pool is locked after start"), GM->ToggleRoomMapPool(EIslandTheme::PirateBeach));
    TestEqual(TEXT("Drawing uses independent limit"), GS->GetSecondsRemaining(), 90);
    GM->AdjustRoomSetting(TEXT("DrawingTime"), 1);
    TestEqual(TEXT("Cannot change settings during round"), GS->DrawingDurationSeconds, 90);
    GM->AdjustRoomSetting(TEXT("TreasureRange"), 1);
    GM->AdjustRoomSetting(TEXT("SpreadPlayerSpawns"), 1);
    GM->AdjustRoomSetting(TEXT("DigCooldown"), 1);
    GM->AdjustRoomSetting(TEXT("InkLimit"), 1);
    GM->AdjustRoomSetting(TEXT("SketchSceneLock"), 1);
    TestFalse(TEXT("Range setting locked during round"), GS->bTreasureRangeVisible);
    TestTrue(TEXT("Spawn setting locked during round"), GS->bSpreadPlayerSpawns);
    TestEqual(TEXT("Dig cooldown is locked during round"), GS->DigCooldownSeconds, 10);
    TestEqual(TEXT("Ink limit is locked during round"), GS->InkLimit, 500);
    TestTrue(TEXT("Scene lock is locked during round"), GS->bSketchSceneLock);
    GS->RoundEndServerTime = -1.f;
    GM->Tick(0.f);
    TestEqual(TEXT("Drawing timeout automatically starts searching"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    TestEqual(TEXT("Handoff uses searching limit"), GS->GetSecondsRemaining(), 150);
    GS->RoundEndServerTime = 123.f;
    GM->HandoffToHunter({});
    TestEqual(TEXT("Late submission cannot reset searching timer"), GS->RoundEndServerTime, 123.f);
    GS->RoundEndServerTime = -1.f;
    GM->Tick(0.f);
    TestEqual(TEXT("Searching timeout still finishes round"), GS->Phase, ETreasureRoundPhase::HunterTimedOut);
    GM->StartNewRound();
    TestEqual(TEXT("New round restores all paint"), GS->SurfacePaintStampsUsed, 0);
    TestFalse(TEXT("Replay retains beach exclusion"), GS->bBeachInMapPool);
    TestTrue(TEXT("Replay retains forest selection"), GS->bForestInMapPool);
    TestEqual(TEXT("Replay returns to configured drawing time"), GS->GetSecondsRemaining(), 90);
    TestEqual(TEXT("Replay keeps searching setting"), GS->SearchingDurationSeconds, 150);
    TestEqual(TEXT("Replay keeps dig cooldown setting"), GS->DigCooldownSeconds, 10);
    TestEqual(TEXT("Replay keeps ink limit"), GS->InkLimit, 500);
    TestTrue(TEXT("Replay keeps preprinted island setting"), GS->bPreprintedIsland);
    GS->Phase = ETreasureRoundPhase::Won;
    GM->ReturnToSetup();
    TestFalse(TEXT("Lobby return retains beach exclusion"), GS->bBeachInMapPool);
    TestTrue(TEXT("Host can add beach back in lobby"), GM->ToggleRoomMapPool(EIslandTheme::PirateBeach));
    TestTrue(TEXT("Host can exclude forest when beach is available"), GM->ToggleRoomMapPool(EIslandTheme::MistForest));
    for (TActorIterator<AProceduralIsland> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed()) TestEqual(TEXT("Excluded lobby preview changes to beach"), It->Theme, EIslandTheme::PirateBeach);
    TestTrue(TEXT("Host can restore both maps"), GM->ToggleRoomMapPool(EIslandTheme::MistForest));
    TestFalse(TEXT("Return to setup stops round without removing party"), GS->bGameStarted);
    TestEqual(TEXT("Return keeps party"), GS->PlayerArray.Num(), 2);
    TestEqual(TEXT("Return keeps settings"), GS->SearchingDurationSeconds, 150);
    TestEqual(TEXT("Return keeps dig cooldown setting"), GS->DigCooldownSeconds, 10);
    TestFalse(TEXT("Return keeps range setting"), GS->bTreasureRangeVisible);
    TestTrue(TEXT("Return keeps spawn setting"), GS->bSpreadPlayerSpawns);

    GS->bGameStarted = false;
    GS->Phase = ETreasureRoundPhase::ScoutDrawing;
    // Visit both size bounds and verify the actual generated terrain, not just lobby state.
    const float Scales[] = {0.5f, 1.f, 1.37f, 2.f};
    for (int32 I = 0; I < 4; ++I)
    {
        TestTrue(TEXT("Custom scale accepted"), GM->SetRoomMapScale(Scales[I]));
        GM->StartHostedRound();
        AProceduralIsland* ActiveIsland = nullptr;
        for (TActorIterator<AProceduralIsland> It(World); It; ++It)
            if (!It->IsActorBeingDestroyed()) ActiveIsland = *It;
        TestNotNull(TEXT("Island generated"), ActiveIsland);
        if (ActiveIsland)
        {
            const int32 BaseCells = 38;
            const float BaseExtent = ActiveIsland->Theme == EIslandTheme::MistForest ? 9405.f : 12540.f;
            TestEqual(TEXT("Terrain uses selected size"), ActiveIsland->GridSize, FMath::RoundToInt(BaseCells * FMath::Sqrt(Scales[I])) + 1);
            TestTrue(TEXT("Terrain extent matches exact multiplier"), FMath::IsNearlyEqual(
                (ActiveIsland->GridSize - 1) * ActiveIsland->CellSize, BaseExtent * FMath::Sqrt(Scales[I]), 0.01f));
        }
        GS->bGameStarted = false;
    }
    for (int32 I = 0; I < 30; ++I)
    {
        GM->AdjustRoomSetting(TEXT("MapSize"), 1);
        GM->AdjustRoomSetting(TEXT("DrawingTime"), -1);
        GM->AdjustRoomSetting(TEXT("SearchingTime"), 1);
    }
    TestEqual(TEXT("Size upper bound"), GS->RoomMapScale, 5.f);
    TestFalse(TEXT("Reject too-small custom map"), GM->SetRoomMapScale(0.1f));
    TestFalse(TEXT("Reject too-large custom map"), GM->SetRoomMapScale(6.f));
    TestEqual(TEXT("Drawing lower bound"), GS->DrawingDurationSeconds, 30);
    TestEqual(TEXT("Searching upper bound"), GS->SearchingDurationSeconds, 600);
    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    GM->StartSoloTest(-3);
    TestEqual(TEXT("Solo full flow uses drawing setting"), GS->GetSecondsRemaining(), 30);
    GS->RoundEndServerTime = -1.f;
    GM->Tick(0.f);
    TestEqual(TEXT("Solo drawing timeout also hands off"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    TestEqual(TEXT("Solo handoff uses searching setting"), GS->GetSecondsRemaining(), 600);
    GM->StartNewRound();
    TestEqual(TEXT("Solo replay keeps drawing setting"), GS->GetSecondsRemaining(), 30);
    GM->StartSoloTest(-2);
    TestEqual(TEXT("Explorer test uses searching setting"), GS->GetSecondsRemaining(), 600);
    for (TActorIterator<AProceduralIsland> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed()) TestEqual(TEXT("Solo terrain uses selected size"), It->GridSize, FMath::RoundToInt(38.f * FMath::Sqrt(5.f)) + 1);
    GM->StartSoloTest(0);
    TestEqual(TEXT("Map preview timer remains unchanged"), GS->GetSecondsRemaining(), 3600);
    for (TActorIterator<AProceduralIsland> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed()) TestEqual(TEXT("Map preview size remains unchanged"), It->GridSize, 39);
    GM->StartSoloTest(1);
    for (TActorIterator<AProceduralIsland> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed() && It->Theme == EIslandTheme::MistForest)
        {
            TestEqual(TEXT("Forest 1x keeps the standard grid"), It->GridSize, 39);
            TestTrue(TEXT("Forest 1x side length is 75 percent of its former size"), FMath::IsNearlyEqual(
                (It->GridSize - 1) * It->CellSize, 12540.f * 0.75f, 0.01f));
            It->MapScale = 2.f;
            It->ConfigureThemeParameters();
            TestTrue(TEXT("Forest room area multiplier scales from the new base"), FMath::IsNearlyEqual(
                (It->GridSize - 1) * It->CellSize, 12540.f * 0.75f * FMath::Sqrt(2.f), 0.01f));
        }
    GM->StartSoloTest(2);
    const int32 CanyonSeed = GS->IslandSeed;
    bool bCanyonFound = false;
    for (TActorIterator<AProceduralIsland> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed() && It->Theme == EIslandTheme::CanyonGraybox)
        { bCanyonFound = true; TestEqual(TEXT("Canyon graybox uses its own terrain grid"), It->GridSize, 129); }
    TestTrue(TEXT("Canyon graybox starts in solo test"), bCanyonFound);
    TestTrue(TEXT("Solo map preview can be refreshed"), GM->RefreshSoloMap());
    TestNotEqual(TEXT("Refresh chooses a new seed"), GS->IslandSeed, CanyonSeed);
    World->DestroyWorld(false);
    return true;
}

#endif
