#include "TreasureSketchGameMode.h"

#include "ProceduralIsland.h"
#include "TreasureSurfacePaint.h"
#include "TreasureSketchCharacter.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchHUD.h"
#include "TreasureSketchPlayerController.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureRules.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"

namespace
{
uint8 GetRoomMapPoolMask(const ATreasureSketchGameState* GS)
{
    return GS ? (GS->bBeachInMapPool ? 1u << static_cast<uint8>(EIslandTheme::PirateBeach) : 0u)
        | (GS->bForestInMapPool ? 1u << static_cast<uint8>(EIslandTheme::MistForest) : 0u) : 0xff;
}

bool IsThemeInRoomMapPool(const ATreasureSketchGameState* GS, EIslandTheme Theme)
{
    return (GetRoomMapPoolMask(GS) & (1u << static_cast<uint8>(Theme))) != 0;
}
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
    RecoverFallenPlayers();
    UpdateHeldDigs();
    SendHunterViewToScout(DeltaSeconds);
}

bool ATreasureSketchGameMode::StartHeldDig(ATreasureSketchPlayerController* HunterController)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    ATreasureSketchPlayerState* Hunter = HunterController
        ? HunterController->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
    ATreasureSketchCharacter* Character = HunterController
        ? Cast<ATreasureSketchCharacter>(HunterController->GetPawn()) : nullptr;
    if (!HasAuthority() || !GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::HunterSearching
        || !Hunter || Hunter->PlayerRole != ETreasurePlayerRole::Hunter || !GS->PlayerArray.Contains(Hunter)
        || !Character || !Character->GetCharacterMovement()->IsMovingOnGround() || Hunter->bDigging
        || Hunter->GetDigCooldownRemaining(GS->RoundSerial, GS->GetServerWorldTimeSeconds()) > 0.f) return false;
    Hunter->bDigging = true;
    Hunter->DigStartedServerTime = GS->GetServerWorldTimeSeconds();
    Hunter->DigStartLocation = Character->GetActorLocation();
    Hunter->DigStartRoundSerial = GS->RoundSerial;
    Character->SetDiggingPose(true);
    Hunter->ForceNetUpdate();
    return true;
}

void ATreasureSketchGameMode::CancelHeldDig(ATreasureSketchPlayerState* Hunter, bool bNotifyInterrupted)
{
    if (!HasAuthority() || !Hunter || !Hunter->bDigging) return;
    Hunter->bDigging = false;
    if (const ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(Hunter->GetOwner()))
        if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(PC->GetPawn()))
            Character->SetDiggingPose(false);
    Hunter->ForceNetUpdate();
    if (bNotifyInterrupted)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(Hunter->GetOwner()))
            PC->ClientDigInterrupted();
}

void ATreasureSketchGameMode::UpdateHeldDigs()
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS) return;
    for (APlayerState* State : GS->PlayerArray)
    {
        ATreasureSketchPlayerState* Hunter = Cast<ATreasureSketchPlayerState>(State);
        if (!Hunter || !Hunter->bDigging) continue;
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(Hunter->GetOwner());
        ATreasureSketchCharacter* Character = PC ? Cast<ATreasureSketchCharacter>(PC->GetPawn()) : nullptr;
        if (!GS->bGameStarted || GS->Phase != ETreasureRoundPhase::HunterSearching
            || Hunter->DigStartRoundSerial != GS->RoundSerial || !Character
            || !Character->GetCharacterMovement()->IsMovingOnGround()
            || FVector::Dist2D(Character->GetActorLocation(), Hunter->DigStartLocation) > 100.f)
        {
            CancelHeldDig(Hunter, true);
            continue;
        }
        if (GS->GetServerWorldTimeSeconds() - Hunter->DigStartedServerTime < HeldDigSeconds) continue;
        CancelHeldDig(Hunter);
        float Distance = 0.f;
        bool bAttempted = false;
        const bool bFound = TryDig(Hunter, Character->GetActorLocation(), Distance, bAttempted);
        if (bAttempted)
            PC->ClientDigResult(bFound, TreasureRules::DigFeedbackBand(Distance, GS->RoomMapScale),
                GS->RoomMode == ETreasureRoomMode::ExplorerRace);
    }
}

void ATreasureSketchGameMode::RecoverFallenPlayers()
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !Island || !GS || !GS->bGameStarted
        || (GS->IsRoundOver() && !GS->bReviewingRound)) return;
    const float FallLimitZ = Island->GetActorLocation().Z - 600.f;
    const float Now = GS->GetServerWorldTimeSeconds();
    for (APlayerState* State : GS->PlayerArray)
    {
        ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State);
        ATreasureSketchPlayerController* PC = PS ? Cast<ATreasureSketchPlayerController>(PS->GetOwner()) : nullptr;
        ATreasureSketchCharacter* Character = PC ? Cast<ATreasureSketchCharacter>(PC->GetPawn()) : nullptr;
        if (!Character || Character->IsHidden()) continue;
        if (Character->GetActorLocation().Z < FallLimitZ)
        {
            CancelHeldDig(PS, true);
            Character->GetCharacterMovement()->StopMovementImmediately();
            Character->SetActorLocation(Island->FindSpawnPoint(), false, nullptr, ETeleportType::ResetPhysics);
            Character->bWasInShallowWater = false;
            Character->WaterSlowUntilServerTime = 0.f;
            Character->SetWaterSlowed(false);
            PC->ClientTerrainFeedback(2);
            continue;
        }
        const FVector Local = Character->GetActorLocation() - Island->GetActorLocation();
        const float FeetZ = Local.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float GroundZ = Island->HeightAt(Local.X, Local.Y);
        const bool bShallowWater = GroundZ < -10.f && GroundZ > -130.f && FeetZ < 15.f && FeetZ > -160.f;
        if (bShallowWater)
        {
            Character->WaterSlowUntilServerTime = Now + 1.5f;
            if (!Character->bWasInShallowWater) PC->ClientTerrainFeedback(1);
        }
        Character->bWasInShallowWater = bShallowWater;
        Character->SetWaterSlowed(bShallowWater || Character->WaterSlowUntilServerTime > Now);
    }
}

bool ATreasureSketchGameMode::FinishIfTimeExpired()
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted
        || (GS->Phase != ETreasureRoundPhase::ScoutDrawing && GS->Phase != ETreasureRoundPhase::HunterSearching)
        || GS->GetServerWorldTimeSeconds() < GS->RoundEndServerTime)
        return false;

    const bool bScoutTimedOut = GS->Phase == ETreasureRoundPhase::ScoutDrawing;
    if (bScoutTimedOut)
    {
        if (GS->RoomMode == ETreasureRoomMode::TeamVersus)
            for (APlayerState* State : GS->PlayerArray)
                if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State);
                    PS && PS->PlayerRole == ETreasurePlayerRole::Hunter && !PS->bVersusTreasurePlaced)
                {
                    const ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PS->GetOwner());
                    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
                    const int32 Team = PS->VersusTeam;
                    if (Team < 0 || Team > 1) continue;
                    FVector Location = Pawn ? Pawn->GetActorLocation() - FVector(0.f, 0.f, 50.f)
                        : TreasureLocation;
                    const FVector Local = Location - Island->GetActorLocation();
                    if (Island->HeightAt(Local.X, Local.Y) < 0.f
                        || (bVersusTreasurePlaced[1 - Team]
                            && FVector::Dist2D(Location, VersusTreasureLocations[1 - Team]) < 1200.f))
                    {
                        FRandomStream Stream(IslandSeed ^ (Team * 9719) ^ 0x41AE);
                        for (int32 Attempt = 0; Attempt < 120; ++Attempt)
                        {
                            const FVector Candidate = Island->FindRandomLandPoint(Stream, 115.f)
                                + FVector(0.f, 0.f, 35.f);
                            Location = Candidate;
                            if (!bVersusTreasurePlaced[1 - Team]
                                || FVector::Dist2D(Candidate, VersusTreasureLocations[1 - Team]) >= 1200.f) break;
                        }
                    }
                    VersusTreasureLocations[Team] = Location;
                    bVersusTreasurePlaced[Team] = true;
                    PS->bVersusTreasurePlaced = true;
                    PS->ForceNetUpdate();
                }
        BeginHunterSearching(CollectSketchPages());
        UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_AUTO_HANDOFF Drawing time expired"));
        return true;
    }
    GS->Phase = ETreasureRoundPhase::HunterTimedOut;
    GS->ResultServerTime = GS->GetServerWorldTimeSeconds();
    if (GS->RoomMode == ETreasureRoomMode::ExplorerRace) ScoreRaceRound(nullptr);
    RecordCompletedRound();
    HideTreasureFromScout();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_TIMEOUT Phase=%s"),
        bScoutTimedOut ? TEXT("ScoutDrawing") : TEXT("HunterSearching"));
    return true;
}

