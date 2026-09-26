#include "TreasureSketchGameMode.h"

#include "ProceduralIsland.h"
#include "TreasureSketchCharacter.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchHUD.h"
#include "TreasureSketchPlayerController.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureRules.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
constexpr float PhaseDurationSeconds = 120.f;
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
    RevealTreasureToScout();
}

void ATreasureSketchGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FinishIfTimeExpired();
    SendHunterViewToScout(DeltaSeconds);
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
    int32 RequestedSeed = 0;
    IslandSeed = FParse::Value(FCommandLine::Get(), TEXT("IslandSeed="), RequestedSeed) && RequestedSeed != 0
        ? FMath::Abs(RequestedSeed)
        : Stream.RandRange(1000, 999999);
    Stream.Initialize(IslandSeed ^ 0x35D1A7);
    Island = GetWorld()->SpawnActorDeferred<AProceduralIsland>(AProceduralIsland::StaticClass(), FTransform::Identity);
    Island->Seed = IslandSeed;
    FString RequestedTheme;
    if (FParse::Value(FCommandLine::Get(), TEXT("IslandTheme="), RequestedTheme))
        Island->Theme = RequestedTheme.Equals(TEXT("Ruins"), ESearchCase::IgnoreCase)
            ? EIslandTheme::JungleRuins : EIslandTheme::PirateBeach;
    else
        Island->Theme = AProceduralIsland::SelectThemeFromTable(IslandSeed);
    Island->FinishSpawning(FTransform::Identity);

    TreasureLocation = Island->FindRandomLandPoint(Stream, 170.f) + FVector(0.f, 0.f, 35.f);
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        GS->IslandSeed = IslandSeed;
        ++GS->RoundSerial;
        GS->Phase = ETreasureRoundPhase::ScoutDrawing;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + PhaseDurationSeconds;
        GS->bGameStarted = false;
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_ROUND_READY Seed=%d Theme=%s Shape=%s Treasure=%s"),
        IslandSeed, *Island->GetThemeName(), *Island->GetShapeName(), *TreasureLocation.ToCompactString());
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

void ATreasureSketchGameMode::StartSoloTest(int32 ThemeChoice)
{
    if (!HasAuthority()) return;

    if (Island) Island->Destroy();
    FRandomStream Stream(FDateTime::Now().GetTicks());
    IslandSeed = Stream.RandRange(1000, 999999);
    Stream.Initialize(IslandSeed ^ 0x35D1A7);

    Island = GetWorld()->SpawnActorDeferred<AProceduralIsland>(AProceduralIsland::StaticClass(), FTransform::Identity);
    Island->Seed = IslandSeed;
    Island->Theme = ThemeChoice == 0 ? EIslandTheme::PirateBeach
        : ThemeChoice == 1 ? EIslandTheme::JungleRuins
        : AProceduralIsland::SelectThemeFromTable(IslandSeed, true);
    Island->FinishSpawning(FTransform::Identity);
    TreasureLocation = Island->FindRandomLandPoint(Stream, 170.f) + FVector(0.f, 0.f, 35.f);

    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (GS)
    {
        GS->IslandSeed = IslandSeed;
        ++GS->RoundSerial;
        GS->Phase = ETreasureRoundPhase::HunterSearching;
        GS->bGameStarted = true;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + 3600.f;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
        if (PS) PS->PlayerRole = ETreasurePlayerRole::Hunter;
        if (PC && PC->GetPawn())
            PC->GetPawn()->SetActorLocation(Island->FindSpawnPoint(), false, nullptr, ETeleportType::ResetPhysics);
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SOLO_TEST Seed=%d Theme=%s Shape=%s"),
        IslandSeed, *Island->GetThemeName(), *Island->GetShapeName());
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
    if (!Island || !GS || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        const ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
        if (!PC || !PS || PS->PlayerRole != ETreasurePlayerRole::Scout) continue;
        PC->ClientRevealTreasure(TreasureLocation);
    }
}

void ATreasureSketchGameMode::HideTreasureFromScout()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        if (PC) PC->ClientHideTreasure();
    }
}

void ATreasureSketchGameMode::SendHunterViewToScout(float DeltaSeconds)
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::HunterSearching)
    {
        HunterViewUpdateTime = 0.f;
        return;
    }

    HunterViewUpdateTime += DeltaSeconds;
    if (HunterViewUpdateTime < 0.05f) return;
    HunterViewUpdateTime = 0.f;

    ATreasureSketchPlayerController* ScoutPC = nullptr;
    ATreasureSketchPlayerController* HunterPC = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        const ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
        if (!PS) continue;
        if (PS->PlayerRole == ETreasurePlayerRole::Scout) ScoutPC = PC;
        else if (PS->PlayerRole == ETreasurePlayerRole::Hunter) HunterPC = PC;
    }
    if (!ScoutPC || !HunterPC || !HunterPC->GetPawn()) return;

    const FRotator ViewRotation = HunterPC->GetControlRotation();
    const FVector ViewLocation = HunterPC->GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 72.f)
        + FRotator(0.f, ViewRotation.Yaw, 0.f).Vector() * 46.f;
    ScoutPC->ClientUpdateHunterView(ViewLocation, ViewRotation);
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
        if (PS->PlayerRole == ETreasurePlayerRole::Scout)
        {
            if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(PC->GetPawn()))
                Character->SetSpectatorHidden(true);
        }
        else if (PS->PlayerRole == ETreasurePlayerRole::Hunter)
        {
            PC->ClientReceiveSketch(SubmittedStrokes);
            if (APawn* Pawn = PC->GetPawn())
            {
                const FVector SpawnLocation = Island ? Island->FindSpawnPoint() : FVector(-3800.f, 0.f, 500.f);
                Pawn->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::ResetPhysics);
            }
        }
    }
}

bool ATreasureSketchGameMode::TryDig(const FVector& WorldLocation, float& OutDistance)
{
    OutDistance = FVector::Dist2D(WorldLocation, TreasureLocation);
    const float VerticalDistance = FMath::Abs(WorldLocation.Z - TreasureLocation.Z);
    if (FinishIfTimeExpired()) return false;
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->Phase != ETreasureRoundPhase::HunterSearching) return false;
    if (OutDistance <= TreasureRules::DigHorizontalRadius
        && VerticalDistance <= TreasureRules::DigVerticalHalfHeight)
    {
        GS->Phase = ETreasureRoundPhase::Won;
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
    if (Island) Island->Destroy();
    BuildRound();
    GS->bGameStarted = bWasStarted;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        if (!PC) continue;

        if (APawn* Pawn = PC->GetPawn())
        {
            const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
            const float SpawnY = PS && PS->PlayerRole == ETreasurePlayerRole::Hunter ? 400.f : 0.f;
            const FVector SpawnLocation = Island ? Island->FindSpawnPoint(SpawnY) : FVector(-3800.f, SpawnY, 500.f);
            Pawn->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::ResetPhysics);
            if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(Pawn))
                Character->SetSpectatorHidden(false);
        }
        PC->ClientStartNewRound(GS->RoundSerial);
    }
    RevealTreasureToScout();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_REPLAY New round started with seed=%d roles_swapped=%d"),
        IslandSeed, bSwapRoles ? 1 : 0);
}
