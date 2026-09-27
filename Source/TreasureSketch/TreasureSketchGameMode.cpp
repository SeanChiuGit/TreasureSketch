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

FVector ATreasureSketchGameMode::FindPlayerSpawn(TArray<FVector>& UsedSpawns) const
{
    FVector Result = Island ? Island->FindSpawnPoint() : FVector(-3800.f, 0.f, 500.f);
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
        Island->Theme = AProceduralIsland::SelectThemeFromTable(IslandSeed);
    Island->ConfigureThemeParameters();
    Island->FinishSpawning(FTransform::Identity);

    TreasureLocation = Island->FindRandomLandPoint(Stream,
        Island->Theme == EIslandTheme::MistForest ? 105.f : 170.f) + FVector(0.f, 0.f, 35.f);
    if (ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>())
    {
        GS->IslandSeed = IslandSeed;
        ++GS->RoundSerial;
        GS->Phase = ETreasureRoundPhase::ScoutDrawing;
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + GS->DrawingDurationSeconds;
        GS->bGameStarted = false;
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_ROUND_READY Seed=%d Theme=%s Shape=%s Treasure=%s"),
        IslandSeed, *Island->GetThemeName(), *Island->GetShapeName(), *TreasureLocation.ToCompactString());
}

bool ATreasureSketchGameMode::SelectRoomMode(ETreasureRoomMode Mode)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || GS->bGameStarted || Mode != ETreasureRoomMode::OneMapmaker) return false;
    GS->RoomMode = Mode;
    GS->ForceNetUpdate();
    return true;
}

bool ATreasureSketchGameMode::SetRoomMapScale(float Scale)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!HasAuthority() || !GS || GS->bGameStarted || GetNetMode() == NM_DedicatedServer
        || !FMath::IsFinite(Scale) || Scale < GS->MinMapScale || Scale > GS->MaxMapScale) return false;
    GS->RoomMapScale = Scale;
    // Each theme applies this multiplier to its native extent and mesh spacing.
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
            || GS->RoomMode != ETreasureRoomMode::OneMapmaker)
        {
            UE_LOG(LogTemp, Warning, TEXT("TREASURE_ONLINE_START waiting for second player"));
            return;
        }
        int32 Scouts = 0, Hunters = 0;
        for (APlayerState* State : GS->PlayerArray)
            if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
            { Scouts += PS->PlayerRole == ETreasurePlayerRole::Scout; Hunters += PS->PlayerRole == ETreasurePlayerRole::Hunter; }
        if (Scouts != 1 || Hunters != GS->PlayerArray.Num() - 1) return;
        // Lobby previews may have been built before the host changed map size.
        if (!Island || !FMath::IsNearlyEqual(Island->MapScale, GS->RoomMapScale))
        {
            HideTreasureFromScout();
            if (Island) Island->Destroy();
            BuildRound();
            for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
                if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
                    PC->ClientStartNewRound(GS->RoundSerial);
            RevealTreasureToScout();
        }
        TArray<FVector> PlayerSpawns;
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get()))
                if (APawn* Pawn = PC->GetPawn())
                    Pawn->SetActorLocation(FindPlayerSpawn(PlayerSpawns), false, nullptr, ETeleportType::ResetPhysics);
        GS->bGameStarted = true;
        GS->ForceNetUpdate();
        GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + GS->DrawingDurationSeconds;
        UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_START players=%d grid=%d drawing=%d searching=%d"),
            GS->PlayerArray.Num(), Island->GridSize, GS->DrawingDurationSeconds, GS->SearchingDurationSeconds);
    }
}

