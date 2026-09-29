#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IpNetDriver.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchPlayerController.h"
#include "../TreasureSketchPlayerState.h"
#include "../TreasureSketchCharacter.h"

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
    TestTrue(TEXT("A player can claim the sole explorer role in the lobby"), GM->ClaimSingleRoomRole(Players[2]));
    TestEqual(TEXT("Claimant becomes explorer"), Players[2]->PlayerRole, ETreasurePlayerRole::Hunter);
    TestEqual(TEXT("Previous explorer becomes mapmaker"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    TestTrue(TEXT("The original explorer can claim the role again"), GM->ClaimSingleRoomRole(Players[0]));
    GM->StartHostedRound();
    TestTrue(TEXT("Four-player new mode starts"), GS->bGameStarted);
    TestFalse(TEXT("Cannot switch mode mid-round"), GM->SelectRoomMode(ETreasureRoomMode::OneMapmaker));
    TestFalse(TEXT("Cannot claim a role mid-round"), GM->ClaimSingleRoomRole(Players[2]));
    World->SetNetDriver(nullptr);
    Controllers[1]->SetPauseMenuOpen(true);
    TestTrue(TEXT("A mapmaker can open the in-round menu"), Controllers[1]->IsPauseMenuOpen());
    Controllers[1]->HandleFrontEndAction(TEXT("PauseResume"));
    TestFalse(TEXT("Resume closes the in-round menu"), Controllers[1]->IsPauseMenuOpen());
    Controllers[1]->ClientRevealTreasure_Implementation(GM->GetTreasureLocation());
    Controllers[1]->ToggleSpectatorView();
    Controllers[1]->UpdateSpectatorCamera(0.f);
    TestTrue(TEXT("Mapmaker can fly above the island while drawing"), Controllers[1]->IsDrawingOverheadView() && Controllers[1]->IsScoutSpectating());
    Controllers[1]->ToggleMap();
    TestTrue(TEXT("Drawing remains available from the overhead view"), Controllers[1]->IsMapOpen());
    Controllers[1]->ToggleSpectatorView();
    TestFalse(TEXT("Tab closes the drawing board and returns to the ground"), Controllers[1]->IsDrawingOverheadView() || Controllers[1]->IsMapOpen());
    TestTrue(TEXT("Returning from overhead keeps the treasure marker"), Controllers[1]->bTreasureMarkerVisible);
    GS->bSketchSceneLock = true;
    Controllers[1]->ToggleMap();
    Controllers[1]->ToggleMap();
    Controllers[1]->ToggleSpectatorView();
    TestTrue(TEXT("Locked drawing board cannot return to the scene"), Controllers[1]->IsMapOpen()
        && !Controllers[1]->IsDrawingOverheadView());
    GS->bSketchSceneLock = false;
    Controllers[1]->ToggleMap();
    World->SetNetDriver(Driver);

    FSketchStroke FirstStroke;
    FirstStroke.ColorIndex = 1;
    FirstStroke.Points = { FVector2D(0.1f, 0.2f), FVector2D(0.3f, 0.4f) };
    FSketchStroke OtherStroke;
    OtherStroke.ColorIndex = 2;
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
        Explorer->ClientAppendLiveSketch_Implementation(GS->RoundSerial, Players[1]->GetPlayerId(), 0, 1, 0, FirstStroke.Points);
        TestEqual(TEXT("First mapmaker's strokes appear live"), Explorer->GetStrokes()[0].Points.Num(), 2);
        TestEqual(TEXT("Live stroke keeps its chosen color"), Explorer->GetStrokes()[0].ColorIndex, 1);
        Explorer->ClientAppendLiveSketch_Implementation(GS->RoundSerial, Players[2]->GetPlayerId(), 0, 2, 0, OtherStroke.Points);
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
        Explorer->ClientAppendLiveSketch_Implementation(GS->RoundSerial - 1, Players[1]->GetPlayerId(), 0, 2, 0, OtherStroke.Points);
        TestEqual(TEXT("Old round live update is rejected"), Explorer->GetStrokes()[0].Points.Num(), 2);
    }
    World->SetNetDriver(Driver);
    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    const TSharedPtr<IImageWrapper> Jpeg = Images.CreateImageWrapper(EImageFormat::JPEG);
    TArray<FColor> PhotoPixels;
    PhotoPixels.Init(FColor::Green, 384 * 216);
    TArray<uint8> TestPhoto;
    if (Jpeg.IsValid() && Jpeg->SetRaw(PhotoPixels.GetData(), PhotoPixels.Num() * sizeof(FColor),
        384, 216, ERGBFormat::BGRA, 8))
    {
        const TArray64<uint8> Encoded = Jpeg->GetCompressed(50);
        TestPhoto.Append(Encoded.GetData(), static_cast<int32>(Encoded.Num()));
    }
    TestTrue(TEXT("Test photograph can be encoded"), TestPhoto.Num() > 100);
    ATreasureSketchCharacter* PhotoPawn = World->SpawnActor<ATreasureSketchCharacter>();
    Controllers[1]->Possess(PhotoPawn);
    const FVector PhotoOrigin = Controllers[1]->GetPawn()->GetActorLocation();
    const FRotator PhotoRotation = Controllers[1]->GetControlRotation();
    Controllers[1]->bServerDrawingOverheadView = true;
    Controllers[1]->ServerSubmitPhoto_Implementation(GS->RoundSerial, PhotoOrigin, PhotoRotation, TestPhoto);
    TestTrue(TEXT("Ghost camera cannot submit a photograph"), Controllers[1]->GetServerPhoto().IsEmpty());
    Controllers[1]->bServerDrawingOverheadView = false;
    Controllers[1]->ServerSubmitPhoto_Implementation(GS->RoundSerial, PhotoOrigin, PhotoRotation, TestPhoto);
    TestEqual(TEXT("Ground camera stores one photograph"), Controllers[1]->GetServerPhoto(), TestPhoto);
    Explorer->ClientReceiveLivePhoto_Implementation(GS->RoundSerial, Players[1]->GetPlayerId(), TestPhoto);
    TestEqual(TEXT("Waiting explorer receives the matching live photograph"), Explorer->GetPhotoJpeg(), TestPhoto);
    const TArray<FSketchPage> PhotoPages = GM->CollectSketchPages();
    TestEqual(TEXT("Unsubmitted map page keeps its photograph"), PhotoPages[0].PhotoJpeg, TestPhoto);
    GS->bLimitedInk = true;
    GS->InkLimit = 2;
    Controllers[1]->ServerAppendDrawing_Implementation(GS->RoundSerial, 0, 1, 0,
        { FVector2D(0.1f, 0.2f), FVector2D(0.2f, 0.3f), FVector2D(0.3f, 0.4f) });
    TestTrue(TEXT("Server rejects strokes exceeding ink limit"), Controllers[1]->ServerDrawing.IsEmpty());
    Controllers[1]->ServerAppendDrawing_Implementation(GS->RoundSerial, 0, 1, 0, FirstStroke.Points);
    TestEqual(TEXT("Server accepts ink within limit"), Controllers[1]->ServerDrawing[0].Points.Num(), 2);
    Controllers[1]->ServerAppendDrawing_Implementation(GS->RoundSerial, 1, 5, 1, FirstStroke.Points);
    TestEqual(TEXT("Large eraser is accepted without using ink"), Controllers[1]->ServerDrawing.Num(), 2);
    TestEqual(TEXT("Large eraser size is kept"), Controllers[1]->ServerDrawing[1].EraserSize, static_cast<uint8>(1));
    Controllers[1]->ServerAppendDrawing_Implementation(GS->RoundSerial, 2, 5, 2, FirstStroke.Points);
    TestEqual(TEXT("Invalid eraser size is rejected"), Controllers[1]->ServerDrawing.Num(), 2);
    GS->bLimitedInk = false;
    FSketchStroke LargeEraser = FirstStroke;
    LargeEraser.ColorIndex = 5;
    LargeEraser.EraserSize = 1;
    GM->SubmitPlayerSketch(Players[0], { FirstStroke });
    TestFalse(TEXT("Explorer cannot submit a map"), Players[0]->bSketchSubmitted);
    GM->SubmitPlayerSketch(Players[1], { FirstStroke, LargeEraser });
    GM->SubmitPlayerSketch(Players[1], { OtherStroke });
    TestTrue(TEXT("Submission readiness is stored"), Players[1]->bSketchSubmitted);
    TestEqual(TEXT("One map does not release explorer"), GS->Phase, ETreasureRoundPhase::ScoutDrawing);
    GM->SubmitPlayerSketch(Players[2], { OtherStroke });
    TestEqual(TEXT("Still waits for last mapmaker"), GS->Phase, ETreasureRoundPhase::ScoutDrawing);
    GM->SubmitPlayerSketch(Players[3], {});
    TestEqual(TEXT("All maps release explorer"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    float DigDistance = 0.f;
    bool bDigAttempted = false;
    const FVector WrongDigLocation = GM->GetTreasureLocation() + FVector(10000.f, 0.f, 0.f);
    TestFalse(TEXT("A distant dig misses"), GM->TryDig(Players[0], WrongDigLocation, DigDistance, bDigAttempted));
    TestTrue(TEXT("The first dig is attempted"), bDigAttempted);
    TestTrue(TEXT("Dig starts the configured cooldown"), Players[0]->GetDigCooldownRemaining(
        GS->RoundSerial, GS->GetServerWorldTimeSeconds()) > 0.f);
    const float FirstNextDigTime = Players[0]->NextDigServerTime;
    GM->TryDig(Players[0], WrongDigLocation, DigDistance, bDigAttempted);
    TestFalse(TEXT("Repeated dig during cooldown is ignored"), bDigAttempted);
    TestEqual(TEXT("Ignored dig does not extend cooldown"), Players[0]->NextDigServerTime, FirstNextDigTime);
    Players[0]->NextDigServerTime = GS->GetServerWorldTimeSeconds() - 1.f;
    GM->TryDig(Players[0], WrongDigLocation, DigDistance, bDigAttempted);
    TestTrue(TEXT("Explorer can dig again after cooldown"), bDigAttempted);
    World->SetNetDriver(nullptr);
    Explorer->UpdateWaitingSketchInput();
    TestFalse(TEXT("Waiting page input stops at handoff"), Explorer->bWaitingSketchInputActive);
    World->SetNetDriver(Driver);
    const TArray<FSketchPage> Pages = GM->CollectSketchPages();
    TestEqual(TEXT("All independent pages retained, including blank"), Pages.Num(), 3);
    if (Pages.Num() == 3)
    {
        TestEqual(TEXT("Submitted map keeps its photograph"), Pages[0].PhotoJpeg, TestPhoto);
        TestEqual(TEXT("Page identifies its author"), Pages[0].MapmakerName, FString(TEXT("Player1")));
        TestEqual(TEXT("Duplicate submit cannot overwrite page"), Pages[0].Strokes[0].Points[0], FVector2D(0.1f, 0.2f));
        if (TestEqual(TEXT("Final page retains eraser stroke"), Pages[0].Strokes.Num(), 2))
            TestEqual(TEXT("Final page keeps large eraser width"), Pages[0].Strokes[1].EraserSize, static_cast<uint8>(1));
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
    TestFalse(TEXT("Review starts with treasure marker hidden"), Controllers[1]->bTreasureMarkerVisible);
    Controllers[1]->ToggleSpectatorTreasure();
    TestTrue(TEXT("Mapmaker can reveal their review treasure marker"), Controllers[1]->bTreasureMarkerVisible);
    Controllers[1]->bMapOpen = true;
    Controllers[1]->HandleFrontEndAction(TEXT("ToggleReviewTreasure"));
    TestFalse(TEXT("Map button can hide the review treasure marker again"), Controllers[1]->bTreasureMarkerVisible);
    Controllers[1]->bMapOpen = false;
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
    TestEqual(TEXT("New round clears the previous explorer's cooldown"), Players[0]->GetDigCooldownRemaining(
        GS->RoundSerial, GS->GetServerWorldTimeSeconds()), 0.f);
    TestFalse(TEXT("Next round clears review state"), GS->bReviewingRound);
    TestEqual(TEXT("Replay keeps mode"), GS->RoomMode, ETreasureRoomMode::OneExplorer);
    TestTrue(TEXT("Replay clears the previous photograph"), Controllers[1]->GetServerPhoto().IsEmpty());
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
    GM->ReturnToSetup();
    TestFalse(TEXT("Host can stop an active round for everyone"), GS->bGameStarted);
    TestEqual(TEXT("Return retains new mode"), GS->RoomMode, ETreasureRoomMode::OneExplorer);
    GS->PlayerArray.Remove(Players[1]);
    GM->NormalizeRoomRoles(Players[1]);
    TestEqual(TEXT("Departing explorer is replaced"), Players[0]->PlayerRole, ETreasurePlayerRole::Hunter);
    TestTrue(TEXT("Can return to original mode"), GM->SelectRoomMode(ETreasureRoomMode::OneMapmaker));
    TestEqual(TEXT("Same single player becomes mapmaker"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    TestEqual(TEXT("Remaining members become explorers"), Players[2]->PlayerRole, ETreasurePlayerRole::Hunter);
    TestTrue(TEXT("Another player can claim sole mapmaker role"), GM->ClaimSingleRoomRole(Players[2]));
    TestEqual(TEXT("New claimant becomes mapmaker"), Players[2]->PlayerRole, ETreasurePlayerRole::Scout);
    TestEqual(TEXT("Former mapmaker becomes explorer"), Players[0]->PlayerRole, ETreasurePlayerRole::Hunter);
    TestTrue(TEXT("Mapmaker can claim the role back"), GM->ClaimSingleRoomRole(Players[0]));
    GM->StartHostedRound();
    const TArray<FSketchPage> SingleMapmakerPage = GM->CollectSketchPages();
    TestEqual(TEXT("Original mode also has a live page"), SingleMapmakerPage.Num(), 1);
    World->SetNetDriver(nullptr);
    Controllers[2]->ClientInitializeLiveSketch_Implementation(GS->RoundSerial, SingleMapmakerPage);
    TestEqual(TEXT("Original mode explorer can view live page"), Controllers[2]->GetSketchPageCount(), 1);
    World->SetNetDriver(Driver);
    GM->SubmitPlayerSketch(Players[0], {});
    TestEqual(TEXT("Single mapmaker releases multiple explorers"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    GM->TryDig(Players[2], GM->GetTreasureLocation() + FVector(10000.f, 0.f, 0.f), DigDistance, bDigAttempted);
    TestTrue(TEXT("One explorer's dig starts their own cooldown"), bDigAttempted);
    TestEqual(TEXT("Another explorer has an independent cooldown"), Players[3]->GetDigCooldownRemaining(
        GS->RoundSerial, GS->GetServerWorldTimeSeconds()), 0.f);
    GS->DigCooldownSeconds = 0;
    GM->TryDig(Players[3], GM->GetTreasureLocation() + FVector(10000.f, 0.f, 0.f), DigDistance, bDigAttempted);
    TestTrue(TEXT("Zero cooldown accepts the first dig"), bDigAttempted);
    GM->TryDig(Players[3], GM->GetTreasureLocation() + FVector(10000.f, 0.f, 0.f), DigDistance, bDigAttempted);
    TestTrue(TEXT("Zero cooldown accepts an immediate second dig"), bDigAttempted);
    TestTrue(TEXT("Another explorer completes the round"), GM->TryDig(Players[3],
        GM->GetTreasureLocation(), DigDistance, bDigAttempted));
    GM->StartNewRound(true, Players[3]);
    TestEqual(TEXT("Result choice lets the requester become mapmaker"), Players[3]->PlayerRole, ETreasurePlayerRole::Scout);

    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
