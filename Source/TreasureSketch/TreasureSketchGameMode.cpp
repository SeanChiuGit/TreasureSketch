#include "TreasureSketchGameMode.h"

#include "ProceduralIsland.h"
#include "TreasureSurfacePaint.h"
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
    if (bScoutTimedOut)
    {
        BeginHunterSearching(CollectSketchPages());
        UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_AUTO_HANDOFF Drawing time expired"));
        return true;
    }
    GS->Phase = ETreasureRoundPhase::HunterTimedOut;
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

void ATreasureSketchGameMode::ResetSurfacePaint()
{
    if (SurfacePaint) SurfacePaint->Destroy();
    SurfacePaint = nullptr;
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
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (FinishIfTimeExpired() || !GS || !GS->bGameStarted || !GS->bSurfacePaintEnabled
        || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    if (!SurfacePaint) SurfacePaint = GetWorld()->SpawnActor<ATreasureSurfacePaint>();
    if (SurfacePaint) SurfacePaint->AddStamp(Hit);
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
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        GS->IslandSeed = IslandSeed;
        ++GS->RoundSerial;
        GS->Phase = ETreasureRoundPhase::ScoutDrawing;
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
        || (Mode != ETreasureRoomMode::OneMapmaker && Mode != ETreasureRoomMode::OneExplorer)) return false;
    if (GS->RoomMode == Mode) return true;
    ATreasureSketchPlayerState* SinglePlayer = nullptr;
    const ETreasurePlayerRole PreviousSingleRole = GS->RoomMode == ETreasureRoomMode::OneExplorer
        ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
    for (APlayerState* State : GS->PlayerArray)
        if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS->PlayerRole == PreviousSingleRole)
        { SinglePlayer = PS; break; }
    HideTreasureFromScout();
    GS->RoomMode = Mode;
    NormalizeRoomRoles(nullptr, SinglePlayer);
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
                    ? Island->FindSpawnPoint() : FindPlayerSpawn(UsedSpawns);
                if (bScout) { bFirstScoutPlaced = true; MapmakerLandingSpawns.Add(Spawn); }
                Pawn->SetActorLocation(Spawn, false, nullptr, ETeleportType::ResetPhysics);
                if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(Pawn)) Character->SetSpectatorHidden(false);
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
    if (!HasAuthority() || !GS || !GS->IsRoundOver()) return;
    HideTreasureFromScout();
    if (Island) Island->Destroy();
    BuildRound();
    GS->ForceNetUpdate();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
        {
            if (ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(PC->GetPawn()))
                Character->SetSpectatorHidden(false);
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
        if (GS->PlayerArray.Num() < 2 || GS->PlayerArray.Num() > ATreasureSketchGameState::MaxRoomPlayers
            || (GS->RoomMode != ETreasureRoomMode::OneMapmaker && GS->RoomMode != ETreasureRoomMode::OneExplorer))
        {
            UE_LOG(LogTemp, Warning, TEXT("TREASURE_ONLINE_START waiting for second player"));
            return;
        }
        int32 Scouts = 0, Hunters = 0;
        for (APlayerState* State : GS->PlayerArray)
            if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
            { Scouts += PS->PlayerRole == ETreasurePlayerRole::Scout; Hunters += PS->PlayerRole == ETreasurePlayerRole::Hunter; }
        const int32 ExpectedScouts = GS->RoomMode == ETreasureRoomMode::OneExplorer ? GS->PlayerArray.Num() - 1 : 1;
        if (Scouts != ExpectedScouts || Hunters != GS->PlayerArray.Num() - ExpectedScouts) return;
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
                    PS && PS->PlayerRole == ETreasurePlayerRole::Hunter)
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
            PC->GetPawn()->SetActorLocation(Island->FindSpawnPoint(), false, nullptr, ETeleportType::ResetPhysics);
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
    if (!GS || GS->RoomMode != ETreasureRoomMode::OneMapmaker) return;
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
            { PS->bSketchSubmitted = false; PS->ForceNetUpdate(); }
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
                    Page.Strokes = PC->GetServerDrawing();
            }
            if (Page.MapmakerName.IsEmpty()) Page.MapmakerName = FString::Printf(TEXT("地图师 %d"), Pages.Num() + 1);
            Pages.Add(MoveTemp(Page));
        }
    return Pages;
}

