#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IpNetDriver.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchPlayerController.h"
#include "../TreasureSketchPlayerState.h"
#include "../TreasureSketchCharacter.h"

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
            TestFalse(TEXT("Race dig hint omits precise meters"), Controllers[1]->GetStatusMessage().Contains(TEXT("米")));
            Controllers[1]->ClientDigResult_Implementation(false, 123, false);
            TestTrue(TEXT("Cooperative dig still reports meters"), Controllers[1]->GetStatusMessage().Contains(TEXT("123 米")));
        }
        float Distance = 0.f;
        bool bAttempted = false;
        TestTrue(TEXT("Finder wins at treasure"), GM->TryDig(Players[FinderIndex],
            GM->GetTreasureLocation(), Distance, bAttempted));
        TestTrue(TEXT("Dig was accepted"), bAttempted);
        TestEqual(TEXT("Winner is named"), GS->RaceRoundWinner, Players[FinderIndex]->GetPlayerName());
    };

    FinishRound(1);
    TestEqual(TEXT("Finder earns two points"), Players[1]->RacePoints, 2);
    TestEqual(TEXT("Mapmaker earns one point"), Players[0]->RacePoints, 1);
    TestEqual(TEXT("Other explorer earns no points"), Players[2]->RacePoints, 0);
    GM->StartNewRound();
    TestEqual(TEXT("Second round index"), GS->RaceRoundIndex, 2);
    TestEqual(TEXT("Mapmaker rotates to second player"), Players[1]->PlayerRole, ETreasurePlayerRole::Scout);
    TestTrue(TEXT("Winner clears on next round"), GS->RaceRoundWinner.IsEmpty());

    FinishRound(2);
    GM->StartNewRound();
    TestEqual(TEXT("Third round index"), GS->RaceRoundIndex, 3);
    TestEqual(TEXT("Mapmaker rotates to third player"), Players[2]->PlayerRole, ETreasurePlayerRole::Scout);
    FinishRound(0);
    TestEqual(TEXT("Final score for first player"), Players[0]->RacePoints, 3);
    TestEqual(TEXT("Final score for second player"), Players[1]->RacePoints, 3);
    TestEqual(TEXT("Final score for third player"), Players[2]->RacePoints, 3);
    TestEqual(TEXT("Each player found treasure once"), Players[0]->RaceFinds + Players[1]->RaceFinds + Players[2]->RaceFinds, 3);

    GM->StartNewRound();
    TestEqual(TEXT("New series starts at first round"), GS->RaceRoundIndex, 1);
    TestEqual(TEXT("Mapmaker rotates back to first player"), Players[0]->PlayerRole, ETreasurePlayerRole::Scout);
    for (const ATreasureSketchPlayerState* PS : Players)
        TestEqual(TEXT("New series clears scores"), PS->RacePoints, 0);

    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
