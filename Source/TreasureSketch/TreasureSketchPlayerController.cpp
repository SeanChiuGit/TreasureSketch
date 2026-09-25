#include "TreasureSketchPlayerController.h"

#include "TreasureSketchGameMode.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureSketchCharacter.h"
#include "TreasureMarker.h"
#include "TreasureOnlineSubsystem.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

ATreasureSketchPlayerController::ATreasureSketchPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void ATreasureSketchPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("Map", IE_Pressed, this, &ATreasureSketchPlayerController::ToggleMap);
    InputComponent->BindAction("Handoff", IE_Pressed, this, &ATreasureSketchPlayerController::Handoff);
    InputComponent->BindAction("Dig", IE_Pressed, this, &ATreasureSketchPlayerController::Dig);
    InputComponent->BindAction("ClearSketch", IE_Pressed, this, &ATreasureSketchPlayerController::ClearSketch);
    InputComponent->BindAction("NewRound", IE_Pressed, this, &ATreasureSketchPlayerController::NewRound);
    InputComponent->BindAction("HostOnline", IE_Pressed, this, &ATreasureSketchPlayerController::HostOnlineGame);
    InputComponent->BindAction("JoinOnline", IE_Pressed, this, &ATreasureSketchPlayerController::JoinOnlineGame);
    InputComponent->BindAction("ConfirmJoin", IE_Pressed, this, &ATreasureSketchPlayerController::ConfirmJoinOnlineGame);
    InputComponent->BindAction("StartOnlineRound", IE_Pressed, this, &ATreasureSketchPlayerController::StartOnlineRound);
    InputComponent->BindAction("InviteSteamFriend", IE_Pressed, this, &ATreasureSketchPlayerController::InviteSteamFriend);
    InputComponent->BindAction("ToggleSpectatorView", IE_Pressed, this, &ATreasureSketchPlayerController::ToggleSpectatorView);
    InputComponent->BindAction("ToggleTreasureMarker", IE_Pressed, this, &ATreasureSketchPlayerController::ToggleSpectatorTreasure);
}

FVector2D ATreasureSketchPlayerController::GetPaperMin() const
{
    int32 W = 1280, H = 720; GetViewportSize(W, H);
    return FVector2D(W * 0.08f, H * 0.07f);
}

FVector2D ATreasureSketchPlayerController::GetPaperSize() const
{
    int32 W = 1280, H = 720; GetViewportSize(W, H);
    return FVector2D(W * 0.84f, H * 0.86f);
}

bool ATreasureSketchPlayerController::IsPointOnPaper(const FVector2D& Point) const
{
    const FVector2D Min = GetPaperMin(), Max = Min + GetPaperSize();
    return Point.X >= Min.X && Point.Y >= Min.Y && Point.X <= Max.X && Point.Y <= Max.Y;
}

void ATreasureSketchPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    UpdateReplayInput();
    UpdateSpectatorCamera(DeltaTime);
    ApplyPhaseInputRules();
    if (!StatusMessage.IsEmpty() && GetWorld()->GetTimeSeconds() >= StatusUntil)
        StatusMessage.Empty();
    if (!bMapOpen || !IsLocalScout()) { bWasDrawing = false; return; }

    float X = 0.f, Y = 0.f;
    const bool bPressed = IsInputKeyDown(EKeys::LeftMouseButton) && GetMousePosition(X, Y) && IsPointOnPaper(FVector2D(X,Y));
    if (bPressed)
    {
        if (!bWasDrawing) Strokes.AddDefaulted();
        const FVector2D Normalized = (FVector2D(X,Y) - GetPaperMin()) / GetPaperSize();
        if (Strokes.Last().Points.IsEmpty() || FVector2D::Distance(Strokes.Last().Points.Last(), Normalized) > 0.003f)
        {
            Strokes.Last().Points.Add(Normalized);
        }
    }
    bWasDrawing = bPressed;
}

bool ATreasureSketchPlayerController::IsLocalScout() const
{
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    return PS && PS->PlayerRole == ETreasurePlayerRole::Scout;
}

bool ATreasureSketchPlayerController::IsHunterWaiting() const
{
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    return PS && GS && PS->PlayerRole == ETreasurePlayerRole::Hunter && GS->Phase == ETreasureRoundPhase::ScoutDrawing;
}

