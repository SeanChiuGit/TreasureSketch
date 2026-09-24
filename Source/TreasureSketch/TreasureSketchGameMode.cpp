#include "TreasureSketchGameMode.h"

#include "ProceduralIsland.h"
#include "TreasureMarker.h"
#include "TreasureSketchCharacter.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchHUD.h"
#include "TreasureSketchPlayerController.h"
#include "TreasureSketchPlayerState.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
    if (Marker) Marker->SetActorHiddenInGame(true);
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_TIMEOUT Phase=%s"),
        bScoutTimedOut ? TEXT("ScoutDrawing") : TEXT("HunterSearching"));
    return true;
}

void ATreasureSketchGameMode::BuildRound()
{
    FRandomStream Stream(FDateTime::Now().GetTicks());
    int32 RequestedSeed = 0;
    IslandSeed = FParse::Value(FCommandLine::Get(), TEXT("IslandSeed="), RequestedSeed) && RequestedSeed != 0
        ? FMath::Abs(RequestedSeed)
        : Stream.RandRange(1000, 999999);
    Stream.Initialize(IslandSeed ^ 0x35D1A7);
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
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_ROUND_READY Seed=%d Shape=%s Treasure=%s"),
        IslandSeed, *Island->GetShapeName(), *TreasureLocation.ToCompactString());
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

    int32 AssignedPlayers = 0;
    for (APlayerState* PS : GameState->PlayerArray)
    {
        if (const ATreasureSketchPlayerState* TreasurePS = Cast<ATreasureSketchPlayerState>(PS))
            if (TreasurePS != NewPS && TreasurePS->PlayerRole != ETreasurePlayerRole::Unassigned) ++AssignedPlayers;
    }
    NewPS->PlayerRole = AssignedPlayers == 0 ? ETreasurePlayerRole::Scout : ETreasurePlayerRole::Hunter;
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_PLAYER_JOIN Role=%s"),
        NewPS->PlayerRole == ETreasurePlayerRole::Scout ? TEXT("Scout") : TEXT("Hunter"));
}

void ATreasureSketchGameMode::HandoffToHunter(const TArray<FSketchStroke>& SubmittedStrokes)
{
    if (FinishIfTimeExpired()) return;
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    GS->Phase = ETreasureRoundPhase::HunterSearching;
    GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + PhaseDurationSeconds;
    if (Marker) Marker->SetActorHiddenInGame(true);
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

void ATreasureSketchGameMode::StartNewRound()
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS) return;

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
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_REPLAY New round started with seed=%d"), IslandSeed);
}