void ATreasureSketchGameMode::SubmitPlayerSketch(ATreasureSketchPlayerState* Scout, const TArray<FSketchStroke>& SubmittedStrokes)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || !Scout || !GS->PlayerArray.Contains(Scout) || Scout->PlayerRole != ETreasurePlayerRole::Scout || Scout->bSketchSubmitted) return;
    FSketchPage Page;
    Page.MapmakerId = Scout->GetPlayerId();
    Page.MapmakerName = Scout->GetPlayerName();
    Page.Strokes = SubmittedStrokes;
    SubmittedSketches.Add(Scout->GetPlayerId(), MoveTemp(Page));
    Scout->bSketchSubmitted = true;
    Scout->ForceNetUpdate();
    if (const FSketchPage* Submitted = SubmittedSketches.Find(Scout->GetPlayerId()))
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
                if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                    PS && PS->PlayerRole == ETreasurePlayerRole::Hunter)
                    PC->ClientReplaceLiveSketch(GS->RoundSerial, *Submitted);
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
            if (PS->PlayerRole == ETreasurePlayerRole::Scout && !PS->bSketchSubmitted) return;
    BeginHunterSearching(CollectSketchPages());
}

void ATreasureSketchGameMode::BroadcastSketchDelta(ATreasureSketchPlayerState* Scout, int32 StrokeIndex, const TArray<FVector2D>& Points)
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing || !Scout
        || Scout->PlayerRole != ETreasurePlayerRole::Scout || Scout->bSketchSubmitted) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                PS && PS->PlayerRole == ETreasurePlayerRole::Hunter)
                PC->ClientAppendLiveSketch(GS->RoundSerial, Scout->GetPlayerId(), StrokeIndex, Points);
}

void ATreasureSketchGameMode::BroadcastSketchClear(ATreasureSketchPlayerState* Scout)
{
    const ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing || !Scout
        || Scout->PlayerRole != ETreasurePlayerRole::Scout || Scout->bSketchSubmitted) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
            if (const ATreasureSketchPlayerState* PS = PC->GetPlayerState<ATreasureSketchPlayerState>();
                PS && PS->PlayerRole == ETreasurePlayerRole::Hunter)
                PC->ClientClearLiveSketch(GS->RoundSerial, Scout->GetPlayerId());
}

void ATreasureSketchGameMode::BeginHunterSearching(const TArray<FSketchPage>& Pages)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    GS->Phase = ETreasureRoundPhase::HunterSearching;
    GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + GS->SearchingDurationSeconds;
    GS->ForceNetUpdate();
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
        }
        else if (PS->PlayerRole == ETreasurePlayerRole::Hunter)
        {
            PC->ClientReceiveSketchPages(GS->RoundSerial, Pages);
            if (APawn* Pawn = PC->GetPawn())
            {
                const FVector SpawnLocation = FindPlayerSpawn(HunterSpawns);
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
                    PS && PS->PlayerRole == ETreasurePlayerRole::Scout)
                    Character->SetSpectatorHidden(!bReviewing);
            if (bReviewing) PC->ClientBeginReview(GS->RoundSerial, Pages, TreasureLocation);
            else PC->ClientEndReview(GS->RoundSerial);
        }
}

void ATreasureSketchGameMode::StartNewRound(bool bSwapRoles)
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
        for (int32 Offset = 1; Offset < GS->PlayerArray.Num(); ++Offset)
            if (ATreasureSketchPlayerState* Next = Cast<ATreasureSketchPlayerState>(GS->PlayerArray[(CurrentIndex + Offset) % GS->PlayerArray.Num()]))
            { NormalizeRoomRoles(nullptr, Next); break; }
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
