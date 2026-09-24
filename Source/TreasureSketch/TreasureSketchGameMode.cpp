#include "TreasureSketchGameMode.h"

#include "ProceduralIsland.h"
#include "TreasureMarker.h"
#include "TreasureSketchCharacter.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchHUD.h"
#include "TreasureSketchPlayerController.h"
#include "TreasureSketchPlayerState.h"
#include "Engine/World.h"

namespace
{
constexpr float PhaseDurationSeconds = 60.f;
}

ATreasureSketchGameMode::ATreasureSketchGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = ATreasureSketchCharacter::StaticClass();
    PlayerControllerClass = ATreasureSketchPlayerController::StaticClass();
    PlayerStateClass = ATreasureSketchPlayerState::StaticClass();
    GameStateClass = ATreasureSketchGameState::StaticClass();
    HUDClass = ATreasureSketchHUD::StaticClass();
}

void ATreasureSketchGameMode::BeginPlay()
{
    Super::BeginPlay();
    BuildRound();
}

void ATreasureSketchGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FinishIfTimeExpired();
}

bool ATreasureSketchGameMode::FinishIfTimeExpired()
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted
        || (GS->Phase != ETreasureRoundPhase::ScoutDrawing && GS->Phase != ETreasureRoundPhase::HunterSearching)
        || GS->GetServerWorldTimeSeconds() < GS->RoundEndServerTime)
        return false;

    const bool bScoutTimedOut = GS->Phase == ETreasureRoundPhase::ScoutDrawing;
    GS->Phase = bScoutTimedOut ? ETreasureRoundPhase::ScoutTimedOut : ETreasureRoundPhase::HunterTimedOut;
    HideTreasureFromScout();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_TIMEOUT Phase=%s"),
        bScoutTimedOut ? TEXT("ScoutDrawing") : TEXT("HunterSearching"));
    return true;
}

void ATreasureSketchGameMode::BuildRound()
{
    FRandomStream Stream(FDateTime::Now().GetTicks());
    IslandSeed = Stream.RandRange(1000, 999999);
    FActorSpawnParameters Params;
    Island = GetWorld()->SpawnActorDeferred<AProceduralIsland>(AProceduralIsland::StaticClass(), FTransform::Identity);
    Island->Seed = IslandSeed;
    Island->FinishSpawning(FTransform::Identity);

    TreasureLocation = Island->FindRandomLandPoint(Stream, 170.f) + FVector(0.f, 0.f, 35.f);
    Marker = GetWorld()->SpawnActor<ATreasureMarker>(ATreasureMarker::StaticClass(), TreasureLocation, FRotator::ZeroRotator, Params);
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        GS->IslandSeed = IslandSeed;
        GS->Phase = ETreasureRoundPhase::ScoutDrawing;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + PhaseDurationSeconds;
        GS->bGameStarted = false;
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_ROUND_READY Seed=%d Treasure=%s"), IslandSeed, *TreasureLocation.ToCompactString());
}

void ATreasureSketchGameMode::StartHostedRound()
{
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        if (GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
        if (GS->PlayerArray.Num() < 2)
        {
            UE_LOG(LogTemp, Warning, TEXT("TREASURE_ONLINE_START waiting for second player"));
            return;
        }
        GS->bGameStarted = true;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + PhaseDurationSeconds;
        UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_START players=%d"), GS->PlayerArray.Num());
    }
}

void ATreasureSketchGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    ATreasureSketchPlayerState* NewPS = NewPlayer ? NewPlayer->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
    if (!NewPS) return;

    bool bScoutAssigned = false;
    bool bHunterAssigned = false;
    for (APlayerState* PS : GameState->PlayerArray)
    {
        if (const ATreasureSketchPlayerState* TreasurePS = Cast<ATreasureSketchPlayerState>(PS))
        {
            if (TreasurePS == NewPS) continue;
            bScoutAssigned |= TreasurePS->PlayerRole == ETreasurePlayerRole::Scout;
            bHunterAssigned |= TreasurePS->PlayerRole == ETreasurePlayerRole::Hunter;
        }
    }
    NewPS->PlayerRole = !bScoutAssigned ? ETreasurePlayerRole::Scout
        : !bHunterAssigned ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Unassigned;
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_PLAYER_JOIN Role=%s"),
        NewPS->PlayerRole == ETreasurePlayerRole::Scout ? TEXT("Scout")
        : NewPS->PlayerRole == ETreasurePlayerRole::Hunter ? TEXT("Hunter") : TEXT("Unassigned"));
    RevealTreasureToScout();
}