void ATreasureSketchGameMode::StartSoloTest(int32 ThemeChoice)
{
    if (!HasAuthority()) return;
    ResetSurfacePaint();
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
    bool bHasScout = false;
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS != DepartingState)
            bHasScout |= PS->PlayerRole == ETreasurePlayerRole::Scout;
    if (!bHasScout)
        for (APlayerState* State : GS->PlayerArray)
            if (ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State); PS && PS != DepartingState)
            { PS->PlayerRole = ETreasurePlayerRole::Scout; PS->ForceNetUpdate(); break; }
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

    bool bScoutAssigned = false;
    for (APlayerState* PS : GameState->PlayerArray)
    {
        if (const ATreasureSketchPlayerState* TreasurePS = Cast<ATreasureSketchPlayerState>(PS))
        {
            if (TreasurePS == NewPS) continue;
            bScoutAssigned |= TreasurePS->PlayerRole == ETreasurePlayerRole::Scout;
        }
    }
    NewPS->PlayerRole = !bScoutAssigned ? ETreasurePlayerRole::Scout
        : ETreasurePlayerRole::Hunter;
    NewPS->ForceNetUpdate();
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
        else if (PS->PlayerRole == ETreasurePlayerRole::Hunter && !HunterPC) HunterPC = PC;
    }
    if (ScoutPC)
    {
        int32 Index = 0;
        for (APlayerState* State : GS->PlayerArray)
            if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
                if (PS->PlayerRole == ETreasurePlayerRole::Hunter)
                {
                    if (Index++ == ScoutPC->GetSpectatedHunterIndex())
                        HunterPC = Cast<ATreasureSketchPlayerController>(State->GetOwner());
                }
    }
    if (!ScoutPC || !HunterPC || !HunterPC->GetPawn()) return;

    const FRotator ViewRotation = HunterPC->GetControlRotation();
    const FVector ViewLocation = HunterPC->GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 72.f)
        + FRotator(0.f, ViewRotation.Yaw, 0.f).Vector() * 46.f;
    ScoutPC->ClientUpdateHunterView(ViewLocation, ViewRotation, HunterPC->PlayerState);
}

void ATreasureSketchGameMode::HandoffToHunter(const TArray<FSketchStroke>& SubmittedStrokes)
{
    if (FinishIfTimeExpired()) return;
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    GS->Phase = ETreasureRoundPhase::HunterSearching;
    GS->RoundEndServerTime = GS->GetServerWorldTimeSeconds() + GS->SearchingDurationSeconds;
    HideTreasureFromScout();
    UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_HANDOFF Hunter active; marker hidden"));

    TArray<FVector> HunterSpawns;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
        if (!PC || !PS) continue;
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
            PC->ClientReceiveSketch(SubmittedStrokes);
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

void ATreasureSketchGameMode::StartNewRound(bool bSwapRoles)
{
    ATreasureSketchGameState* GS = GetGameState<ATreasureSketchGameState>();
    if (!GS) return;
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
        ATreasureSketchPlayerState* Scout = nullptr;
        ATreasureSketchPlayerState* Hunter = nullptr;
        for (APlayerState* PS : GS->PlayerArray)
        {
            ATreasureSketchPlayerState* TreasurePS = Cast<ATreasureSketchPlayerState>(PS);
            if (!TreasurePS) continue;
            if (TreasurePS->PlayerRole == ETreasurePlayerRole::Scout) Scout = TreasurePS;
            else if (TreasurePS->PlayerRole == ETreasurePlayerRole::Hunter && !Hunter) Hunter = TreasurePS;
        }
        if (!Scout || !Hunter)
        {
            UE_LOG(LogTemp, Warning, TEXT("TREASURE_SKETCH_REPLAY Role swap needs one scout and one hunter"));
            return;
        }
        const int32 ScoutIndex = GS->PlayerArray.IndexOfByKey(Scout);
        for (int32 Offset = 1; Offset < GS->PlayerArray.Num(); ++Offset)
            if (ATreasureSketchPlayerState* Candidate = Cast<ATreasureSketchPlayerState>(GS->PlayerArray[(ScoutIndex + Offset) % GS->PlayerArray.Num()]))
                if (Candidate->PlayerRole == ETreasurePlayerRole::Hunter) { Hunter = Candidate; break; }
        Scout->PlayerRole = ETreasurePlayerRole::Hunter;
        Hunter->PlayerRole = ETreasurePlayerRole::Scout;
        Scout->ForceNetUpdate();
        Hunter->ForceNetUpdate();
    }

    const bool bWasStarted = GS->bGameStarted;
    if (Island) Island->Destroy();
    BuildRound();
    GS->bGameStarted = bWasStarted;

    TArray<FVector> PlayerSpawns;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(It->Get());
        if (!PC) continue;

        if (APawn* Pawn = PC->GetPawn())
        {
            const FVector SpawnLocation = FindPlayerSpawn(PlayerSpawns);
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
