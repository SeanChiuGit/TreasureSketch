#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "IpNetDriver.h"
#include "../ProceduralIsland.h"
#include "../TreasureSketchCharacter.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchPlayerController.h"
#include "../TreasureSketchPlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeamVersusFlowTest, "TreasureSketch.RoomSettings.TeamVersus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTeamVersusFlowTest::RunTest(const FString& Parameters)
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
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* PS = World->SpawnActor<ATreasureSketchPlayerState>();
        auto* PC = World->SpawnActor<ATreasureSketchPlayerController>();
        auto* Pawn = World->SpawnActor<ATreasureSketchCharacter>();
        PS->SetPlayerId(Index + 1);
        PS->SetPlayerName(FString::Printf(TEXT("Player%d"), Index + 1));
        PC->SetPlayerState(PS);
        PS->SetOwner(PC);
        PC->Possess(Pawn);
        GS->PlayerArray.AddUnique(PS);
        Players.Add(PS);
        Controllers.Add(PC);
    }
    TestTrue(TEXT("2v2 is available in lobby"), GM->SelectRoomMode(ETreasureRoomMode::TeamVersus));
    for (int32 Index = 0; Index < 4; ++Index)
    {
        TestEqual(TEXT("Two players share each team"), Players[Index]->VersusTeam, Index / 2);
        TestEqual(TEXT("Each team has a hider and mapmaker"), Players[Index]->PlayerRole,
            Index % 2 == 0 ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout);
        for (int32 OtherIndex = 0; OtherIndex < 4; ++OtherIndex)
            if (OtherIndex != Index)
                TestEqual(TEXT("Only opposing hider and mapmaker can see and collide with each other"),
                    Players[Index]->IsVersusCounterpart(Players[OtherIndex]),
                    Index / 2 != OtherIndex / 2 && Index % 2 != OtherIndex % 2);
    }
    GM->StartHostedRound();
    TestTrue(TEXT("Four-player versus round starts"), GS->bGameStarted);
    TestEqual(TEXT("Hiding and drawing run together"), GS->Phase, ETreasureRoundPhase::ScoutDrawing);
    for (int32 Index = 0; Index < 4; ++Index)
        for (int32 OtherIndex = 0; OtherIndex < 4; ++OtherIndex)
            if (OtherIndex != Index)
                TestEqual(TEXT("Movement ignores everyone outside the visible pair"),
                    CastChecked<ATreasureSketchCharacter>(Controllers[Index]->GetPawn())
                        ->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Controllers[OtherIndex]->GetPawn()),
                    !Players[Index]->IsVersusCounterpart(Players[OtherIndex]));
    TestFalse(TEXT("Mapmaker cannot place treasure"), GM->TryPlaceVersusTreasure(Controllers[1]));
    FRandomStream FirstHidingStream(1731);
    Controllers[0]->GetPawn()->SetActorLocation(
        GM->Island->FindRandomLandPoint(FirstHidingStream, 115.f) + FVector(0.f, 0.f, 180.f));
    for (int32 HiderIndex : { 0, 2 })
        CastChecked<ATreasureSketchCharacter>(Controllers[HiderIndex]->GetPawn())
            ->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    const FVector FirstLocal = Controllers[0]->GetPawn()->GetActorLocation() - GM->Island->GetActorLocation();
    TestTrue(TEXT("First hider is in walking mode"), CastChecked<ATreasureSketchCharacter>(
        Controllers[0]->GetPawn())->GetCharacterMovement()->IsMovingOnGround());
    TestTrue(TEXT("First hiding spot is dry land"), GM->Island->HeightAt(FirstLocal.X, FirstLocal.Y) >= -100.f);
    TestTrue(TEXT("First hider places a treasure"), GM->TryPlaceVersusTreasure(Controllers[0]));
    TestFalse(TEXT("Hider can place only once"), GM->TryPlaceVersusTreasure(Controllers[0]));
    FSketchStroke RedClue;
    RedClue.ColorIndex = 1;
    RedClue.Points = { FVector2D(0.2f, 0.3f) };
    FSketchStroke BlueClue;
    BlueClue.ColorIndex = 2;
    BlueClue.Points = { FVector2D(0.7f, 0.8f) };
    GM->SubmitPlayerSketch(Players[1], { RedClue });
    GM->SubmitPlayerSketch(Players[3], { BlueClue });
    TestEqual(TEXT("Drawing cannot finish before both treasures are placed"), GS->Phase, ETreasureRoundPhase::ScoutDrawing);
    FRandomStream Stream(10231);
    bool bFoundFarLand = false;
    for (int32 Attempt = 0; Attempt < 300; ++Attempt)
    {
        const FVector Candidate = GM->Island->FindRandomLandPoint(Stream, 115.f) + FVector(0.f, 0.f, 180.f);
        if (FVector::Dist2D(Candidate, GM->VersusTreasureLocations[0]) < 1500.f) continue;
        Controllers[2]->GetPawn()->SetActorLocation(Candidate);
        bFoundFarLand = true;
        break;
    }
    TestTrue(TEXT("Island offers two separated hiding spots"), bFoundFarLand);
    TestTrue(TEXT("Second hider places the other treasure"), GM->TryPlaceVersusTreasure(Controllers[2]));
    TestEqual(TEXT("Both maps and treasures start the search"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    const TArray<FSketchPage> AllPages = GM->CollectSketchPages();
    const TArray<FSketchPage> RedTeamPages = GM->VersusPagesForTeam(AllPages, 0);
    const TArray<FSketchPage> BlueTeamPages = GM->VersusPagesForTeam(AllPages, 1);
    TestEqual(TEXT("Red team gets exactly its mapmaker's page"), RedTeamPages.Num(), 1);
    TestEqual(TEXT("Blue team gets exactly its mapmaker's page"), BlueTeamPages.Num(), 1);
    Controllers[0]->ClientReceiveSketchPages_Implementation(GS->RoundSerial, RedTeamPages);
    Controllers[2]->ClientReceiveSketchPages_Implementation(GS->RoundSerial, BlueTeamPages);
    TestEqual(TEXT("First seeker receives one teammate map"), Controllers[0]->GetSketchPageCount(), 1);
    TestEqual(TEXT("First seeker receives its teammate's name"), Controllers[0]->GetActiveMapmakerName(), FString(TEXT("Player2")));
    if (!Controllers[0]->GetStrokes().IsEmpty())
        TestEqual(TEXT("First seeker gets the red clue"), Controllers[0]->GetStrokes()[0].ColorIndex, static_cast<uint8>(1));
    else AddError(TEXT("First seeker received an empty drawing"));
    TestEqual(TEXT("Second seeker receives one teammate map"), Controllers[2]->GetSketchPageCount(), 1);
    TestEqual(TEXT("Second seeker receives its teammate's name"), Controllers[2]->GetActiveMapmakerName(), FString(TEXT("Player4")));
    if (!Controllers[2]->GetStrokes().IsEmpty())
        TestEqual(TEXT("Second seeker gets the blue clue"), Controllers[2]->GetStrokes()[0].ColorIndex, static_cast<uint8>(2));
    else AddError(TEXT("Second seeker received an empty drawing"));
    TestFalse(TEXT("Mapmaker cannot dig"), [&]()
        { float Distance = 0.f; bool bAttempted = false;
          return GM->TryDig(Players[1], GM->VersusTreasureLocations[0], Distance, bAttempted); }());
    TestFalse(TEXT("Hider cannot shove after becoming seeker"), GM->TryShove(Controllers[0]));
    TestTrue(TEXT("Mapmaker can defend with a shove"), GM->TryShove(Controllers[1]));
    float Distance = 0.f;
    bool bAttempted = false;
    TestFalse(TEXT("A team cannot win by digging its own treasure"), GM->TryDig(Players[0],
        GM->VersusTreasureLocations[0], Distance, bAttempted));
    TestTrue(TEXT("Wrong treasure still counts as a dig attempt"), bAttempted);
    Players[0]->NextDigServerTime = 0.f;
    TestTrue(TEXT("Finding the opponent's treasure wins"), GM->TryDig(Players[0],
        GM->VersusTreasureLocations[1], Distance, bAttempted));
    TestEqual(TEXT("Winner is the seeking player's team"), GS->VersusWinningTeam, 0);
    TestEqual(TEXT("Round ends immediately"), GS->Phase, ETreasureRoundPhase::Won);
    GM->StartNewRound();
    TestEqual(TEXT("Replay clears both placements"), GS->VersusWinningTeam, -1);
    TestFalse(TEXT("First team placement resets"), Players[0]->bVersusTreasurePlaced);
    TestFalse(TEXT("Second team placement resets"), Players[2]->bVersusTreasurePlaced);
    GS->Phase = ETreasureRoundPhase::Won;
    GM->StartNewRound(true);
    TestEqual(TEXT("Role swap keeps first team together"), Players[0]->VersusTeam, Players[1]->VersusTeam);
    TestEqual(TEXT("Former hider can draw next round"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    TestEqual(TEXT("Former mapmaker can hide next round"), Players[1]->PlayerRole, ETreasurePlayerRole::Hunter);
    TestFalse(TEXT("Opposing hider remains outside the visible pair after role swap"),
        Players[0]->IsVersusCounterpart(Players[2]));
    TestTrue(TEXT("Opposing mapmaker remains the visible counterpart after role swap"),
        Players[0]->IsVersusCounterpart(Players[3]));

    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