FVector ATreasureSketchGameMode::FindPlayerSpawn(TArray<FVector>& UsedSpawns) const
{
    FVector Result = Island ? Island->FindSpawnPoint() : FVector(-3800.f, 0.f, 500.f);
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (Island && GS && GS->bSpreadPlayerSpawns && !UsedSpawns.IsEmpty())
    {
        FRandomStream Stream(IslandSeed ^ (UsedSpawns.Num() * 7919));
        for (int32 Attempt = 0; Attempt < 80; ++Attempt)
        {
            Result = Island->FindRandomLandPoint(Stream, 115.f) + FVector(0.f, 0.f, 180.f);
            bool bOccupied = false;
            for (const FVector& Used : UsedSpawns)
                bOccupied |= FVector::Dist2D(Used, Result) < (Attempt < 60 ? 1800.f : 180.f);
            if (!bOccupied) { UsedSpawns.Add(Result); return Result; }
        }
    }
    // FindSpawnPoint snaps to safe land samples. Different offsets can resolve to
    // the same sample, so test actual positions before placing another player.
    for (int32 Attempt = 0; Island && Attempt < 40; ++Attempt)
    {
        const float Offset = Attempt == 0 ? 0.f : ((Attempt + 1) / 2) * 600.f * (Attempt % 2 ? 1.f : -1.f);
        Result = Island->FindSpawnPoint(Offset);
        bool bOccupied = false;
        for (const FVector& Used : UsedSpawns) bOccupied |= FVector::Dist2D(Used, Result) < 180.f;
        if (!bOccupied) break;
    }
    UsedSpawns.Add(Result);
    return Result;
}

FVector ATreasureSketchGameMode::FindHunterSpawn(TArray<FVector>& UsedSpawns) const
{
    FVector Candidate = FindPlayerSpawn(UsedSpawns);
    if (!Island || FVector::Dist2D(Candidate, TreasureLocation) >= TreasureRules::MinimumHunterSpawnDistance)
        return Candidate;
    UsedSpawns.Pop();
    FRandomStream Stream(IslandSeed ^ (UsedSpawns.Num() * 7919) ^ 0x563A);
    FVector Best = Candidate;
    for (int32 Attempt = 0; Attempt < 120; ++Attempt)
    {
        const FVector Point = Island->FindRandomLandPoint(Stream, 115.f) + FVector(0.f, 0.f, 180.f);
        if (FVector::Dist2D(Point, TreasureLocation) < TreasureRules::MinimumHunterSpawnDistance) continue;
        bool bTooClose = false;
        for (const FVector& Used : UsedSpawns) bTooClose |= FVector::Dist2D(Point, Used) < 180.f;
        Best = Point;
        if (!bTooClose) break;
    }
    UsedSpawns.Add(Best);
    return Best;
}

void ATreasureSketchGameMode::ResetSurfacePaint()
{
    if (SurfacePaint) SurfacePaint->Destroy();
    SurfacePaint = nullptr;
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        GS->SurfacePaintStampsUsed = 0;
        GS->ForceNetUpdate();
    }
}

void ATreasureSketchGameMode::ToggleSurfacePaint()
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->bGameStarted) return;
    GS->bSurfacePaintEnabled = !GS->bSurfacePaintEnabled;
    GS->ForceNetUpdate();
}

void ATreasureSketchGameMode::SpraySurface(const FHitResult& Hit)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (FinishIfTimeExpired() || !GS || !GS->bGameStarted || !GS->bSurfacePaintEnabled
        || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    if (!SurfacePaint) SurfacePaint = GetWorld()->SpawnActor<ATreasureSurfacePaint>();
    if (SurfacePaint && SurfacePaint->AddStamp(Hit))
    {
        ++GS->SurfacePaintStampsUsed;
        GS->ForceNetUpdate();
    }
}

void ATreasureSketchGameMode::BuildRound()
{
    ResetSurfacePaint();
    ResetSubmittedSketches();
    FRandomStream Stream(FDateTime::Now().GetTicks());
    int32 RequestedSeed = 0;
    IslandSeed = FParse::Value(FCommandLine::Get(), TEXT("IslandSeed="), RequestedSeed) && RequestedSeed != 0
        ? FMath::Abs(RequestedSeed)
        : Stream.RandRange(1000, 999999);
    Stream.Initialize(IslandSeed ^ 0x35D1A7);
    Island = GetWorld()->SpawnActorDeferred<AProceduralIsland>(AProceduralIsland::StaticClass(), FTransform::Identity);
    Island->Seed = IslandSeed;
    if (const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        Island->MapScale = GS->RoomMapScale;
    }
    FString RequestedTheme;
    if (FParse::Value(FCommandLine::Get(), TEXT("IslandTheme="), RequestedTheme))
        Island->Theme = RequestedTheme.Equals(TEXT("Forest"), ESearchCase::IgnoreCase)
            ? EIslandTheme::MistForest
            : RequestedTheme.Equals(TEXT("Ruins"), ESearchCase::IgnoreCase)
                ? EIslandTheme::JungleRuins : EIslandTheme::PirateBeach;
    else
        Island->Theme = AProceduralIsland::SelectThemeFromTable(IslandSeed, false,
            GetRoomMapPoolMask(GetGameState<ATreasureSketchGameState>()));
    Island->ConfigureThemeParameters();
    Island->FinishSpawning(FTransform::Identity);

    TreasureLocation = Island->FindRandomLandPoint(Stream,
        Island->Theme == EIslandTheme::MistForest ? 105.f : 170.f) + FVector(0.f, 0.f, 35.f);
    bVersusTreasurePlaced[0] = bVersusTreasurePlaced[1] = false;
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        GS->IslandSeed = IslandSeed;
        ++GS->RoundSerial;
        GS->Phase = ETreasureRoundPhase::ScoutDrawing;
        GS->ResultServerTime = 0.f;
        GS->VersusWinningTeam = -1;
        GS->bReviewingRound = false;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + GS->DrawingDurationSeconds;
        GS->bGameStarted = false;
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_ROUND_READY Seed=%d Theme=%s Shape=%s Treasure=%s"),
        IslandSeed, *Island->GetThemeName(), *Island->GetShapeName(), *TreasureLocation.ToCompactString());
}

bool ATreasureSketchGameMode::SelectRoomMode(ETreasureRoomMode Mode)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || GS->bGameStarted
        || (Mode != ETreasureRoomMode::OneMapmaker && Mode != ETreasureRoomMode::OneExplorer
            && Mode != ETreasureRoomMode::ExplorerRace && Mode != ETreasureRoomMode::TeamVersus)) return false;
    if (GS->RoomMode == Mode) return true;
    ATreasureSketchPlayerState* SinglePlayer = nullptr;
    const ETreasurePlayerRole PreviousSingleRole = GS->RoomMode == ETreasureRoomMode::OneExplorer
        ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
    if (GS->RoomMode == ETreasureRoomMode::TeamVersus && !GS->PlayerArray.IsEmpty())
        SinglePlayer = Cast<ATreasureSketchPlayerState>(GS->PlayerArray[0]);
    else
        for (APlayerState* State : GS->PlayerArray)
            if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS->PlayerRole == PreviousSingleRole)
            { SinglePlayer = PS; break; }
    HideTreasureFromScout();
    GS->RoomMode = Mode;
    GS->RaceRoundIndex = 0;
    GS->RaceTotalRounds = 0;
    GS->RaceRoundWinner.Reset();
    NormalizeRoomRoles(nullptr, SinglePlayer);
    RevealTreasureToScout();
    GS->ForceNetUpdate();
    return true;
}

bool ATreasureSketchGameMode::ClaimSingleRoomRole(ATreasureSketchPlayerState* Player)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || GS->bGameStarted || !Player || !GS->PlayerArray.Contains(Player)
        || GS->RoomMode == ETreasureRoomMode::TeamVersus) return false;
    const ETreasurePlayerRole SingleRole = GS->RoomMode == ETreasureRoomMode::OneExplorer
        ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
    if (Player->PlayerRole == SingleRole) return true;
    HideTreasureFromScout();
    NormalizeRoomRoles(nullptr, Player);
    RevealTreasureToScout();
    GS->ForceNetUpdate();
    return true;
}