void ATreasureSketchPlayerController::ApplyPhaseInputRules()
{
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (!PS || !GS) return;
    const bool bShouldWait = (PS->PlayerRole == ETreasurePlayerRole::Hunter && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
        || (PS->PlayerRole == ETreasurePlayerRole::Scout && GS->Phase != ETreasureRoundPhase::ScoutDrawing)
        || GS->IsRoundOver();
    const bool bShouldLock = bMapOpen || bShouldWait;
    if (bInputLocked != bShouldLock)
    {
        SetIgnoreMoveInput(bShouldLock);
        SetIgnoreLookInput(bShouldLock);
        bInputLocked = bShouldLock;
    }
}

void ATreasureSketchPlayerController::UpdateReplayInput()
{
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    const bool bRoundOver = GS && GS->IsRoundOver();
    if (bReplayInputActive == bRoundOver) return;

    bReplayInputActive = bRoundOver;
    bMapOpen = false;
    bWasDrawing = false;
    bShowMouseCursor = bRoundOver;
    if (bRoundOver)
    {
        FInputModeGameAndUI ReplayInput;
        ReplayInput.SetHideCursorDuringCapture(false);
        ReplayInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(ReplayInput);
        int32 W = 1280, H = 720;
        GetViewportSize(W, H);
        SetMouseLocation(W / 2, H / 2);
    }
    else
    {
        SetInputMode(FInputModeGameOnly());
    }
}

ATreasureSketchCharacter* ATreasureSketchPlayerController::FindHunterCharacter() const
{
    for (TActorIterator<ATreasureSketchCharacter> It(GetWorld()); It; ++It)
    {
        const ATreasureSketchPlayerState* PS = It->GetPlayerState<ATreasureSketchPlayerState>();
        if (PS && PS->PlayerRole == ETreasurePlayerRole::Hunter) return *It;
    }
    return nullptr;
}

void ATreasureSketchPlayerController::StartSpectating()
{
    FVector StartLocation = GetPawn() ? GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 300.f)
        : FVector(0.f, 0.f, 500.f);
    FRotator StartRotation = GetControlRotation();
    if (ATreasureSketchCharacter* Hunter = FindHunterCharacter())
    {
        StartLocation = Hunter->GetActorLocation() + FVector(-400.f, 0.f, 350.f);
        StartRotation = (Hunter->GetActorLocation() - StartLocation).Rotation();
    }
    SpectatorCamera = GetWorld()->SpawnActor<ACameraActor>(
        ACameraActor::StaticClass(), StartLocation, StartRotation);
    if (!SpectatorCamera) return;

    SpectatorCamera->SetActorEnableCollision(false);
    SpectatorView = EScoutSpectatorView::FreeFlight;
    bMapOpen = false;
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
    SetViewTargetWithBlend(SpectatorCamera.Get(), 0.2f);
}

void ATreasureSketchPlayerController::StopSpectating()
{
    if (!SpectatorCamera) return;
    if (GetPawn()) SetViewTarget(GetPawn());
    SpectatorCamera->Destroy();
    SpectatorCamera = nullptr;
    bHasHunterView = false;
    SetLocalTreasureMarkerVisible(false);
}