void ATreasureSketchGameMode::RevealTreasureToScout()
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!Marker || !GS || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;

    bool bLocalScout = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        const ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
        if (!PC || !PS || PS->PlayerRole != ETreasurePlayerRole::Scout) continue;
        if (PC->IsLocalController()) bLocalScout = true;
        else PC->ClientRevealTreasure(TreasureLocation);
    }
    Marker->SetActorHiddenInGame(!bLocalScout);
}

void ATreasureSketchGameMode::HideTreasureFromScout()
{
    if (Marker) Marker->SetActorHiddenInGame(true);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        if (PC && !PC->IsLocalController()) PC->ClientHideTreasure();
    }
}

void ATreasureSketchGameMode::HandoffToHunter(const TArray<FSketchStroke>& SubmittedStrokes)
{
    if (FinishIfTimeExpired()) return;
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    GS->Phase = ETreasureRoundPhase::HunterSearching;
    GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + PhaseDurationSeconds;
    HideTreasureFromScout();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_HANDOFF Hunter active; marker hidden"));

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        const ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
        if (!PC || !PS) continue;
        if (PS->PlayerRole == ETreasurePlayerRole::Hunter)
        {
            PC->ClientReceiveSketch(SubmittedStrokes);
            if (APawn* Pawn = PC->GetPawn())
            {
                const float SpawnZ = Island ? Island->HeightAt(-2800.f, 0.f) + 180.f : 500.f;
                Pawn->SetActorLocation(FVector(-2800.f, 0.f, FMath::Max(SpawnZ, 250.f)), false, nullptr, ETeleportType::ResetPhysics);
            }
        }
    }
}

bool ATreasureSketchGameMode::TryDig(const FVector& WorldLocation, float& OutDistance)
{
    OutDistance = FVector::Dist2D(WorldLocation, TreasureLocation);
    if (FinishIfTimeExpired()) return false;
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->Phase != ETreasureRoundPhase::HunterSearching) return false;
    if (OutDistance <= 425.f)
    {
        GS->Phase = ETreasureRoundPhase::Won;
        if (Marker) Marker->SetActorHiddenInGame(false);
        UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_FOUND Distance=%.1f"), OutDistance);
        return true;
    }
    return false;
}

void ATreasureSketchGameMode::StartNewRound(bool bSwapRoles)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS) return;

    if (bSwapRoles)
    {
        if (!GS->IsRoundOver()) return;
        ATreasureSketchPlayerState* Scout = nullptr;
        ATreasureSketchPlayerState* Hunter = nullptr;
        for (APlayerState* PS : GS->PlayerArray)
        {
            ATreasureSketchPlayerState* TreasurePS = Cast<ATreasureSketchPlayerState>(PS);
            if (!TreasurePS) continue;
            if (TreasurePS->PlayerRole == ETreasurePlayerRole::Scout) Scout = TreasurePS;
            else if (TreasurePS->PlayerRole == ETreasurePlayerRole::Hunter) Hunter = TreasurePS;
        }
        if (GS->PlayerArray.Num() != 2 || !Scout || !Hunter)
        {
            UE_LOG(LogTemp, Warning, TEXT("TREASURE_SKETCH_REPLAY Role swap needs one scout and one hunter"));
            return;
        }
        Scout->PlayerRole = ETreasurePlayerRole::Hunter;
        Hunter->PlayerRole = ETreasurePlayerRole::Scout;
        Scout->ForceNetUpdate();
        Hunter->ForceNetUpdate();
    }

    const bool bWasStarted = GS->bGameStarted;
    if (Marker) Marker->Destroy();
    if (Island) Island->Destroy();
    BuildRound();
    GS->bGameStarted = bWasStarted;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        if (!PC) continue;
        PC->ClientStartNewRound();

        if (APawn* Pawn = PC->GetPawn())
        {
            const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
            const float SpawnY = PS && PS->PlayerRole == ETreasurePlayerRole::Hunter ? 400.f : 0.f;
            const float SpawnZ = Island ? Island->HeightAt(-2800.f, SpawnY) + 180.f : 500.f;
            Pawn->SetActorLocation(FVector(-2800.f, SpawnY, FMath::Max(SpawnZ, 250.f)), false, nullptr, ETeleportType::ResetPhysics);
        }
    }
    RevealTreasureToScout();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_REPLAY New round started with seed=%d roles_swapped=%d"),
        IslandSeed, bSwapRoles ? 1 : 0);
}
