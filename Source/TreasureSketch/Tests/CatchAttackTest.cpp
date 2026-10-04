#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "../CatchAttack.h"
#include "../TreasureSketchGameMode.h"
#include "../TreasureSketchGameState.h"
#include "../TreasureSketchCharacter.h"
#include "../TreasureSketchPlayerController.h"
#include "../TreasureSketchPlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchAttackTest, "TreasureSketch.RoomSettings.CatchAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatchAttackTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> AllowScriptExecution(GAllowActorScriptExecutionInEditor, true);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* GM = World->SpawnActor<ATreasureSketchGameMode>();
    auto* GS = World->SpawnActor<ATreasureSketchGameState>();
    World->SetGameState(GS);
    GM->GameState = GS;
    GS->RoomMode = ETreasureRoomMode::HideAndSeek;
    TestEqual(TEXT("Default capture reach is 4.5 meters"), GS->CatchRangeMeters, 4.5f);
    TestTrue(TEXT("Host can enter an exact capture reach"), GM->SetCatchRange(5.25f));
    GM->AdjustRoomSetting(TEXT("CatchRange"), 1);
    TestEqual(TEXT("Capture reach increments by a quarter meter"), GS->CatchRangeMeters, 5.5f);
    TestFalse(TEXT("Non-finite capture reach is rejected"), GM->SetCatchRange(std::numeric_limits<float>::quiet_NaN()));
    TestFalse(TEXT("Out-of-bounds capture reach is rejected"), GM->SetCatchRange(10.1f));
    GS->RoomMode = ETreasureRoomMode::TeamVersus;
    TestFalse(TEXT("Capture setting only affects hide-and-seek"), GM->SetCatchRange(4.5f));
    GS->RoomMode = ETreasureRoomMode::HideAndSeek;
    GM->SetCatchRange(4.5f);
    GS->bGameStarted = true;
    TestFalse(TEXT("Capture setting is locked during a round"), GM->SetCatchRange(6.f));
    auto* Catcher = World->SpawnActor<ATreasureSketchCharacter>();
    auto* Hider = World->SpawnActor<ATreasureSketchCharacter>();
    auto* CatchPC = World->SpawnActor<ATreasureSketchPlayerController>();
    auto* HidePC = World->SpawnActor<ATreasureSketchPlayerController>();
    auto* CatchPS = World->SpawnActor<ATreasureSketchPlayerState>();
    auto* HidePS = World->SpawnActor<ATreasureSketchPlayerState>();
    CatchPC->SetPlayerState(CatchPS); CatchPS->SetOwner(CatchPC); CatchPC->Possess(Catcher);
    HidePC->SetPlayerState(HidePS); HidePS->SetOwner(HidePC); HidePC->Possess(Hider);
    CatchPS->PlayerRole = ETreasurePlayerRole::Scout;
    HidePS->PlayerRole = ETreasurePlayerRole::Hunter;
    GS->PlayerArray.AddUnique(CatchPS); GS->PlayerArray.AddUnique(HidePS);
    Catcher->SetActorLocation(FVector::ZeroVector);
    CatchPC->SetControlRotation(FRotator::ZeroRotator);
    auto Attack = [&](const FVector& Location)
    {
        GS->Phase = ETreasureRoundPhase::HunterSearching;
        GS->RoundEndServerTime = 300.f;
        HidePS->bHideEliminated = false;
        Hider->SetActorLocation(Location);
        GM->ResolveShove(CatchPC, Catcher, GS->RoundSerial);
        return HidePS->bHideEliminated;
    };
    TestTrue(TEXT("Running and jumping hider is caught beyond the old reach and height"), Attack(FVector(350.f, 0.f, 250.f)));
    TestFalse(TEXT("Beyond the displayed radius misses"), Attack(FVector(451.f, 0.f, 0.f)));
    TestFalse(TEXT("Beyond the displayed vertical range misses"), Attack(FVector(300.f, 0.f, 301.f)));
    TestFalse(TEXT("Hider behind catcher misses"), Attack(FVector(-300.f, 0.f, 0.f)));
    TestTrue(TEXT("Wider front sector catches a side-jumping target"),
        Attack(FRotator(0.f, 75.f, 0.f).Vector() * 400.f + FVector(0.f, 0.f, 200.f)));
    GS->bGameStarted = false; GM->SetCatchRange(6.f); GS->bGameStarted = true;
    TestTrue(TEXT("Room reach setting changes authoritative capture"), Attack(FVector(550.f, 0.f, 200.f)));
    GS->Phase = ETreasureRoundPhase::HunterSearching;
    HidePS->bHideEliminated = false;
    Hider->SetActorLocation(FVector(350.f, 0.f, 250.f));
    CatchPS->NextShoveServerTime = 0.f;
    World->GetTimerManager().Tick(0.f); // Activate newly scheduled timers in this isolated world.
    TestTrue(TEXT("Capture attack starts"), GM->TryShove(CatchPC));
    TGuardValue<uint64> TimerFrame(GFrameCounter, GFrameCounter + 1);
    World->GetTimerManager().Tick(0.14f);
    TestFalse(TEXT("Capture does not resolve before its windup"), HidePS->bHideEliminated);
    ++GFrameCounter;
    World->GetTimerManager().Tick(0.02f);
    TestTrue(TEXT("Capture resolves after the shorter windup"), HidePS->bHideEliminated);
    World->GetTimerManager().ClearAllTimersForObject(GM);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