bool ATreasureSketchGameMode::ToggleRoomMapPool(EIslandTheme Theme)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || GS->bGameStarted
        || (GetNetMode() != NM_ListenServer && GetNetMode() != NM_Standalone)) return false;
    bool* Selected = Theme == EIslandTheme::PirateBeach ? &GS->bBeachInMapPool
        : Theme == EIslandTheme::MistForest ? &GS->bForestInMapPool : nullptr;
    if (!Selected || (*Selected && !(Theme == EIslandTheme::PirateBeach
        ? GS->bForestInMapPool : GS->bBeachInMapPool))) return false;
    *Selected = !*Selected;
    GS->ForceNetUpdate();

    FString RequestedTheme;
    if (Island && !FParse::Value(FCommandLine::Get(), TEXT("IslandTheme="), RequestedTheme)
        && !IsThemeInRoomMapPool(GS, Island->Theme))
    {
        HideTreasureFromScout();
        Island->Destroy();
        BuildRound();
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
                PC->ClientStartNewRound(GS->RoundSerial);
        RevealTreasureToScout();
    }
    return true;
}

void ATreasureSketchGameMode::NormalizeRoomRoles(APlayerState* Excluded, ATreasureSketchPlayerState* PreferredSinglePlayer)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS) return;
    if (GS->RoomMode == ETreasureRoomMode::TeamVersus)
    {
        int32 Slot = 0;
        for (APlayerState* State : GS->PlayerArray)
            if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS != Excluded)
            {
                PS->VersusTeam = Slot / 2;
                PS->PlayerRole = Slot % 2 == 0 ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
                PS->ForceNetUpdate();
                ++Slot;
            }
        return;
    }
    const ETreasurePlayerRole SingleRole = GS->RoomMode == ETreasureRoomMode::OneExplorer
        ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
    const ETreasurePlayerRole OtherRole = SingleRole == ETreasurePlayerRole::Scout
        ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
    ATreasureSketchPlayerState* SinglePlayer = PreferredSinglePlayer != Excluded && GS->PlayerArray.Contains(PreferredSinglePlayer)
        ? PreferredSinglePlayer : nullptr;
    for (APlayerState* State : GS->PlayerArray)
        if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS != Excluded)
            if (!SinglePlayer && PS->PlayerRole == SingleRole) SinglePlayer = PS;
    if (!SinglePlayer)
        for (APlayerState* State : GS->PlayerArray)
            if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS != Excluded)
            { SinglePlayer = PS; break; }
    for (APlayerState* State : GS->PlayerArray)
        if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS != Excluded)
        {
            PS->VersusTeam = -1;
            PS->bVersusTreasurePlaced = false;
            PS->PlayerRole = PS == SinglePlayer ? SingleRole : OtherRole;
            PS->ForceNetUpdate();
        }
}

void ATreasureSketchGameMode::PlaceRoundPlayers()
{
    MapmakerLandingSpawns.Reset();
    TArray<FVector> UsedSpawns;
    if (Island) UsedSpawns.Add(Island->FindSpawnPoint());
    bool bFirstScoutPlaced = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            if (APawn* Pawn = PC->GetPawn())
            {
                const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                const bool bScout = PS && PS->PlayerRole == ETreasurePlayerRole::Scout;
                const FVector Spawn = Island && bScout && !bFirstScoutPlaced
                    ? Island->FindSpawnPoint() : bScout ? FindPlayerSpawn(UsedSpawns) : FindHunterSpawn(UsedSpawns);
                if (bScout) { bFirstScoutPlaced = true; MapmakerLandingSpawns.Add(Spawn); }
                Pawn->SetActorLocation(Spawn, false, nullptr, ETeleportType::ResetPhysics);
                if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(Pawn))
                {
                    Character->SetSpectatorHidden(false);
                    Character->SetShoveWindingUp(false);
                }
            }
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (GS && GS->RoomMode == ETreasureRoomMode::TeamVersus)
    {
        for (APlayerState* State : GS->PlayerArray)
            if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State);
                PS && PS->PlayerRole == ETreasurePlayerRole::Hunter)
                if (const ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PS->GetOwner()))
                    for (APlayerState* OtherState : GS->PlayerArray)
                        if (const ATreasureSketchPlayerState* Other = Cast<ATreasureSketchPlayerState>(OtherState);
                            Other && Other->VersusTeam == PS->VersusTeam
                            && Other->PlayerRole == ETreasurePlayerRole::Scout)
                            if (const ATreasureSketchPlayerController* OtherPC =
                                Cast<ATreasureSketchPlayerController>(Other->GetOwner()))
                                if (APawn* First = PC->GetPawn())
                                    if (APawn* Second = OtherPC->GetPawn())
                                    {
                                        First->MoveIgnoreActorAdd(Second);
                                        Second->MoveIgnoreActorAdd(First);
                                    }
    }
    else if (GS)
    {
        for (APlayerState* State : GS->PlayerArray)
            if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
                if (const ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PS->GetOwner()))
                    if (APawn* First = PC->GetPawn())
                        for (APlayerState* OtherState : GS->PlayerArray)
                            if (const ATreasureSketchPlayerState* Other = Cast<ATreasureSketchPlayerState>(OtherState);
                                Other && Other != PS)
                                if (const ATreasureSketchPlayerController* OtherPC =
                                    Cast<ATreasureSketchPlayerController>(Other->GetOwner()))
                                    if (APawn* Second = OtherPC->GetPawn()) First->MoveIgnoreActorRemove(Second);
    }
}

bool ATreasureSketchGameMode::SetRoomMapScale(float Scale)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || GS->bGameStarted || GetNetMode() == NM_DedicatedServer
        || !FMath::IsFinite(Scale) || Scale < GS->MinMapScale || Scale > GS->MaxMapScale) return false;
    GS->RoomMapScale = Scale;
    // Each theme converts the area multiplier to a length multiplier when generating terrain.
    GS->ForceNetUpdate();
    return true;
}

void ATreasureSketchGameMode::AdjustRoomSetting(FName Setting, int32 Direction)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || (GetNetMode() != NM_ListenServer && GetNetMode() != NM_Standalone) || !GS || GS->bGameStarted
        || (Direction != -1 && Direction != 1)) return;
    if (Setting == TEXT("MapSize"))
    {
        SetRoomMapScale(FMath::Clamp(GS->RoomMapScale + Direction * 0.25f, GS->MinMapScale, GS->MaxMapScale));
    }
    else if (Setting == TEXT("MovementSpeed"))
    {
        GS->MovementSpeedMultiplier = FMath::Clamp(GS->MovementSpeedMultiplier + Direction * 0.25f,
            GS->MinMovementSpeed, GS->MaxMovementSpeed);
        GS->ApplyMovementSpeed();
    }
    else if (Setting == TEXT("TreasureRange"))
    {
        GS->bTreasureRangeVisible = !GS->bTreasureRangeVisible;
    }
    else if (Setting == TEXT("SpreadPlayerSpawns"))
    {
        GS->bSpreadPlayerSpawns = !GS->bSpreadPlayerSpawns;
    }
    else if (Setting == TEXT("SketchSceneLock")) GS->bSketchSceneLock = !GS->bSketchSceneLock;
    else if (Setting == TEXT("PhotoClue")) GS->bPhotoClueEnabled = !GS->bPhotoClueEnabled;
    else if (Setting == TEXT("PreprintedIsland")) GS->bPreprintedIsland = !GS->bPreprintedIsland;
    else if (Setting == TEXT("LimitedInk")) GS->bLimitedInk = !GS->bLimitedInk;
    else if (Setting == TEXT("InkLimit"))
        GS->InkLimit = FMath::Clamp(GS->InkLimit + Direction * ATreasureSketchGameState::InkLimitStep,
            ATreasureSketchGameState::MinInkLimit, ATreasureSketchGameState::MaxInkLimit);
    else if (Setting == TEXT("DigCooldown"))
    {
        GS->DigCooldownSeconds = FMath::Clamp(GS->DigCooldownSeconds + Direction * ATreasureSketchGameState::DigCooldownStepSeconds,
            ATreasureSketchGameState::MinDigCooldownSeconds, ATreasureSketchGameState::MaxDigCooldownSeconds);
    }
    else
    {
        int32* Duration = Setting == TEXT("DrawingTime") ? &GS->DrawingDurationSeconds
            : Setting == TEXT("SearchingTime") ? &GS->SearchingDurationSeconds : nullptr;
        if (!Duration) return;
        *Duration = FMath::Clamp(*Duration + Direction * ATreasureSketchGameState::PhaseSecondsStep,
            ATreasureSketchGameState::MinPhaseSeconds, ATreasureSketchGameState::MaxPhaseSeconds);
    }
    GS->ForceNetUpdate();
}

