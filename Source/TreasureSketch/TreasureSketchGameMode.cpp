#include "TreasureSketchGameMode.h"

#include "ProceduralIsland.h"
#include "TreasureMarker.h"
#include "TreasureSketchCharacter.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchHUD.h"
#include "TreasureSketchPlayerController.h"
#include "TreasureSketchPlayerState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ATreasureSketchGameMode::ATreasureSketchGameMode()
{
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
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + 60.f;
        GS->bGameStarted = GetNetMode() == NM_Standalone;
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_ROUND_READY Seed=%d Treasure=%s"), IslandSeed, *TreasureLocation.ToCompactString());
}

void ATreasureSketchGameMode::StartHostedRound()
{
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        if (GS->PlayerArray.Num() < 2)
        {
            UE_LOG(LogTemp, Warning, TEXT("TREASURE_ONLINE_START waiting for second player"));
            return;
        }
        GS->bGameStarted = true;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + 60.f;
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
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    GS->Phase = ETreasureRoundPhase::HunterSearching;
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
    UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
}
