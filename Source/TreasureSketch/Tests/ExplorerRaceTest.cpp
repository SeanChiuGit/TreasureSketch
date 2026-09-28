#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "IpNetDriver.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchPlayerController.h"
#include "../TreasureSketchPlayerState.h"
#include "../TreasureSketchCharacter.h"
#include "../TreasureRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExplorerRaceFlowTest, "TreasureSketch.RoomSettings.ExplorerRace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExplorerRaceFlowTest::RunTest(const FString& Parameters)
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
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* PS = World->SpawnActor<ATreasureSketchPlayerState>();
        auto* PC = World->SpawnActor<ATreasureSketchPlayerController>();
        PS->SetPlayerId(Index + 1);
        PS->SetPlayerName(FString::Printf(TEXT("Player%d"), Index + 1));
        PC->SetPlayerState(PS);
        PS->SetOwner(PC);
        GS->PlayerArray.AddUnique(PS);
        Players.Add(PS);
        Controllers.Add(PC);
        GM->NormalizeRoomRoles();
        if (Index == 1)
        {
            TestTrue(TEXT("Host can enable race in a two-player lobby"), GM->SelectRoomMode(ETreasureRoomMode::ExplorerRace));
            GM->StartHostedRound();
            TestFalse(TEXT("Race needs a third player to start"), GS->bGameStarted);
        }
    }
    TestTrue(TEXT("Race option is selectable"), GM->SelectRoomMode(ETreasureRoomMode::ExplorerRace));
    GM->StartHostedRound();
    TestTrue(TEXT("Three-player race starts"), GS->bGameStarted);
    TestEqual(TEXT("Race has one round per player"), GS->RaceTotalRounds, 3);
    TestEqual(TEXT("First round index"), GS->RaceRoundIndex, 1);
    TestEqual(TEXT("First mapmaker"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    {
        TArray<FVector> ReservedSpawns;
        for (int32 Index = 0; Index < 3; ++Index)
            TestTrue(TEXT("Explorer spawn stays at least thirty meters from treasure"),
                FVector::Dist2D(GM->FindHunterSpawn(ReservedSpawns), GM->GetTreasureLocation())
                    >= TreasureRules::MinimumHunterSpawnDistance);
    }

    auto FinishRound = [&](int32 FinderIndex)
    {
        GM->SubmitPlayerSketch(Players[GS->RaceRoundIndex - 1], {});
        TestEqual(TEXT("Searching begins after map handoff"), GS->Phase, ETreasureRoundPhase::HunterSearching);
        if (GS->RaceRoundIndex == 1)
        {
            auto* ShovingPawn = World->SpawnActor<ATreasureSketchCharacter>();
            auto* TargetPawn = World->SpawnActor<ATreasureSketchCharacter>();
            Controllers[1]->Possess(ShovingPawn);
            Controllers[2]->Possess(TargetPawn);
            ShovingPawn->SetActorLocation(FVector(0.f, 0.f, -1000.f));
            GM->RecoverFallenPlayers();
            TestTrue(TEXT("Fallen explorer returns to the island"), ShovingPawn->GetActorLocation().Z > 0.f);
            ShovingPawn->SetActorLocation(FVector(0.f, 0.f, 50.f));
            ShovingPawn->Jump();
            ShovingPawn->CheckJumpInput(0.f);
            TestEqual(TEXT("Low-water jump has extra lift"), ShovingPawn->GetCharacterMovement()->JumpZVelocity, 900.f);
            ShovingPawn->StopJumping();
            ShovingPawn->SetActorLocation(FVector(0.f, 0.f, 500.f));
            ShovingPawn->Jump();
            ShovingPawn->CheckJumpInput(0.f);
            TestEqual(TEXT("Land jump keeps its original lift"), ShovingPawn->GetCharacterMovement()->JumpZVelocity, 620.f);
            ShovingPawn->StopJumping();
            ShovingPawn->SetActorLocation(FVector(0.f, 0.f, 5000.f));
            TargetPawn->SetActorLocation(FVector(150.f, 0.f, 5000.f));
            Controllers[1]->SetControlRotation(FRotator::ZeroRotator);
            TestTrue(TEXT("Nearby explorer can shove the opponent"), GM->TryShove(Controllers[1]));
            TestTrue(TEXT("Shove visibly winds up"), ShovingPawn->IsShoveWindingUp());
            TestFalse(TEXT("Shove cooldown blocks repeat use"), GM->TryShove(Controllers[1]));
            TestTrue(TEXT("Shove attempt starts cooldown"), Players[1]->NextShoveServerTime > GS->GetServerWorldTimeSeconds());
            GM->ResolveShove(Controllers[1], ShovingPawn, GS->RoundSerial);
            TestFalse(TEXT("Windup ends after shove"), ShovingPawn->IsShoveWindingUp());
            const float ProtectedUntil = Players[2]->ShoveProtectedUntilServerTime;
            TestTrue(TEXT("Hit grants brief protection"), ProtectedUntil > GS->GetServerWorldTimeSeconds());

            Players[1]->NextShoveServerTime = 0.f;
            TestTrue(TEXT("Protected opponent still consumes an attempted shove"), GM->TryShove(Controllers[1]));
            GM->ResolveShove(Controllers[1], ShovingPawn, GS->RoundSerial);
            TestEqual(TEXT("Protected opponent cannot be pushed again"), Players[2]->ShoveProtectedUntilServerTime, ProtectedUntil);
            TestTrue(TEXT("Blocked shove keeps its cooldown"), Players[1]->NextShoveServerTime > GS->GetServerWorldTimeSeconds());

            TargetPawn->SetActorLocation(FVector(1000.f, 0.f, 5000.f));
            Players[1]->NextShoveServerTime = 0.f;
            TestTrue(TEXT("Empty shove is accepted"), GM->TryShove(Controllers[1]));
            GM->ResolveShove(Controllers[1], ShovingPawn, GS->RoundSerial);
            TestTrue(TEXT("Empty shove also uses cooldown"), Players[1]->NextShoveServerTime > GS->GetServerWorldTimeSeconds());
            Controllers[1]->ClientDigResult_Implementation(false, 1, true);
            TestEqual(TEXT("Dig feedback uses a visual band"), Controllers[1]->GetDigFeedbackBand(), 1);
            TestTrue(TEXT("Wrong dig no longer prints a distance message"), Controllers[1]->GetStatusMessage().IsEmpty());
            TestEqual(TEXT("Six meters is the nearest band"), TreasureRules::DigFeedbackBand(600.f, 1.f), 0);
            TestEqual(TEXT("Thresholds follow map length scale"), TreasureRules::DigFeedbackBand(1200.f, 4.f), 0);
        }
        if (GS->RaceRoundIndex == 1)
        {
            float MissDistance = 0.f;
            bool bMissAttempted = false;
            TestFalse(TEXT("Near miss does not end the race"), GM->TryDig(Players[2],
                GM->GetTreasureLocation() + FVector(600.f, 0.f, 0.f), MissDistance, bMissAttempted));
            TestTrue(TEXT("Near miss counts toward proximity score"), bMissAttempted);
        }
        float Distance = 0.f;
        bool bAttempted = false;
        TestTrue(TEXT("Finder wins at treasure"), GM->TryDig(Players[FinderIndex],
            GM->GetTreasureLocation(), Distance, bAttempted));
        TestTrue(TEXT("Dig was accepted"), bAttempted);
        TestEqual(TEXT("Winner is named"), GS->RaceRoundWinner, Players[FinderIndex]->GetPlayerName());
    };

    FinishRound(1);
    TestTrue(TEXT("Fast finder gets a large speed bonus"), Players[1]->RaceLastRoundPoints >= 8);
    TestTrue(TEXT("Fast map discovery rewards the mapmaker"), Players[0]->RaceLastRoundPoints >= 3);
    TestEqual(TEXT("Near-miss explorer earns four points"), Players[2]->RaceLastRoundPoints, 4);
    TestEqual(TEXT("Round score is added to total"), Players[2]->RacePoints, 4);
    GM->StartNewRound();
    TestEqual(TEXT("Second round index"), GS->RaceRoundIndex, 2);
    TestEqual(TEXT("Mapmaker rotates to second player"), Players[1]->PlayerRole, ETreasurePlayerRole::Scout);
    TestTrue(TEXT("Winner clears on next round"), GS->RaceRoundWinner.IsEmpty());

    FinishRound(2);
    GM->StartNewRound();
    TestEqual(TEXT("Third round index"), GS->RaceRoundIndex, 3);
    TestEqual(TEXT("Mapmaker rotates to third player"), Players[2]->PlayerRole, ETreasurePlayerRole::Scout);
    FinishRound(0);
    for (const ATreasureSketchPlayerState* PS : Players)
        TestTrue(TEXT("Each player earned race points across three roles"), PS->RacePoints > 0);
    TestEqual(TEXT("Each player found treasure once"), Players[0]->RaceFinds + Players[1]->RaceFinds + Players[2]->RaceFinds, 3);

    GM->StartNewRound();
    TestEqual(TEXT("New series starts at first round"), GS->RaceRoundIndex, 1);
    TestEqual(TEXT("Mapmaker rotates back to first player"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    for (const ATreasureSketchPlayerState* PS : Players)
        TestEqual(TEXT("New series clears scores"), PS->RacePoints, 0);

    GM->SubmitPlayerSketch(Players[0], {});
    float MissDistance = 0.f;
    bool bMissAttempted = false;
    TestFalse(TEXT("Far miss is valid in a new series"), GM->TryDig(Players[1],
        GM->GetTreasureLocation() + FVector(12000.f, 0.f, 0.f), MissDistance, bMissAttempted));
    GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() - 1.f;
    GM->FinishIfTimeExpired();
    TestEqual(TEXT("Far misses lose one point on timeout"), Players[1]->RaceLastRoundPoints, -1);
    TestTrue(TEXT("Timeout has no race winner"), GS->RaceRoundWinner.IsEmpty());

    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