void ATreasureSketchGameMode::ReturnToSetup()
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || !GS->bGameStarted) return;
    GS->RaceRoundIndex = 0;
    GS->RaceTotalRounds = 0;
    GS->RaceRoundWinner.Reset();
    HideTreasureFromScout();
    if (Island) Island->Destroy();
    BuildRound();
    GS->ForceNetUpdate();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
        {
            if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(PC->GetPawn()))
            {
                Character->SetSpectatorHidden(false);
                Character->SetShoveWindingUp(false);
            }
            PC->ClientStartNewRound(GS->RoundSerial);
            PC->ClientReturnToLobby();
        }
    RevealTreasureToScout();
}

void ATreasureSketchGameMode::StartHostedRound()
{
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        if (GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
        if (GS->PlayerArray.Num() < (GS->RoomMode == ETreasureRoomMode::TeamVersus ? 4
            : GS->RoomMode == ETreasureRoomMode::ExplorerRace ? 3 : 2)
            || GS->PlayerArray.Num() > ATreasureSketchGameState::MaxRoomPlayers
            )
        {
            UE_LOG(LogTemp, Warning, TEXT("TREASURE_ONLINE_START waiting for second player"));
            return;
        }
        int32 Scouts = 0, Hunters = 0;
        for (APlayerState* State : GS->PlayerArray)
            if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
            { Scouts += PS->PlayerRole == ETreasurePlayerRole::Scout; Hunters += PS->PlayerRole == ETreasurePlayerRole::Hunter; }
        const int32 ExpectedScouts = GS->RoomMode == ETreasureRoomMode::TeamVersus ? 2
            : GS->RoomMode == ETreasureRoomMode::OneExplorer ? GS->PlayerArray.Num() - 1 : 1;
        if (Scouts != ExpectedScouts || Hunters != GS->PlayerArray.Num() - ExpectedScouts) return;
        if (GS->RoomMode == ETreasureRoomMode::TeamVersus)
        {
            GS->VersusWinningTeam = -1;
            for (APlayerState* State : GS->PlayerArray)
                if (ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
                { Member->bVersusTreasurePlaced = false; Member->ForceNetUpdate(); }
        }
        if (GS->RoomMode == ETreasureRoomMode::ExplorerRace)
        {
            CurrentRaceSeriesId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
            GS->RaceRoundIndex = 1;
            GS->RaceTotalRounds = GS->PlayerArray.Num();
            GS->RaceRoundWinner.Reset();
            for (APlayerState* State : GS->PlayerArray)
                if (ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
                {
                    Member->RacePoints = 0;
                    Member->RaceFinds = 0;
                    Member->RaceLastRoundPoints = 0;
                    Member->RoundShoveHits = 0;
                    Member->RaceBestMissDistance = TNumericLimits<float>::Max();
                    Member->RaceFarMisses = 0;
                    Member->NextShoveServerTime = 0.f;
                    Member->ShoveProtectedUntilServerTime = 0.f;
                    Member->ForceNetUpdate();
                }
        }
        // Lobby previews may have been built before the host changed map size.
        FString RequestedTheme;
        const bool bThemeForced = FParse::Value(FCommandLine::Get(), TEXT("IslandTheme="), RequestedTheme);
        if (!Island || !FMath::IsNearlyEqual(Island->MapScale, GS->RoomMapScale)
            || (!bThemeForced && !IsThemeInRoomMapPool(GS, Island->Theme)))
        {
            HideTreasureFromScout();
            if (Island) Island->Destroy();
            BuildRound();
            for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
                if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
                    PC->ClientStartNewRound(GS->RoundSerial);
            RevealTreasureToScout();
        }
        PlaceRoundPlayers();
        GS->bGameStarted = true;
        GS->ForceNetUpdate();
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + GS->DrawingDurationSeconds;
        const TArray<FSketchPage> LivePages = CollectSketchPages();
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
                if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                    PS && PS->PlayerRole == ETreasurePlayerRole::Hunter
                    && GS->RoomMode != ETreasureRoomMode::TeamVersus)
                    PC->ClientInitializeLiveSketch(GS->RoundSerial, LivePages);
        UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_START players=%d grid=%d drawing=%d searching=%d"),
            GS->PlayerArray.Num(), Island->GridSize, GS->DrawingDurationSeconds, GS->SearchingDurationSeconds);
    }
}

void ATreasureSketchGameMode::StartSoloTest(int32 ThemeChoice)
{
    if (!HasAuthority()) return;
    ResetSurfacePaint();
    ResetSubmittedSketches();
    const bool bHunterGameplayTest = ThemeChoice == -2;
    const bool bFullFlowTest = ThemeChoice == -3;

    // The menu preview may already have revealed the previous round's marker while the
    // local player was assigned Scout. Remove it before replacing the island/treasure.
    HideTreasureFromScout();
    if (Island) Island->Destroy();
    FRandomStream Stream(FDateTime::Now().GetTicks());
    IslandSeed = Stream.RandRange(1000, 999999);
    if (bHunterGameplayTest || bFullFlowTest)
    {
        if (const ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(GetWorld()->GetFirstPlayerController()))
            if (PC->GetTestSeed() > 0) IslandSeed = PC->GetTestSeed();
    }
    Stream.Initialize(IslandSeed ^ 0x35D1A7);

    Island = GetWorld()->SpawnActorDeferred<AProceduralIsland>(AProceduralIsland::StaticClass(), FTransform::Identity);
    Island->Seed = IslandSeed;
    Island->Theme = ThemeChoice == 0 ? EIslandTheme::PirateBeach
        : ThemeChoice == 1 ? EIslandTheme::MistForest
        : AProceduralIsland::SelectThemeFromTable(IslandSeed, true);
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (GS && (bHunterGameplayTest || bFullFlowTest))
    {
        Island->MapScale = GS->RoomMapScale;
    }
    Island->FinishSpawning(FTransform::Identity);
    if (bHunterGameplayTest) Island->Tags.Add(TEXT("SoloHunterGameplayTest"));
    if (bFullFlowTest) Island->Tags.Add(TEXT("SoloFullFlowTest"));
    TreasureLocation = Island->FindRandomLandPoint(Stream, Island->Theme == EIslandTheme::MistForest ? 105.f : 170.f) + FVector(0.f, 0.f, 35.f);

    if (GS)
    {
        GS->IslandSeed = IslandSeed;
        ++GS->RoundSerial;
        GS->Phase = bFullFlowTest ? ETreasureRoundPhase::ScoutDrawing : ETreasureRoundPhase::HunterSearching;
        GS->ResultServerTime = 0.f;
        GS->bReviewingRound = false;
        GS->bGameStarted = true;
        GS->ForceNetUpdate();
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + (bFullFlowTest ? GS->DrawingDurationSeconds
            : bHunterGameplayTest ? GS->SearchingDurationSeconds : 3600.f);
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
        if (PS) PS->PlayerRole = bFullFlowTest ? ETreasurePlayerRole::Scout : ETreasurePlayerRole::Hunter;
        if (PC && PC->GetPawn())
        {
            if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(PC->GetPawn()))
                Character->SetSpectatorHidden(false);
            TArray<FVector> UsedSpawns;
            PC->GetPawn()->SetActorLocation(bFullFlowTest ? Island->FindSpawnPoint() : FindHunterSpawn(UsedSpawns),
                false, nullptr, ETeleportType::ResetPhysics);
        }
        // Solo map testing deliberately shows the exact marker and its debug cylinder.
        // It must use the newly generated treasure location, not the menu preview location.
        if (PC)
        {
            PC->ClientStartNewRound(GS ? GS->RoundSerial : 0);
            if (!bFullFlowTest) PC->ClientReceiveSketch(TArray<FSketchStroke>());
            if (!bHunterGameplayTest) PC->ClientRevealTreasure(TreasureLocation);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SOLO_TEST Seed=%d Theme=%s Shape=%s"),
        IslandSeed, *Island->GetThemeName(), *Island->GetShapeName());
}

void ATreasureSketchGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!ErrorMessage.IsEmpty() || !GS) return;
    if (GS->PlayerArray.Num() >= ATreasureSketchGameState::MaxRoomPlayers)
        ErrorMessage = TEXT("Room is full (4 players).");
    else if (GS->bGameStarted)
        ErrorMessage = TEXT("Round already started. Join before the next game.");
}

void ATreasureSketchGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS) return;
    // A departure changes the party. Return everyone to the lobby rather than
    // leaving hunters waiting forever for a disconnected mapmaker.
    const APlayerState* DepartingState = Exiting ? Exiting->GetPlayerState<APlayerState>() : nullptr;
    NormalizeRoomRoles(const_cast<APlayerState*>(DepartingState));
    if (GS->bGameStarted)
    {
        GS->RaceRoundIndex = 0;
        GS->RaceTotalRounds = 0;
        GS->RaceRoundWinner.Reset();
        HideTreasureFromScout();
        if (Island) Island->Destroy();
        BuildRound();
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            {
                if (PC == Exiting) continue;
                if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(PC->GetPawn()))
                    Character->SetSpectatorHidden(false);
                PC->ClientStartNewRound(GS->RoundSerial);
                PC->ClientReturnToLobby();
            }
        RevealTreasureToScout();
    }
}

void ATreasureSketchGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    ATreasureSketchPlayerState* NewPS = NewPlayer ? NewPlayer->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
    if (!NewPS) return;

    NormalizeRoomRoles();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_PLAYER_JOIN Role=%s"),
        NewPS->PlayerRole == ETreasurePlayerRole::Scout ? TEXT("Scout")
        : NewPS->PlayerRole == ETreasurePlayerRole::Hunter ? TEXT("Hunter") : TEXT("Unassigned"));
    RevealTreasureToScout();
}

void ATreasureSketchGameMode::RevealTreasureToScout()
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!Island || !GS || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || GS->RoomMode == ETreasureRoomMode::TeamVersus) return;

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

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* ScoutPC = Cast<ATreasureSketchPlayerController>(It->Get());
        if (!ScoutPC || !ScoutPC->IsLocalScout()) continue;
        ATreasureSketchPlayerController* HunterPC = nullptr;
        int32 Index = 0;
        for (APlayerState* State : GS->PlayerArray)
            if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
                if (PS->PlayerRole == ETreasurePlayerRole::Hunter && Index++ == ScoutPC->GetSpectatedHunterIndex())
                { HunterPC = Cast<ATreasureSketchPlayerController>(State->GetOwner()); break; }
        if (!HunterPC || !HunterPC->GetPawn()) continue;
        const FRotator ViewRotation = HunterPC->GetControlRotation();
        const FVector ViewLocation = HunterPC->GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 72.f)
            + FRotator(0.f, ViewRotation.Yaw, 0.f).Vector() * 46.f;
        ScoutPC->ClientUpdateHunterView(ViewLocation, ViewRotation, HunterPC->PlayerState);
    }
}

void ATreasureSketchGameMode::HandoffToHunter(const TArray<FSketchStroke>& SubmittedStrokes)
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || (GS->RoomMode != ETreasureRoomMode::OneMapmaker
        && GS->RoomMode != ETreasureRoomMode::ExplorerRace)) return;
    FSketchPage Page;
    Page.MapmakerName = TEXT("地图师");
    Page.Strokes = SubmittedStrokes;
    BeginHunterSearching({ Page });
}

void ATreasureSketchGameMode::ResetSubmittedSketches()
{
    SubmittedSketches.Reset();
    MapmakerLandingSpawns.Reset();
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
        for (APlayerState* State : GS->PlayerArray)
            if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
            {
                PS->bSketchSubmitted = false;
                PS->bVersusTreasurePlaced = false;
                PS->ForceNetUpdate();
                if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PS->GetOwner()))
                    PC->ResetRoundPhoto();
            }
}

TArray<FSketchPage> ATreasureSketchGameMode::CollectSketchPages() const
{
    TArray<FSketchPage> Pages;
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS) return Pages;
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS->PlayerRole == ETreasurePlayerRole::Scout)
        {
            FSketchPage Page;
            Page.MapmakerId = PS->GetPlayerId();
            if (const FSketchPage* Submitted = SubmittedSketches.Find(PS->GetPlayerId())) Page = *Submitted;
            else
            {
                Page.MapmakerName = PS->GetPlayerName();
                if (const ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PS->GetOwner()))
                {
                    Page.Strokes = PC->GetServerDrawing();
                    Page.PhotoJpeg = PC->GetServerPhoto();
                }
            }
            if (Page.MapmakerName.IsEmpty()) Page.MapmakerName = FString::Printf(TEXT("地图师 %d"), Pages.Num() + 1);
            Pages.Add(MoveTemp(Page));
        }
    return Pages;
}

TArray<FSketchPage> ATreasureSketchGameMode::VersusPagesForTeam(
    const TArray<FSketchPage>& Pages, int32 Team) const
{
    TArray<FSketchPage> TeamPages;
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->RoomMode != ETreasureRoomMode::TeamVersus || Team < 0 || Team > 1) return TeamPages;
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* Teammate = Cast<ATreasureSketchPlayerState>(State);
            Teammate && Teammate->VersusTeam == Team && Teammate->PlayerRole == ETreasurePlayerRole::Scout)
            for (const FSketchPage& Page : Pages)
                if (Page.MapmakerId == Teammate->GetPlayerId())
                { TeamPages.Add(Page); return TeamPages; }
    return TeamPages;
}

void ATreasureSketchGameMode::SubmitPlayerSketch(ATreasureSketchPlayerState* Scout, const TArray<FSketchStroke>& SubmittedStrokes)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || !Scout || !GS->PlayerArray.Contains(Scout) || Scout->PlayerRole != ETreasurePlayerRole::Scout || Scout->bSketchSubmitted) return;
    FSketchPage Page;
    Page.MapmakerId = Scout->GetPlayerId();
    Page.MapmakerName = Scout->GetPlayerName();
    if (const ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(Scout->GetOwner()))
        Page.PhotoJpeg = PC->GetServerPhoto();
    int32 InkUsed = 0, EraserUsed = 0;
    for (const FSketchStroke& Stroke : SubmittedStrokes)
    {
        if (Stroke.ColorIndex > 5 || Stroke.Points.Num() > 10000 || Stroke.EraserSize > 1
            || (Stroke.ColorIndex != 5 && Stroke.EraserSize != 0)) continue;
        const bool bEraser = Stroke.ColorIndex == 5;
        const int32 Remaining = bEraser ? 5000 - EraserUsed
            : (GS->bLimitedInk ? GS->InkLimit : 10000) - InkUsed;
        if (Remaining <= 0) continue;
        FSketchStroke SafeStroke;
        SafeStroke.ColorIndex = Stroke.ColorIndex;
        SafeStroke.EraserSize = Stroke.EraserSize;
        for (const FVector2D& Point : Stroke.Points)
        {
            if (SafeStroke.Points.Num() >= Remaining) break;
            if (FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y)
                && Point.X >= 0.f && Point.X <= 1.f && Point.Y >= 0.f && Point.Y <= 1.f)
                SafeStroke.Points.Add(Point);
        }
        if (bEraser) EraserUsed += SafeStroke.Points.Num();
        else InkUsed += SafeStroke.Points.Num();
        if (!SafeStroke.Points.IsEmpty()) Page.Strokes.Add(MoveTemp(SafeStroke));
    }
    SubmittedSketches.Add(Scout->GetPlayerId(), MoveTemp(Page));
    Scout->bSketchSubmitted = true;
    Scout->ForceNetUpdate();
    if (const FSketchPage* Submitted = SubmittedSketches.Find(Scout->GetPlayerId()))
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
                if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                    PS && PS->PlayerRole == ETreasurePlayerRole::Hunter
                    && GS->RoomMode != ETreasureRoomMode::TeamVersus)
                    PC->ClientReplaceLiveSketch(GS->RoundSerial, *Submitted);
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
            if (PS->PlayerRole == ETreasurePlayerRole::Scout && !PS->bSketchSubmitted) return;
    if (GS->RoomMode == ETreasureRoomMode::TeamVersus) TryBeginVersusSearch();
    else BeginHunterSearching(CollectSketchPages());
}

