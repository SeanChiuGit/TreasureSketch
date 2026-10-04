#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "IpNetDriver.h"
#include "TimerManager.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchPlayerController.h"
#include "../TreasureSketchPlayerState.h"
#include "../TreasureSketchCharacter.h"
#include "../ProceduralIsland.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHideAndSeekFlowTest, "TreasureSketch.RoomSettings.HideAndSeek",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHideAndSeekFlowTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->URL.AddOption(TEXT("listen"));
    auto* Driver = NewObject<UIpNetDriver>(World);
    Driver->SetWorld(World);
    World->SetNetDriver(Driver);
    auto* GM = World->SpawnActor<ATreasureSketchGameMode>();
    auto* GS = World->SpawnActor<ATreasureSketchGameState>();
    World->SetGameState(GS);
    GM->GameState = GS;
    TArray<ATreasureSketchPlayerState*> Players;
    TArray<ATreasureSketchPlayerController*> Controllers;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* PS = World->SpawnActor<ATreasureSketchPlayerState>();
        auto* PC = World->SpawnActor<ATreasureSketchPlayerController>();
        PS->SetPlayerId(Index + 1);
        PC->SetPlayerState(PS);
        PS->SetOwner(PC);
        GS->PlayerArray.AddUnique(PS);
        Players.Add(PS);
        Controllers.Add(PC);
    }
    TestTrue(TEXT("Hide and seek can be selected"), GM->SelectRoomMode(ETreasureRoomMode::HideAndSeek));
    TestEqual(TEXT("Hider default speed is two times"), GS->HiderSpeedMultiplier, 2.f);
    TestEqual(TEXT("Catcher default speed is two times"), GS->CatcherSpeedMultiplier, 2.f);
    GS->PlayerArray.Remove(Players[3]);
    GS->PlayerArray.Remove(Players[2]);
    GS->PlayerArray.Remove(Players[1]);
    GM->StartHostedRound();
    TestFalse(TEXT("One player cannot start"), GS->bGameStarted);
    GS->PlayerArray.Add(Players[1]);
    GS->bCanyonInMapPool = false;
    GS->bForestInMapPool = false;
    GS->RoomMapScale = 0.5f;
    TestTrue(TEXT("Host can enter a precise hider speed"), GM->SetRoleMovementSpeed(TEXT("HiderSpeed"), 1.05f));
    GM->AdjustRoomSetting(TEXT("HiderSpeed"), 1);
    TestTrue(TEXT("Hider speed button increments by 0.05"), FMath::IsNearlyEqual(GS->HiderSpeedMultiplier, 1.10f));
    GM->AdjustRoomSetting(TEXT("HiderSpeed"), -1);
    TestTrue(TEXT("Host can enter an independent catcher speed"), GM->SetRoleMovementSpeed(TEXT("CatcherSpeed"), 1.10f));
    TestFalse(TEXT("Out of range speed is rejected"), GM->SetRoleMovementSpeed(TEXT("HiderSpeed"), 0.1f));
    TestFalse(TEXT("Unknown speed setting is rejected"), GM->SetRoleMovementSpeed(TEXT("OtherSpeed"), 1.f));
    GM->StartHostedRound();
    TestFalse(TEXT("Speed input locks when play starts"), GM->SetRoleMovementSpeed(TEXT("HiderSpeed"), 2.f));
    GM->AdjustRoomSetting(TEXT("CatcherSpeed"), 1);
    TestTrue(TEXT("Speed buttons also lock during play"), FMath::IsNearlyEqual(GS->CatcherSpeedMultiplier, 1.10f));
    TestTrue(TEXT("Two players start"), GS->bGameStarted);
    TestEqual(TEXT("Drawing is skipped"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    TestEqual(TEXT("Three treasures are generated"), GS->HideTreasures.Num(), 3);
    for (int32 Index = 0; Index < GS->HideTreasures.Num(); ++Index)
        for (int32 Other = 0; Other < Index; ++Other)
            TestTrue(TEXT("Treasure dig ranges do not overlap"),
                FVector::Dist2D(GS->HideTreasures[Index], GS->HideTreasures[Other]) > 850.f);
    // Exercise different placement choices on the same terrain, rather than
    // only checking one lucky random sample. Failure must never return duplicates.
    for (int32 PlacementSeed = 0; PlacementSeed < 20; ++PlacementSeed)
    {
        FRandomStream Stream(PlacementSeed);
        const TArray<FVector> Points = GM->Island->FindSeparatedTreasurePoints(Stream, 3, 950.f);
        TestEqual(TEXT("Every placement seed finds three treasures"), Points.Num(), 3);
        for (int32 Index = 0; Index < Points.Num(); ++Index)
            for (int32 Other = 0; Other < Index; ++Other)
                TestTrue(TEXT("All placements are separated"), FVector::Dist2D(Points[Index], Points[Other]) > 950.f);
    }
    FRandomStream ImpossibleStream(0);
    TestTrue(TEXT("Impossible spacing rejects placement instead of stacking treasures"),
        GM->Island->FindSeparatedTreasurePoints(ImpossibleStream, 3, 10000000.f).IsEmpty());
    TestFalse(TEXT("Mode is locked during play"), GM->SelectRoomMode(ETreasureRoomMode::OneMapmaker));
    auto* Catcher = World->SpawnActor<ATreasureSketchCharacter>();
    auto* Hider = World->SpawnActor<ATreasureSketchCharacter>();
    Controllers[0]->Possess(Catcher);
    Controllers[1]->Possess(Hider);
    Controllers[1]->SetAsLocalPlayerController();
    TestTrue(TEXT("Opening preparation lasts ten seconds"), GS->IsHidePreparation()
        && FMath::IsNearlyEqual(GS->HidePreparationEndServerTime - GS->GetServerWorldTimeSeconds(), 10.f));
    TestTrue(TEXT("Opening map is only shown to the catcher"), Controllers[0]->IsCatcherStudyingMap()
        && !Controllers[1]->IsCatcherStudyingMap());
    TestEqual(TEXT("Preparation preserves full search time"), GS->GetSecondsRemaining(), GS->SearchingDurationSeconds);
    TestFalse(TEXT("Catcher cannot attack during preparation"), GM->TryShove(Controllers[0]));
    Catcher->Tick(0.f);
    TestEqual(TEXT("Catcher cannot move during preparation"), Catcher->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
    Controllers[1]->UpdateHideTreasureMarkers();
    TestTrue(TEXT("Treasure X markers are hidden during preparation"), Controllers[1]->VisibleHideTreasures.IsEmpty());
    GS->HidePreparationEndServerTime = GS->GetServerWorldTimeSeconds();
    Catcher->Tick(0.f);
    TestEqual(TEXT("Catcher regains movement after preparation"), Catcher->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
    TestFalse(TEXT("Opening map closes after ten seconds"), Controllers[0]->IsCatcherStudyingMap());
    Controllers[1]->UpdateHideTreasureMarkers();
    TestEqual(TEXT("Three X markers appear after preparation"), Controllers[1]->VisibleHideTreasures.Num(), 3);
    GS->ApplyMovementSpeed();
    TestTrue(TEXT("Catcher uses its own speed"), FMath::IsNearlyEqual(Catcher->GetCharacterMovement()->MaxWalkSpeed, 572.f));
    TestTrue(TEXT("Hider uses its own precise speed"), FMath::IsNearlyEqual(Hider->GetCharacterMovement()->MaxWalkSpeed, 546.f));
    Hider->SetWaterSlowed(true);
    TestTrue(FString::Printf(TEXT("Water slowdown retains the hider speed baseline (actual %.6f)"),
        Hider->GetCharacterMovement()->MaxWalkSpeed), FMath::IsNearlyEqual(Hider->GetCharacterMovement()->MaxWalkSpeed, 409.5f, 0.001f));
    Hider->SetWaterSlowed(false);
    TestTrue(TEXT("Leaving water restores the hider speed"), FMath::IsNearlyEqual(Hider->GetCharacterMovement()->MaxWalkSpeed, 546.f));
    GS->RoomMode = ETreasureRoomMode::OneMapmaker;
    GS->MovementSpeedMultiplier = 1.25f;
    GS->ApplyMovementSpeed();
    TestTrue(TEXT("Other modes keep their shared speed"), FMath::IsNearlyEqual(Hider->GetCharacterMovement()->MaxWalkSpeed, 650.f)
        && FMath::IsNearlyEqual(Catcher->GetCharacterMovement()->MaxWalkSpeed, 650.f));
    GS->RoomMode = ETreasureRoomMode::HideAndSeek;
    GS->MovementSpeedMultiplier = 1.f;
    GS->ApplyMovementSpeed();
    Catcher->SetActorLocation(FVector(0.f, 0.f, 5000.f));
    Hider->SetActorLocation(FVector(150.f, 0.f, 5000.f));
    Hider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    FPropDisguise Form;
    Form.Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    Hider->SetPropDisguise(Form);
    TestTrue(TEXT("Prop disguise keeps the hider speed"), FMath::IsNearlyEqual(Hider->GetCharacterMovement()->MaxWalkSpeed, 546.f));
    TestFalse(TEXT("Catcher cannot dig"), GM->StartHeldDig(Controllers[0]));
    TestFalse(TEXT("Hider cannot catch"), GM->TryShove(Controllers[1]));
    TestTrue(TEXT("Hider begins held dig"), GM->StartHeldDig(Controllers[1]));
    Players[1]->DigStartedServerTime = GS->GetServerWorldTimeSeconds() - 2.f;
    GM->UpdateHeldDigs();
    TestTrue(TEXT("Two seconds is insufficient"), Players[1]->bDigging);
    GM->CancelHeldDig(Players[1]);
    TestFalse(TEXT("Release cancels digging"), Players[1]->bDigging);
    TestEqual(TEXT("Canceled digging has no cooldown"), Players[1]->NextDigServerTime, 0.f);
    GS->DigCooldownSeconds = 0;
    Hider->SetActorLocation(GS->HideTreasures[0]);
    TestTrue(TEXT("Dig can restart"), GM->StartHeldDig(Controllers[1]));
    Players[1]->DigStartedServerTime = GS->GetServerWorldTimeSeconds() - 3.1f;
    GM->UpdateHeldDigs();
    TestEqual(TEXT("Completed hold collects one treasure"), GS->HideTreasureCount, 1);
    TestTrue(TEXT("Treasure collection does not end disguise"), Hider->IsPropDisguised());
    TestEqual(TEXT("Treasure does not end the round"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    float Distance;
    bool bAttempted;
    TestFalse(TEXT("Same treasure cannot be collected twice"),
        GM->TryDig(Players[1], GS->HideTreasures[0], Distance, bAttempted));
    TestTrue(TEXT("Second treasure is collectible"),
        GM->TryDig(Players[1], GS->HideTreasures[1], Distance, bAttempted));
    TestEqual(TEXT("Two treasures still require survival"), GS->Phase, ETreasureRoundPhase::HunterSearching);

    Catcher->SetActorLocation(FVector(0.f, 0.f, 5000.f));
    Hider->SetActorLocation(FVector(150.f, 0.f, 5000.f));
    Controllers[0]->SetControlRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Catcher can attempt capture"), GM->TryShove(Controllers[0]));
    GM->ResolveShove(Controllers[0], Catcher, GS->RoundSerial);
    TestTrue(TEXT("Capture wins even after two treasures"), GS->bHideCaught);
    TestEqual(TEXT("Capture ends round"), GS->Phase, ETreasureRoundPhase::HunterTimedOut);

    GS->RoomMapScale = 1.f;
    GM->StartNewRound(true);
    TestEqual(TEXT("Roles can swap"), Players[0]->PlayerRole, ETreasurePlayerRole::Hunter);
    Catcher->Tick(0.f);
    Hider->Tick(0.f);
    TestTrue(TEXT("Role swap applies hider speed to the new hider"), FMath::IsNearlyEqual(Catcher->GetCharacterMovement()->MaxWalkSpeed, 546.f));
    TestTrue(TEXT("Role swap applies catcher speed to the new catcher"), FMath::IsNearlyEqual(Hider->GetCharacterMovement()->MaxWalkSpeed, 572.f));
    TestEqual(TEXT("Replay skips drawing"), GS->Phase, ETreasureRoundPhase::HunterSearching);
    TestEqual(TEXT("Replay resets treasure count"), GS->HideTreasureCount, 0);
    TestFalse(TEXT("New round removes old prop disguise"), Hider->IsPropDisguised());
    TestFalse(TEXT("Replay clears capture"), GS->bHideCaught);
    TestEqual(TEXT("Replay restores three treasures"), GS->HideTreasures.Num(), 3);

    for (int32 Count = 0; Count <= 3; ++Count)
    {
        GS->Phase = ETreasureRoundPhase::HunterSearching;
        GS->HideTreasureCount = Count;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() - 1.f;
        TestTrue(TEXT("Timeout completes round"), GM->FinishIfTimeExpired());
        TestFalse(TEXT("Timeout is not a capture"), GS->bHideCaught);
        TestEqual(TEXT("Two or more treasures win; zero loses and one draws"), GS->Phase,
            Count >= 2 ? ETreasureRoundPhase::Won : ETreasureRoundPhase::HunterTimedOut);
        TestFalse(TEXT("Results cannot be overwritten by late capture"), GM->TryShove(Controllers[1]));
    }
    GM->ReturnToSetup();
    TestFalse(TEXT("Return to lobby stops play"), GS->bGameStarted);
    TestEqual(TEXT("Lobby preserves mode"), GS->RoomMode, ETreasureRoomMode::HideAndSeek);
    TestTrue(TEXT("Lobby clears public treasures"), GS->HideTreasures.IsEmpty());
    GS->PlayerArray.Add(Players[2]);
    GM->NormalizeRoomRoles(nullptr, Players[0]);
    GM->StartHostedRound();
    TestTrue(TEXT("Three players can start"), GS->bGameStarted);
    TestEqual(TEXT("Two hiders generate six treasures"), GS->HideTreasures.Num(), 6);
    GM->ReturnToSetup();
    GS->PlayerArray.Add(Players[3]);
    GM->NormalizeRoomRoles(nullptr, Players[0]);
    GM->StartHostedRound();
    TestTrue(TEXT("Four players can start"), GS->bGameStarted);
    TestEqual(TEXT("Three hiders generate nine treasures"), GS->HideTreasures.Num(), 9);
    TestEqual(TEXT("Exactly three hiders are active"), GS->GetRemainingHiders(), 3);
    for (int32 Index = 0; Index < GS->HideTreasures.Num(); ++Index)
        for (int32 Other = 0; Other < Index; ++Other)
            TestTrue(TEXT("All nine treasures retain nonoverlapping dig ranges"),
                FVector::Dist2D(GS->HideTreasures[Index], GS->HideTreasures[Other]) > 950.f);
    GS->HidePreparationEndServerTime = GS->GetServerWorldTimeSeconds();
    TArray<ATreasureSketchCharacter*> Hiders = { Hider };
    for (int32 Index = 2; Index < 4; ++Index)
    {
        auto* Extra = World->SpawnActor<ATreasureSketchCharacter>();
        Controllers[Index]->Possess(Extra);
        Hiders.Add(Extra);
    }
    Catcher->SetActorLocation(FVector(0.f, 0.f, 5000.f));
    Controllers[0]->SetControlRotation(FRotator::ZeroRotator);
    for (auto* Active : Hiders) Active->SetActorLocation(FVector(-2000.f, 0.f, 5000.f));
    GS->DigCooldownSeconds = 0;
    TestTrue(TEXT("First hider contributes to shared treasure count"),
        GM->TryDig(Players[1], GS->HideTreasures[0], Distance, bAttempted));
    TestTrue(TEXT("Another hider contributes to the same treasure count"),
        GM->TryDig(Players[2], GS->HideTreasures[1], Distance, bAttempted));
    TestEqual(TEXT("Team collected treasures are shared"), GS->HideTreasureCount, 2);
    for (int32 Index = 0; Index < Hiders.Num(); ++Index)
    {
        Hiders[Index]->SetActorLocation(FVector(150.f, 0.f, 5000.f));
        GM->ResolveShove(Controllers[0], Catcher, GS->RoundSerial);
        TestTrue(TEXT("Caught hider is marked eliminated"), Players[Index + 1]->bHideEliminated);
        TestFalse(TEXT("Caught hider cannot dig"), GM->StartHeldDig(Controllers[Index + 1]));
        TestFalse(TEXT("Caught hider cannot collect treasure via a late request"),
            GM->TryDig(Players[Index + 1], GS->HideTreasures[2], Distance, bAttempted));
        TestEqual(TEXT("Each capture removes only one hider"), GS->GetRemainingHiders(), 2 - Index);
        TestEqual(TEXT("Capture ends round only after all hiders are caught"), GS->Phase,
            Index == 2 ? ETreasureRoundPhase::HunterTimedOut : ETreasureRoundPhase::HunterSearching);
    }
    TestTrue(TEXT("Catching all three wins despite team treasure progress"), GS->bHideCaught);
    GM->StartNewRound(false);
    TestEqual(TEXT("Replay resets all three eliminated hiders"), GS->GetRemainingHiders(), 3);
    TestEqual(TEXT("Replay regenerates nine treasures"), GS->HideTreasures.Num(), 9);
    TestTrue(TEXT("Replay restarts the ten-second map phase"), GS->IsHidePreparation());
    for (int32 Count = 0; Count <= 9; ++Count)
    {
        GS->Phase = ETreasureRoundPhase::HunterSearching;
        GS->HideTreasureCount = Count;
        GS->HidePreparationEndServerTime = 0.f;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() - 1.f;
        GM->FinishIfTimeExpired();
        TestEqual(TEXT("Three-hider team needs six treasures to win"), GS->Phase,
            Count >= 6 ? ETreasureRoundPhase::Won : ETreasureRoundPhase::HunterTimedOut);
        TestEqual(TEXT("Three-hider draw threshold remains three even after elimination"), GS->GetHideDrawThreshold(), 3);
    }
    GM->ReturnToSetup();
    TestTrue(TEXT("Existing mode remains selectable"), GM->SelectRoomMode(ETreasureRoomMode::OneMapmaker));
    World->GetTimerManager().ClearAllTimersForObject(GM);
    World->SetNetDriver(nullptr);
    Driver->SetWorld(nullptr);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