void ATreasureSketchPlayerController::UpdateSpectatorCamera(float DeltaTime)
{
    if (!IsLocalController()) return;
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (GS && PendingSpectatorRoundSerial > 0 && GS->RoundSerial >= PendingSpectatorRoundSerial)
        PendingSpectatorRoundSerial = 0;
    const bool bShouldSpectate = GS && GS->bGameStarted && PendingSpectatorRoundSerial == 0
        && GS->Phase == ETreasureRoundPhase::HunterSearching && IsLocalScout();
    if (!bShouldSpectate)
    {
        StopSpectating();
        return;
    }
    if (!SpectatorCamera) StartSpectating();
    if (!SpectatorCamera) return;

    if (SpectatorView == EScoutSpectatorView::HunterFirstPerson)
    {
        if (bHasHunterView && GetWorld()->GetTimeSeconds() - HunterViewUpdatedAt < 1.f)
        {
            SpectatorCamera->SetActorLocation(FMath::VInterpTo(
                SpectatorCamera->GetActorLocation(), HunterViewLocation, DeltaTime, 18.f));
            SpectatorCamera->SetActorRotation(FMath::RInterpTo(
                SpectatorCamera->GetActorRotation(), HunterViewRotation, DeltaTime, 18.f));
        }
        else if (ATreasureSketchCharacter* Hunter = FindHunterCharacter())
        {
            const FRotator Aim = Hunter->GetBaseAimRotation();
            SpectatorCamera->SetActorLocation(Hunter->GetActorLocation() + FVector(0.f, 0.f, 72.f)
                + FRotator(0.f, Aim.Yaw, 0.f).Vector() * 46.f);
            SpectatorCamera->SetActorRotation(Aim);
        }
        return;
    }

    float MouseX = 0.f, MouseY = 0.f;
    GetInputMouseDelta(MouseX, MouseY);
    FRotator Rotation = SpectatorCamera->GetActorRotation();
    Rotation.Yaw += MouseX * 0.15f;
    Rotation.Pitch = FMath::ClampAngle(Rotation.Pitch - MouseY * 0.15f, -85.f, 85.f);
    Rotation.Roll = 0.f;
    SpectatorCamera->SetActorRotation(Rotation);

    const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
    FVector Direction = YawRotation.Vector() * (static_cast<int32>(IsInputKeyDown(EKeys::W)) - static_cast<int32>(IsInputKeyDown(EKeys::S)));
    Direction += FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y)
        * (static_cast<int32>(IsInputKeyDown(EKeys::D)) - static_cast<int32>(IsInputKeyDown(EKeys::A)));
    Direction.Z += static_cast<int32>(IsInputKeyDown(EKeys::SpaceBar))
        - static_cast<int32>(IsInputKeyDown(EKeys::LeftControl));
    const float Speed = IsInputKeyDown(EKeys::LeftShift) ? 2200.f : 1100.f;
    SpectatorCamera->AddActorWorldOffset(Direction.GetSafeNormal() * Speed * DeltaTime);
}

void ATreasureSketchPlayerController::ToggleSpectatorView()
{
    if (!SpectatorCamera) return;
    SpectatorView = SpectatorView == EScoutSpectatorView::FreeFlight
        ? EScoutSpectatorView::HunterFirstPerson : EScoutSpectatorView::FreeFlight;
    if (SpectatorView == EScoutSpectatorView::HunterFirstPerson && bHasHunterView)
        SpectatorCamera->SetActorLocationAndRotation(HunterViewLocation, HunterViewRotation);
}

void ATreasureSketchPlayerController::ToggleSpectatorTreasure()
{
    if (SpectatorCamera && IsLocalScout() && bHasScoutTreasureLocation)
        SetLocalTreasureMarkerVisible(!bTreasureMarkerVisible);
}

void ATreasureSketchPlayerController::SetLocalTreasureMarkerVisible(bool bVisible)
{
    bTreasureMarkerVisible = bVisible && bHasScoutTreasureLocation;
    if (!bTreasureMarkerVisible)
    {
        if (LocalScoutMarker) LocalScoutMarker->Destroy();
        LocalScoutMarker = nullptr;
    }
    else if (!LocalScoutMarker)
    {
        LocalScoutMarker = GetWorld()->SpawnActor<ATreasureMarker>(
            ATreasureMarker::StaticClass(), ScoutTreasureLocation, FRotator::ZeroRotator);
    }
}

void ATreasureSketchPlayerController::ClientUpdateHunterView_Implementation(
    FVector_NetQuantize ViewLocation, FRotator ViewRotation)
{
    HunterViewLocation = ViewLocation;
    HunterViewRotation = ViewRotation;
    HunterViewUpdatedAt = GetWorld()->GetTimeSeconds();
    bHasHunterView = true;
}

void ATreasureSketchPlayerController::ToggleMap()
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if ((GS && GS->IsRoundOver()) || IsHunterWaiting()
        || (IsLocalScout() && GS && GS->Phase != ETreasureRoundPhase::ScoutDrawing))
        return;
    bMapOpen = !bMapOpen;
    bShowMouseCursor = bMapOpen;
    ApplyPhaseInputRules();
    if (bMapOpen)
    {
        FInputModeGameAndUI DrawingInput;
        DrawingInput.SetHideCursorDuringCapture(false);
        DrawingInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(DrawingInput);
        const FVector2D PaperCenter = GetPaperMin() + GetPaperSize() * 0.5f;
        SetMouseLocation(FMath::RoundToInt(PaperCenter.X), FMath::RoundToInt(PaperCenter.Y));
        UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_DRAWING_FOCUS Board focused; camera and movement locked"));
    }
    else
    {
        SetInputMode(FInputModeGameOnly());
        UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_DRAWING_FOCUS Exploration controls restored"));
    }
}