bool ATreasureSketchGameMode::TryPlaceVersusTreasure(ATreasureSketchPlayerController* HiderController)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    ATreasureSketchPlayerState* PS = HiderController
        ? HiderController->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
    ATreasureSketchCharacter* Character = HiderController
        ? Cast<ATreasureSketchCharacter>(HiderController->GetPawn()) : nullptr;
    if (!HasAuthority() || !GS || !GS->bGameStarted || GS->RoomMode != ETreasureRoomMode::TeamVersus
        || GS->Phase != ETreasureRoundPhase::ScoutDrawing || !PS || !GS->PlayerArray.Contains(PS)
        || PS->PlayerRole != ETreasurePlayerRole::Hunter || PS->bVersusTreasurePlaced
        || PS->VersusTeam < 0 || PS->VersusTeam > 1 || !Island || !Character
        || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
    const int32 Team = PS->VersusTeam;
    const FVector Location = Character->GetActorLocation() - FVector(0.f, 0.f, 50.f);
    const FVector Local = Location - Island->GetActorLocation();
    if (Island->HeightAt(Local.X, Local.Y) < 0.f
        || (bVersusTreasurePlaced[1 - Team]
            && FVector::Dist2D(Location, VersusTreasureLocations[1 - Team]) < 1200.f)) return false;
    VersusTreasureLocations[Team] = Location;
    bVersusTreasurePlaced[Team] = true;
    PS->bVersusTreasurePlaced = true;
    PS->ForceNetUpdate();
    HiderController->ClientRevealTreasure(Location);
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* Other = Cast<ATreasureSketchPlayerState>(State);
            Other && Other->VersusTeam != Team && Other->PlayerRole == ETreasurePlayerRole::Scout)
            if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(Other->GetOwner()))
                PC->ClientRevealTreasure(Location);
    TryBeginVersusSearch();
    return true;
}

void ATreasureSketchGameMode::TryBeginVersusSearch()
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->RoomMode != ETreasureRoomMode::TeamVersus || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || !bVersusTreasurePlaced[0] || !bVersusTreasurePlaced[1]) return;
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State);
            PS && PS->PlayerRole == ETreasurePlayerRole::Scout && !PS->bSketchSubmitted) return;
    BeginHunterSearching(CollectSketchPages());
}

void ATreasureSketchGameMode::BroadcastSketchDelta(ATreasureSketchPlayerState* Scout, int32 StrokeIndex, uint8 ColorIndex, uint8 EraserSize, const TArray<FVector2D>& Points)
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing || !Scout
        || Scout->PlayerRole != ETreasurePlayerRole::Scout || Scout->bSketchSubmitted) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                PS && PS->PlayerRole == ETreasurePlayerRole::Hunter
                && GS->RoomMode != ETreasureRoomMode::TeamVersus)
                PC->ClientAppendLiveSketch(GS->RoundSerial, Scout->GetPlayerId(), StrokeIndex, ColorIndex, EraserSize, Points);
}

void ATreasureSketchGameMode::BroadcastSketchClear(ATreasureSketchPlayerState* Scout)
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing || !Scout
        || Scout->PlayerRole != ETreasurePlayerRole::Scout || Scout->bSketchSubmitted) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                PS && PS->PlayerRole == ETreasurePlayerRole::Hunter
                && GS->RoomMode != ETreasureRoomMode::TeamVersus)
                PC->ClientClearLiveSketch(GS->RoundSerial, Scout->GetPlayerId());
}

void ATreasureSketchGameMode::BroadcastSketchPhoto(ATreasureSketchPlayerState* Scout, const TArray<uint8>& PhotoJpeg)
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing || !Scout
        || Scout->PlayerRole != ETreasurePlayerRole::Scout || Scout->bSketchSubmitted) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                PS && PS->PlayerRole == ETreasurePlayerRole::Hunter
                && GS->RoomMode != ETreasureRoomMode::TeamVersus)
                PC->ClientReceiveLivePhoto(GS->RoundSerial, Scout->GetPlayerId(), PhotoJpeg);
}

void ATreasureSketchGameMode::BeginHunterSearching(const TArray<FSketchPage>& Pages)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    GS->Phase = ETreasureRoundPhase::HunterSearching;
    GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + GS->SearchingDurationSeconds;
    GS->ForceNetUpdate();
    if (GS->RoomMode == ETreasureRoomMode::TeamVersus)
    {
        TArray<FVector> FinderSpawns;
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
            ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
            if (!PC || !PS) continue;
            if (PS->PlayerRole == ETreasurePlayerRole::Scout)
            {
                PS->bSketchSubmitted = true;
                PS->ForceNetUpdate();
                if (PS->VersusTeam >= 0 && PS->VersusTeam <= 1)
                    PC->ClientRevealTreasure(VersusTreasureLocations[PS->VersusTeam]);
            }
            else if (PS->VersusTeam >= 0 && PS->VersusTeam <= 1)
            {
                PC->ClientReceiveSketchPages(GS->RoundSerial, VersusPagesForTeam(Pages, PS->VersusTeam));
                PC->ClientHideTreasure();
                if (APawn* Pawn = PC->GetPawn())
                {
                    FVector Spawn = FindHunterSpawn(FinderSpawns);
                    FRandomStream Stream(IslandSeed ^ (PS->VersusTeam * 7703) ^ 0x5821);
                    for (int32 Attempt = 0; Attempt < 100; ++Attempt)
                    {
                        const FVector Candidate = Island->FindRandomLandPoint(Stream, 115.f) + FVector(0.f, 0.f, 180.f);
                        if (FVector::Dist2D(Candidate, VersusTreasureLocations[1 - PS->VersusTeam]) >=
                            TreasureRules::MinimumHunterSpawnDistance
                            && (FinderSpawns.Num() < 2
                                || FVector::Dist2D(Candidate, FinderSpawns[0]) >= 180.f))
                        { Spawn = Candidate; break; }
                    }
                    FinderSpawns.Last() = Spawn;
                    Pawn->SetActorLocation(Spawn, false, nullptr, ETeleportType::ResetPhysics);
                }
            }
        }
        return;
    }
    HideTreasureFromScout();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_HANDOFF Hunter active; marker hidden"));

    TArray<FVector> HunterSpawns;
    // In nearby mode, explorers start by the mapmaker's landing point. In spread
    // mode reserve that point so even the first explorer starts elsewhere.
    if (Island && GS->bSpreadPlayerSpawns)
    {
        HunterSpawns = MapmakerLandingSpawns;
        if (HunterSpawns.IsEmpty()) HunterSpawns.Add(Island->FindSpawnPoint());
    }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
        if (!PC || !PS) continue;
        if (PS->PlayerRole == ETreasurePlayerRole::Scout)
        { PS->bSketchSubmitted = true; PS->ForceNetUpdate(); }
        if (Island && Island->Tags.Contains(TEXT("SoloFullFlowTest")))
        {
            PS->PlayerRole = ETreasurePlayerRole::Hunter;
            PS->ForceNetUpdate();
        }
        if (PS->PlayerRole == ETreasurePlayerRole::Scout)
        {
            if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(PC->GetPawn()))
                Character->SetSpectatorHidden(true);
            PC->ClientReceiveSketchPages(GS->RoundSerial, Pages);
        }
        else if (PS->PlayerRole == ETreasurePlayerRole::Hunter)
        {
            PC->ClientReceiveSketchPages(GS->RoundSerial, Pages);
            if (APawn* Pawn = PC->GetPawn())
            {
                const FVector SpawnLocation = FindHunterSpawn(HunterSpawns);
                Pawn->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::ResetPhysics);
            }
        }
    }
}

bool ATreasureSketchGameMode::TryDig(ATreasureSketchPlayerState* Hunter, const FVector& WorldLocation,
    float& OutDistance, bool& bAttempted)
{
    bAttempted = false;
    OutDistance = 0.f;
    const ATreasureSketchGameState* InitialGS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !Hunter || !InitialGS
        || Hunter->PlayerRole != ETreasurePlayerRole::Hunter) return false;
    if (FinishIfTimeExpired()) return false;
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::HunterSearching
        || !GS->PlayerArray.Contains(Hunter)) return false;
    if (GS->RoomMode == ETreasureRoomMode::TeamVersus
        && (Hunter->VersusTeam < 0 || Hunter->VersusTeam > 1
            || !bVersusTreasurePlaced[1 - Hunter->VersusTeam])) return false;
    const float ServerTime = GS->GetServerWorldTimeSeconds();
    if (Hunter->GetDigCooldownRemaining(GS->RoundSerial, ServerTime) > 0.f) return false;
    bAttempted = true;
    Hunter->DigCooldownRoundSerial = GS->RoundSerial;
    Hunter->NextDigServerTime = ServerTime + GS->DigCooldownSeconds;
    Hunter->ForceNetUpdate();
    const FVector Target = GS->RoomMode == ETreasureRoomMode::TeamVersus && Hunter->VersusTeam >= 0
        && Hunter->VersusTeam <= 1 ? VersusTreasureLocations[1 - Hunter->VersusTeam] : TreasureLocation;
    OutDistance = FVector::Dist2D(WorldLocation, Target);
    const float VerticalDistance = FMath::Abs(WorldLocation.Z - Target.Z);
    if (OutDistance <= TreasureRules::DigHorizontalRadius
        && VerticalDistance <= TreasureRules::DigVerticalHalfHeight)
    {
        GS->ResultServerTime = ServerTime;
        if (GS->RoomMode == ETreasureRoomMode::ExplorerRace) ScoreRaceRound(Hunter);
        if (GS->RoomMode == ETreasureRoomMode::TeamVersus) GS->VersusWinningTeam = Hunter->VersusTeam;
        GS->Phase = ETreasureRoundPhase::Won;
        GS->ForceNetUpdate();
        RecordCompletedRound();
        UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_FOUND Distance=%.1f"), OutDistance);
        return true;
    }
    if (GS->RoomMode == ETreasureRoomMode::ExplorerRace)
    {
        Hunter->RaceBestMissDistance = FMath::Min(Hunter->RaceBestMissDistance, OutDistance);
        if (TreasureRules::DigFeedbackBand(OutDistance, GS->RoomMapScale) == 4)
            Hunter->RaceFarMisses = FMath::Min(2, Hunter->RaceFarMisses + 1);
    }
    return false;
}

