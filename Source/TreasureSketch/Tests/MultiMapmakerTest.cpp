#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IpNetDriver.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchPlayerController.h"
#include "../TreasureSketchPlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultiMapmakerFlowTest, "TreasureSketch.RoomSettings.MultiMapmaker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMultiMapmakerFlowTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->URL.AddOption(TEXT("listen"));
    UIpNetDriver* Driver = NewObject<UIpNetDriver>(World);
    Driver->SetWorld(World);
    World->SetNetDriver(Driver);
    ATreasureSketchGameMode* GM = World->SpawnActor<ATreasureSketchGameMode>();
    ATreasureSketchGameState* GS = World->SpawnActor<ATreasureSketchGameState>();
    World->SetGameState(GS);
    GM->GameState = GS;
    TArray<ATreasureSketchPlayerState*> Players;
    TArray<ATreasureSketchPlayerController*> Controllers;
    for (int32 I = 0; I < 4; ++I)
    {
        auto* PS = World->SpawnActor<ATreasureSketchPlayerState>();
        auto* PC = World->SpawnActor<ATreasureSketchPlayerController>();
        PS->SetPlayerId(100 + I);
        PS->SetPlayerName(FString::Printf(TEXT("Player%d"), I));
        PC->SetPlayerState(PS);
        PS->SetOwner(PC);
        GS->PlayerArray.AddUnique(PS);
        Players.Add(PS);
        Controllers.Add(PC);
        GM->NormalizeRoomRoles();
    }
    TestEqual(TEXT("Original mode retains first mapmaker"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    TestTrue(TEXT("New mode is selectable"), GM->SelectRoomMode(ETreasureRoomMode::OneExplorer));
    TestFalse(TEXT("Versus is still unavailable"), GM->SelectRoomMode(ETreasureRoomMode::TeamVersus));
    TestEqual(TEXT("Host becomes sole explorer when switching mode"), Players[0]->PlayerRole, ETreasurePlayerRole::Hunter);
    for (int32 I = 1; I < 4; ++I) TestEqual(TEXT("Others become mapmakers"), Players[I]->PlayerRole, ETreasurePlayerRole::Scout);
    GM->StartHostedRound();
    TestTrue(TEXT("Four-player new mode starts"), GS->bGameStarted);
    TestFalse(TEXT("Cannot switch mode mid-round"), GM->SelectRoomMode(ETreasureRoomMode::OneMapmaker));

    FSketchStroke FirstStroke;
    FirstStroke.Points = { FVector2D(0.1f, 0.2f), FVector2D(0.3f, 0.4f) };
    FSketchStroke OtherStroke;
    OtherStroke.Points = { FVector2D(0.5f, 0.6f), FVector2D(0.7f, 0.8f) };
    auto* Explorer = Controllers[0];
    World->SetNetDriver(nullptr); // Model a local waiting explorer without a game window.
    Explorer->UpdateWaitingSketchInput();
    TestTrue(TEXT("Waiting explorer gets page-switch mouse input"), Explorer->bWaitingSketchInputActive);
    const TArray<FSketchPage> LivePages = GM->CollectSketchPages();
    TestEqual(TEXT("Waiting explorer receives every mapmaker's page"), LivePages.Num(), 3);
    Explorer->ClientInitializeLiveSketch_Implementation(GS->RoundSerial, LivePages);
    TestEqual(TEXT("Live pages are ready during drawing"), Explorer->GetSketchPageCount(), 3);
    if (LivePages.Num() == 3)
    {
        Explorer->ClientAppendLiveSketch_Implementation(GS->RoundSerial, Players[1]->GetPlayerId(), 0, FirstStroke.Points);
        TestEqual(TEXT("First mapmaker's strokes appear live"), Explorer->GetStrokes()[0].Points.Num(), 2);
        Explorer->ClientAppendLiveSketch_Implementation(GS->RoundSerial, Players[2]->GetPlayerId(), 0, OtherStroke.Points);
        Explorer->CycleSketchPage(1);
        TestEqual(TEXT("Waiting explorer can switch to second live page"), Explorer->GetActiveSketchPage(), 1);
        TestEqual(TEXT("Second live page stays independent"), Explorer->GetStrokes()[0].Points[0], FVector2D(0.5f, 0.6f));
        Explorer->ClientClearLiveSketch_Implementation(GS->RoundSerial, Players[1]->GetPlayerId());
        Explorer->CycleSketchPage(-1);
        TestTrue(TEXT("Clear removes only its author's live strokes"), Explorer->GetStrokes().IsEmpty());
        Explorer->CycleSketchPage(1);
        TestEqual(TEXT("Other author's drawing survives clear"), Explorer->GetStrokes().Num(), 1);
        FSketchPage SubmittedPage = LivePages[0];
        SubmittedPage.Strokes = { FirstStroke };
        Explorer->ClientReplaceLiveSketch_Implementation(GS->RoundSerial, SubmittedPage);
        Explorer->CycleSketchPage(-1);
        TestEqual(TEXT("Early submission replaces live page with final drawing"), Explorer->GetStrokes()[0].Points.Num(), 2);
        Explorer->ClientAppendLiveSketch_Implementation(GS->RoundSerial - 1, Players[1]->GetPlayerId(), 0, OtherStroke.Points);
        TestEqual(TEXT("Old round live update is rejected"), Explorer->GetStrokes()[0].Points.Num(), 2);
    }
    World->SetNetDriver(Driver);
    GM->SubmitPlayerSketch(Players[0], { FirstStroke });
    TestFalse(TEXT("Explorer cannot submit a map"), Players[0]->bSketchSubmitted);
    GM->SubmitPlayerSketch(Players[1], { FirstStroke });
    GM->SubmitPlayerSketch(Players[1], { OtherStroke });
    TestTrue(TEXT("Submission readiness is stored"), Players[1]->bSketchSubmitted);
    TestEqual(TEXT("One map does not release explorer"), GS->Phase, ETreasureRoundPhase::ScoutDrawing);
    GM->SubmitPlayerSketch(Players[2], { OtherStroke });
    TestEqual(TEXT("Still waits for last mapmaker"), GS->Phase, ETreasureRoundPhase::ScoutDrawing);
    GM->SubmitPlayerSketch(Players[3], {});
    TestEqual(TEXT("All maps release explorer"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    World->SetNetDriver(nullptr);
    Explorer->UpdateWaitingSketchInput();
    TestFalse(TEXT("Waiting page input stops at handoff"), Explorer->bWaitingSketchInputActive);
    World->SetNetDriver(Driver);
    const TArray<FSketchPage> Pages = GM->CollectSketchPages();
    TestEqual(TEXT("All independent pages retained, including blank"), Pages.Num(), 3);
    if (Pages.Num() == 3)
    {
        TestEqual(TEXT("Page identifies its author"), Pages[0].MapmakerName, FString(TEXT("Player1")));
        TestEqual(TEXT("Duplicate submit cannot overwrite page"), Pages[0].Strokes[0].Points[0], FVector2D(0.1f, 0.2f));
        TestEqual(TEXT("Second page retains independent drawing"), Pages[1].Strokes[0].Points[0], FVector2D(0.5f, 0.6f));
        TestTrue(TEXT("Blank third page remains blank"), Pages[2].Strokes.IsEmpty());
    }
    auto* Mapmaker = Controllers[1];
    Mapmaker->Strokes = { FirstStroke };
    World->SetNetDriver(nullptr);
    Mapmaker->ClientReceiveSketchPages_Implementation(GS->RoundSerial, Pages);
    TestEqual(TEXT("Spectating mapmaker receives every final page"), Mapmaker->GetSketchPageCount(), 3);
    TestEqual(TEXT("Spectating mapmaker starts on their own page"), Mapmaker->GetActiveSketchPage(), 0);
    TestEqual(TEXT("Mapmaker's local drawing remains available"), Mapmaker->Strokes.Num(), 1);
    Mapmaker->ToggleMap();
    TestTrue(TEXT("Mapmaker can open the map while spectating"), Mapmaker->IsMapOpen());
    Mapmaker->CycleSketchPage(1);
    TestEqual(TEXT("Mapmaker can switch to another finished page"), Mapmaker->GetActiveSketchPage(), 1);
    Mapmaker->ClearSketch();
    TestEqual(TEXT("Spectating map is read-only"), Mapmaker->GetStrokes().Num(), 1);
    Mapmaker->ToggleMap();
    TestFalse(TEXT("Mapmaker can close the map and resume spectating"), Mapmaker->IsMapOpen());
    World->SetNetDriver(Driver);
    World->SetNetDriver(nullptr); // Local controller for page-input checks, no window.
    Explorer->ClientReceiveSketchPages_Implementation(GS->RoundSerial, Pages);
    Explorer->bMapOpen = true;
    Explorer->CycleSketchPage(1);
    TestEqual(TEXT("Explorer switches to second map"), Explorer->GetActiveSketchPage(), 1);
    Explorer->CycleSketchPage(-1);
    Explorer->CycleSketchPage(-1);
    TestEqual(TEXT("Previous page wraps around"), Explorer->GetActiveSketchPage(), 2);
    Explorer->ClientReceiveSketchPages_Implementation(GS->RoundSerial - 1, {});
    TestEqual(TEXT("Old round delivery is rejected"), Explorer->GetSketchPageCount(), 3);
    World->SetNetDriver(Driver);

    GS->Phase = ETreasureRoundPhase::Won;
    AProceduralIsland* FinishedIsland = GM->Island;
    GM->SetRoundReview(true);
    TestTrue(TEXT("Review is shared by the party"), GS->bReviewingRound);
    TestEqual(TEXT("Review preserves the result"), GS->Phase, ETreasureRoundPhase::Won);
    TestTrue(TEXT("Review keeps the finished island"), GM->Island == FinishedIsland);
    TestEqual(TEXT("Review keeps every finished drawing"), GM->CollectSketchPages().Num(), 3);
    GM->StartNewRound();
    TestTrue(TEXT("Review must end before the next round"), GM->Island == FinishedIsland && GS->bReviewingRound);
    World->SetNetDriver(nullptr);
    Controllers[1]->ClientBeginReview_Implementation(GS->RoundSerial, Pages, GM->GetTreasureLocation());
    TestEqual(TEXT("Mapmaker can inspect all final pages in review"), Controllers[1]->GetSketchPageCount(), 3);
    TestTrue(TEXT("Mapmaker sees treasure during review"), Controllers[1]->bTreasureMarkerVisible);
    Explorer->bMapOpen = true;
    Explorer->CycleSketchPage(1);
    TestEqual(TEXT("Review permits switching finished drawings"), Explorer->GetActiveSketchPage(), 0);
    World->SetNetDriver(Driver);
    GM->SetRoundReview(false);
    TestFalse(TEXT("Party returns to result screen"), GS->bReviewingRound);
    TestTrue(TEXT("Ending review still keeps the finished island"), GM->Island == FinishedIsland);
    World->SetNetDriver(nullptr);
    Controllers[1]->ClientEndReview_Implementation(GS->RoundSerial);
    TestFalse(TEXT("Treasure marker hides after review"), Controllers[1]->bTreasureMarkerVisible);
    World->SetNetDriver(Driver);
    GM->StartNewRound(true);
    TestFalse(TEXT("Next round clears review state"), GS->bReviewingRound);
    TestEqual(TEXT("Replay keeps mode"), GS->RoomMode, ETreasureRoomMode::OneExplorer);
    TestEqual(TEXT("Explorer rotates to next player"), Players[1]->PlayerRole, ETreasurePlayerRole::Hunter);
    TestEqual(TEXT("Old explorer becomes mapmaker"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    for (auto* PS : Players) TestFalse(TEXT("Replay clears readiness"), PS->bSketchSubmitted);
    GM->SubmitPlayerSketch(Players[0], { OtherStroke });
    Controllers[2]->ServerDrawingRoundSerial = GS->RoundSerial;
    Controllers[2]->ServerDrawing = { FirstStroke };
    Controllers[2]->Strokes = { FirstStroke };
    GS->RoundEndServerTime = -1.f;
    GM->Tick(0.f);
    TestEqual(TEXT("Timeout releases explorer with unfinished maps"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    const TArray<FSketchPage> TimeoutPages = GM->CollectSketchPages();
    TestEqual(TEXT("Timeout collects every mapmaker"), TimeoutPages.Num(), 3);
    if (TimeoutPages.Num() == 3)
    {
        TestEqual(TEXT("Submitted map survives timeout"), TimeoutPages[0].Strokes.Num(), 1);
        TestEqual(TEXT("Unsubmitted synchronized map is included"), TimeoutPages[1].Strokes.Num(), 1);
        TestTrue(TEXT("New round does not reuse old blank-author drawing"), TimeoutPages[2].Strokes.IsEmpty());
    }
    GS->Phase = ETreasureRoundPhase::Won;
    GM->ReturnToSetup();
    TestFalse(TEXT("Return stops game"), GS->bGameStarted);
    TestEqual(TEXT("Return retains new mode"), GS->RoomMode, ETreasureRoomMode::OneExplorer);
    GS->PlayerArray.Remove(Players[1]);
    GM->NormalizeRoomRoles(Players[1]);
    TestEqual(TEXT("Departing explorer is replaced"), Players[0]->PlayerRole, ETreasurePlayerRole::Hunter);
    TestTrue(TEXT("Can return to original mode"), GM->SelectRoomMode(ETreasureRoomMode::OneMapmaker));
    TestEqual(TEXT("Same single player becomes mapmaker"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    TestEqual(TEXT("Remaining members become explorers"), Players[2]->PlayerRole, ETreasurePlayerRole::Hunter);
    GM->StartHostedRound();
    const TArray<FSketchPage> SingleMapmakerPage = GM->CollectSketchPages();
    TestEqual(TEXT("Original mode also has a live page"), SingleMapmakerPage.Num(), 1);
    World->SetNetDriver(nullptr);
    Controllers[2]->ClientInitializeLiveSketch_Implementation(GS->RoundSerial, SingleMapmakerPage);
    TestEqual(TEXT("Original mode explorer can view live page"), Controllers[2]->GetSketchPageCount(), 1);
    World->SetNetDriver(Driver);

    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