void ATreasureSketchPlayerController::Handoff()
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (IsLocalScout() && GS && GS->bGameStarted && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
    {
        SetLocalTreasureMarkerVisible(false);
        ServerSubmitSketch(Strokes);
        bMapOpen = false;
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());
        ApplyPhaseInputRules();
        StatusMessage = TEXT("地图已交给寻宝者；现在可以观战。");
        StatusUntil = GetWorld()->GetTimeSeconds() + 5.f;
    }
}

void ATreasureSketchPlayerController::ServerSubmitSketch_Implementation(const TArray<FSketchStroke>& CompletedStrokes)
{
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    if (!PS || PS->PlayerRole != ETreasurePlayerRole::Scout) return;
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->HandoffToHunter(CompletedStrokes);
}

void ATreasureSketchPlayerController::ClientReceiveSketch_Implementation(const TArray<FSketchStroke>& CompletedStrokes)
{
    Strokes = CompletedStrokes;
    StatusMessage = TEXT("侦察者的地图已送达！按 M 查看，按 E 挖掘。");
    StatusUntil = GetWorld()->GetTimeSeconds() + 8.f;
}

void ATreasureSketchPlayerController::Dig()
{
    if (!GetPawn()) return;
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    if (PS && PS->PlayerRole == ETreasurePlayerRole::Hunter)
        ServerTryDig(GetPawn()->GetActorLocation());
}

void ATreasureSketchPlayerController::ServerTryDig_Implementation(FVector_NetQuantize WorldLocation)
{
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    if (!PS || PS->PlayerRole != ETreasurePlayerRole::Hunter) return;
    float Distance = 0.f;
    bool bFound = false;
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        bFound = GM->TryDig(WorldLocation, Distance);
    ClientDigResult(bFound, Distance);
}

void ATreasureSketchPlayerController::ClientDigResult_Implementation(bool bFound, float Distance)
{
    StatusMessage = bFound ? TEXT("找到宝箱！合作成功！")
        : FString::Printf(TEXT("这里没有宝箱（误差 %.0f 米）。继续参照地图寻找。"), Distance / 100.f);
    StatusUntil = GetWorld()->GetTimeSeconds() + 6.f;
}

void ATreasureSketchPlayerController::ClientStartNewRound_Implementation(int32 NewRoundSerial)
{
    StopSpectating();
    PendingSpectatorRoundSerial = NewRoundSerial;
    bHasScoutTreasureLocation = false;
    SetLocalTreasureMarkerVisible(false);
    Strokes.Reset();
    bWasDrawing = false;
    StatusMessage = TEXT("新的一局开始了！");
    StatusUntil = GetWorld()->GetTimeSeconds() + 4.f;
}

void ATreasureSketchPlayerController::ClientRevealTreasure_Implementation(FVector_NetQuantize TreasureLocation)
{
    ScoutTreasureLocation = TreasureLocation;
    bHasScoutTreasureLocation = true;
    if (LocalScoutMarker) LocalScoutMarker->SetActorLocation(ScoutTreasureLocation);
    SetLocalTreasureMarkerVisible(true);
}

void ATreasureSketchPlayerController::ClientHideTreasure_Implementation()
{
    SetLocalTreasureMarkerVisible(false);
}

void ATreasureSketchPlayerController::ClearSketch()
{
    if (IsLocalScout()) Strokes.Reset();
}

void ATreasureSketchPlayerController::NewRound()
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) GM->StartNewRound();
}

void ATreasureSketchPlayerController::RequestReplay(bool bSwapRoles)
{
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (GS && GS->IsRoundOver())
        ServerRequestReplay(bSwapRoles);
}

void ATreasureSketchPlayerController::ServerRequestReplay_Implementation(bool bSwapRoles)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->IsRoundOver()) return;
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->StartNewRound(bSwapRoles);
}

void ATreasureSketchPlayerController::HostOnlineGame()
{
    if (UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>()) Online->HostGame();
}

void ATreasureSketchPlayerController::JoinOnlineGame()
{
    if (UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>()) Online->FindAndJoinGame();
}

void ATreasureSketchPlayerController::ConfirmJoinOnlineGame()
{
    if (UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>()) Online->JoinFirstFoundGame();
}

void ATreasureSketchPlayerController::StartOnlineRound()
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) GM->StartHostedRound();
}

void ATreasureSketchPlayerController::InviteSteamFriend()
{
    if (UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>()) Online->OpenSteamInviteUI();
}