void ATreasureSketchGameMode::ScoreRaceRound(ATreasureSketchPlayerState* Finder)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->RoomMode != ETreasureRoomMode::ExplorerRace) return;
    GS->RaceRoundWinner = Finder ? Finder->GetPlayerName() : FString();
    const float FractionRemaining = FMath::Clamp((GS->RoundEndServerTime - GS->GetServerWorldTimeSeconds())
        / FMath::Max(1, GS->SearchingDurationSeconds), 0.f, 1.f);
    for (APlayerState* State : GS->PlayerArray)
        if (ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
        {
            int32 Delta = 0;
            if (Member == Finder)
            {
                Delta = 8 + FMath::RoundToInt(8.f * FractionRemaining) - Member->RaceFarMisses;
                ++Member->RaceFinds;
            }
            else if (Member->PlayerRole == ETreasurePlayerRole::Scout)
                Delta = Finder ? 3 + FMath::RoundToInt(4.f * FractionRemaining) : 0;
            else if (Member->PlayerRole == ETreasurePlayerRole::Hunter)
                Delta = TreasureRules::RaceProximityPoints(Member->RaceBestMissDistance, GS->RoomMapScale)
                    - Member->RaceFarMisses;
            Member->RaceLastRoundPoints = Delta;
            Member->RacePoints += Delta;
            Member->ForceNetUpdate();
        }
    GS->ForceNetUpdate();
}

void ATreasureSketchGameMode::RecordCompletedRound()
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || !GS->bGameStarted || !GS->IsRoundOver() || !Island) return;
    FPlayedRoundRecord Base;
    Base.RecordId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Base.SeriesId = GS->RoomMode == ETreasureRoomMode::ExplorerRace ? CurrentRaceSeriesId : FString();
    Base.UtcTimeIso = FDateTime::UtcNow().ToIso8601();
    Base.RoundSerial = GS->RoundSerial;
    Base.IslandSeed = IslandSeed;
    Base.Theme = Island->Theme;
    Base.MapScale = Island->MapScale;
    Base.RoomMode = GS->RoomMode;
    Base.Outcome = GS->Phase;
    Base.WinnerName = GS->RoomMode == ETreasureRoomMode::TeamVersus && GS->VersusWinningTeam >= 0
        ? (GS->VersusWinningTeam == 0 ? TEXT("红队") : TEXT("蓝队")) : GS->RaceRoundWinner;
    Base.SearchSeconds = FMath::Clamp(GS->ResultServerTime
        - (GS->RoundEndServerTime - GS->SearchingDurationSeconds), 0.f,
        static_cast<float>(GS->SearchingDurationSeconds));
    Base.RaceRoundIndex = GS->RaceRoundIndex;
    Base.RaceTotalRounds = GS->RaceTotalRounds;
    Base.bPreprintedIsland = GS->bPreprintedIsland;
    if (Base.bPreprintedIsland)
    {
        constexpr int32 Samples = 48;
        Base.IslandTemplateMask.SetNumZeroed(Samples * Samples);
        const float Half = (Island->GridSize - 1) * Island->CellSize * 0.5f;
        for (int32 Y = 0; Y < Samples; ++Y)
            for (int32 X = 0; X < Samples; ++X)
                Base.IslandTemplateMask[Y * Samples + X] = Island->HeightAt(
                    -Half + (X + 0.5f) * 2.f * Half / Samples,
                    -Half + (Y + 0.5f) * 2.f * Half / Samples) > 0.f;
    }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>())
            {
                FPlayedRoundRecord Personal = Base;
                Personal.LocalPlayerName = PS->GetPlayerName();
                Personal.LocalRole = PS->PlayerRole;
                Personal.RaceRoundPoints = PS->RaceLastRoundPoints;
                Personal.RaceTotalPoints = PS->RacePoints;
                PC->ClientRecordCompletedRound(Personal);
            }
}

bool ATreasureSketchGameMode::TryShove(ATreasureSketchPlayerController* ShovingPlayer)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || (GS->RoomMode != ETreasureRoomMode::ExplorerRace
        && GS->RoomMode != ETreasureRoomMode::TeamVersus)
        || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::HunterSearching
        || !ShovingPlayer || !ShovingPlayer->GetPawn()) return false;
    ATreasureSketchPlayerState* ShovingState = ShovingPlayer->GetPlayerState<ATreasureSketchPlayerState>();
    ATreasureSketchCharacter* ShovingCharacter = Cast<ATreasureSketchCharacter>(ShovingPlayer->GetPawn());
    const float Now = GS->GetServerWorldTimeSeconds();
    if (!ShovingState || (GS->RoomMode == ETreasureRoomMode::TeamVersus
            ? ShovingState->PlayerRole != ETreasurePlayerRole::Scout
            : ShovingState->PlayerRole != ETreasurePlayerRole::Hunter)
        || !ShovingCharacter || ShovingState->bDigging || ShovingState->NextShoveServerTime > Now) return false;

    ShovingState->NextShoveServerTime = Now + 5.f;
    ShovingState->ForceNetUpdate();
    ShovingCharacter->SetShoveWindingUp(true);
    ShovingPlayer->ClientShoveFeedback(0, FString()); // Windup.
    const int32 RoundSerial = GS->RoundSerial;
    TWeakObjectPtr<ATreasureSketchPlayerController> WeakController(ShovingPlayer);
    TWeakObjectPtr<ATreasureSketchCharacter> WeakCharacter(ShovingCharacter);
    FTimerHandle ShoveHandle;
    GetWorld()->GetTimerManager().SetTimer(ShoveHandle, FTimerDelegate::CreateWeakLambda(this,
        [this, WeakController, WeakCharacter, RoundSerial]()
        {
            if (WeakController.IsValid() && WeakCharacter.IsValid())
                ResolveShove(WeakController.Get(), WeakCharacter.Get(), RoundSerial);
        }), 0.35f, false);
    return true;
}

void ATreasureSketchGameMode::ResolveShove(ATreasureSketchPlayerController* ShovingPlayer,
    ATreasureSketchCharacter* ShovingCharacter, int32 RoundSerial)
{
    if (!ShovingCharacter) return;
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->RoundSerial != RoundSerial) return;
    ShovingCharacter->SetShoveWindingUp(false);
    if ((GS->RoomMode != ETreasureRoomMode::ExplorerRace && GS->RoomMode != ETreasureRoomMode::TeamVersus)
        || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::HunterSearching
        || !ShovingPlayer || ShovingPlayer->GetPawn() != ShovingCharacter) return;
    ATreasureSketchPlayerState* ShovingState = ShovingPlayer->GetPlayerState<ATreasureSketchPlayerState>();
    if (!ShovingState || (GS->RoomMode == ETreasureRoomMode::TeamVersus
            ? ShovingState->PlayerRole != ETreasurePlayerRole::Scout
            : ShovingState->PlayerRole != ETreasurePlayerRole::Hunter)) return;

    ATreasureSketchCharacter* Target = nullptr;
    ATreasureSketchPlayerState* TargetState = nullptr;
    float BestDistanceSquared = FMath::Square(260.f);
    const FVector Origin = ShovingCharacter->GetActorLocation();
    const FVector Facing = ShovingPlayer->GetControlRotation().Vector().GetSafeNormal2D();
    for (APlayerState* State : GS->PlayerArray)
        if (ATreasureSketchPlayerState* OtherState = Cast<ATreasureSketchPlayerState>(State);
            OtherState && OtherState != ShovingState
            && (GS->RoomMode == ETreasureRoomMode::TeamVersus
                ? OtherState->PlayerRole == ETreasurePlayerRole::Hunter && OtherState->VersusTeam != ShovingState->VersusTeam
                : OtherState->PlayerRole == ETreasurePlayerRole::Hunter))
            if (const ATreasureSketchPlayerController* Other = Cast<ATreasureSketchPlayerController>(OtherState->GetOwner()))
                if (ATreasureSketchCharacter* OtherCharacter = Cast<ATreasureSketchCharacter>(Other->GetPawn()))
                {
                    const FVector Delta = OtherCharacter->GetActorLocation() - Origin;
                    const float DistanceSquared = Delta.SizeSquared2D();
                    if (FMath::Abs(Delta.Z) <= 140.f && DistanceSquared < BestDistanceSquared
                        && FVector::DotProduct(Facing, Delta.GetSafeNormal2D()) > 0.35f)
                    { Target = OtherCharacter; TargetState = OtherState; BestDistanceSquared = DistanceSquared; }
                }
    if (!Target || !TargetState)
    { ShovingPlayer->ClientShoveFeedback(2, FString()); return; }
    if (TargetState->ShoveProtectedUntilServerTime > GS->GetServerWorldTimeSeconds())
    { ShovingPlayer->ClientShoveFeedback(4, FString()); return; }
    FCollisionQueryParams ObstacleQuery(SCENE_QUERY_STAT(ExplorerRaceShove), false);
    ObstacleQuery.AddIgnoredActor(ShovingCharacter);
    ObstacleQuery.AddIgnoredActor(Target);
    if (GetWorld()->LineTraceTestByChannel(Origin + FVector(0.f, 0.f, 65.f),
        Target->GetActorLocation() + FVector(0.f, 0.f, 65.f), ECC_Visibility, ObstacleQuery))
    { ShovingPlayer->ClientShoveFeedback(2, FString()); return; }
    const FVector Direction = (Target->GetActorLocation() - Origin).GetSafeNormal2D();
    FVector SlopeBoost = FVector::ZeroVector;
    bool bSlopeImpact = false;
    if (Island)
    {
        const FVector Local = Target->GetActorLocation() - Island->GetActorLocation();
        const FVector Downhill(Island->HeightAt(Local.X - 90.f, Local.Y) - Island->HeightAt(Local.X + 90.f, Local.Y),
            Island->HeightAt(Local.X, Local.Y - 90.f) - Island->HeightAt(Local.X, Local.Y + 90.f), 0.f);
        if (Downhill.Size2D() / 180.f > 0.35f)
        {
            SlopeBoost = Downhill.GetSafeNormal2D() * 150.f;
            bSlopeImpact = !SlopeBoost.IsNearlyZero();
        }
    }
    CancelHeldDig(TargetState, true);
    Target->LaunchCharacter(Direction * 850.f + SlopeBoost + FVector(0.f, 0.f, 170.f), true, true);
    TargetState->ShoveProtectedUntilServerTime = GS->GetServerWorldTimeSeconds() + 1.5f;
    TargetState->ForceNetUpdate();
    ++ShovingState->RoundShoveHits;
    ShovingState->ForceNetUpdate();
    ShovingPlayer->ClientShoveFeedback(1, TargetState->GetPlayerName());
    if (ATreasureSketchPlayerController* TargetController = Cast<ATreasureSketchPlayerController>(TargetState->GetOwner()))
    {
        TargetController->ClientShoveFeedback(3, ShovingState->GetPlayerName());
        if (bSlopeImpact) TargetController->ClientTerrainFeedback(3);
    }
}

void ATreasureSketchGameMode::SetRoundReview(bool bReviewing)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || !GS->bGameStarted || !GS->IsRoundOver()
        || GS->bReviewingRound == bReviewing) return;

    GS->bReviewingRound = bReviewing;
    GS->ForceNetUpdate();
    const TArray<FSketchPage> Pages = bReviewing ? CollectSketchPages() : TArray<FSketchPage>();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
        {
            if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(PC->GetPawn()))
                if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                    PS && PS->PlayerRole == ETreasurePlayerRole::Scout
                    && GS->RoomMode != ETreasureRoomMode::TeamVersus)
                    Character->SetSpectatorHidden(!bReviewing);
            if (bReviewing)
            {
                const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                const FVector ReviewTreasure = GS->RoomMode == ETreasureRoomMode::TeamVersus && PS
                    && PS->VersusTeam >= 0 && PS->VersusTeam <= 1
                    ? VersusTreasureLocations[PS->PlayerRole == ETreasurePlayerRole::Scout
                        ? 1 - PS->VersusTeam : PS->VersusTeam] : TreasureLocation;
                PC->ClientBeginReview(GS->RoundSerial, Pages, ReviewTreasure);
            }
            else PC->ClientEndReview(GS->RoundSerial);
        }
}

void ATreasureSketchGameMode::StartNewRound(bool bSwapRoles, ATreasureSketchPlayerState* RoleRequester)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->bReviewingRound) return;
    if (Island && Island->Tags.Contains(TEXT("SoloFullFlowTest")))
    {
        StartSoloTest(-3);
        return;
    }
    if (Island && Island->Tags.Contains(TEXT("SoloHunterGameplayTest")))
    {
        StartSoloTest(-2);
        return;
    }

    if (!GS->IsRoundOver() || GS->PlayerArray.Num() < 2) return;
    if (GS->RoomMode == ETreasureRoomMode::ExplorerRace)
    {
        if (GS->RaceRoundIndex >= GS->RaceTotalRounds)
        {
            CurrentRaceSeriesId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
            GS->RaceRoundIndex = 1;
            GS->RaceTotalRounds = GS->PlayerArray.Num();
            for (APlayerState* State : GS->PlayerArray)
                if (ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
                {
                    Member->RacePoints = 0;
                    Member->RaceFinds = 0;
                    Member->RaceLastRoundPoints = 0;
                    Member->RoundShoveHits = 0;
                    Member->ForceNetUpdate();
                }
        }
        else ++GS->RaceRoundIndex;
        GS->RaceRoundWinner.Reset();
        for (APlayerState* State : GS->PlayerArray)
            if (ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
            {
                Member->NextShoveServerTime = 0.f;
                Member->ShoveProtectedUntilServerTime = 0.f;
                Member->RaceBestMissDistance = TNumericLimits<float>::Max();
                Member->RaceFarMisses = 0;
                Member->RaceLastRoundPoints = 0;
                Member->RoundShoveHits = 0;
                Member->ForceNetUpdate();
            }
        bSwapRoles = true;
    }
    if (GS->RoomMode == ETreasureRoomMode::TeamVersus && bSwapRoles)
    {
        for (APlayerState* State : GS->PlayerArray)
            if (ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
            {
                Member->PlayerRole = Member->PlayerRole == ETreasurePlayerRole::Scout
                    ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
                Member->ForceNetUpdate();
            }
        bSwapRoles = false;
    }
    if (bSwapRoles)
    {
        if (!GS->IsRoundOver()) return;
        const ETreasurePlayerRole SingleRole = GS->RoomMode == ETreasureRoomMode::OneExplorer
            ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
        ATreasureSketchPlayerState* CurrentSingle = nullptr;
        for (APlayerState* State : GS->PlayerArray)
            if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS->PlayerRole == SingleRole)
            { CurrentSingle = PS; break; }
        if (!CurrentSingle) return;
        const int32 CurrentIndex = GS->PlayerArray.IndexOfByKey(CurrentSingle);
        ATreasureSketchPlayerState* Preferred = GS->RoomMode != ETreasureRoomMode::ExplorerRace
            && RoleRequester && GS->PlayerArray.Contains(RoleRequester) && RoleRequester->PlayerRole != SingleRole
            ? RoleRequester : nullptr;
        if (!Preferred)
            for (int32 Offset = 1; Offset < GS->PlayerArray.Num(); ++Offset)
                if (ATreasureSketchPlayerState* Next = Cast<ATreasureSketchPlayerState>(GS->PlayerArray[(CurrentIndex + Offset) % GS->PlayerArray.Num()]))
                { Preferred = Next; break; }
        if (Preferred) NormalizeRoomRoles(nullptr, Preferred);
    }

    const bool bWasStarted = GS->bGameStarted;
    if (Island) Island->Destroy();
    BuildRound();
    GS->bGameStarted = bWasStarted;

    PlaceRoundPlayers();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            PC->ClientStartNewRound(GS->RoundSerial);
    RevealTreasureToScout();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_REPLAY New round started with seed=%d roles_swapped=%d"),
        IslandSeed, bSwapRoles ? 1 : 0);
}
