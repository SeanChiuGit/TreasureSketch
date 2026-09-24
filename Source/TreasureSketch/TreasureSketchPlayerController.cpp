#include "TreasureSketchPlayerController.h"

#include "TreasureSketchGameMode.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureOnlineSubsystem.h"
#include "Engine/Engine.h"

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
}

FVector2D ATreasureSketchPlayerController::GetPaperMin() const
{
    int32 W = 1280, H = 720; GetViewportSize(W, H);
    return FVector2D(W * 0.12f, H * 0.10f);
}

FVector2D ATreasureSketchPlayerController::GetPaperSize() const
{
    int32 W = 1280, H = 720; GetViewportSize(W, H);
    return FVector2D(W * 0.76f, H * 0.80f);
}

bool ATreasureSketchPlayerController::IsPointOnPaper(const FVector2D& Point) const
{
    const FVector2D Min = GetPaperMin(), Max = Min + GetPaperSize();
    return Point.X >= Min.X && Point.Y >= Min.Y && Point.X <= Max.X && Point.Y <= Max.Y;
}

void ATreasureSketchPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    ApplyPhaseInputRules();
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
    if (!PS || !GS || bMapOpen) return;
    const bool bShouldWait = (PS->PlayerRole == ETreasurePlayerRole::Hunter && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
        || (PS->PlayerRole == ETreasurePlayerRole::Scout && GS->Phase != ETreasureRoundPhase::ScoutDrawing);
    SetIgnoreMoveInput(bShouldWait);
    SetIgnoreLookInput(bShouldWait);
}

void ATreasureSketchPlayerController::ToggleMap()
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (IsHunterWaiting() || (IsLocalScout() && GS && GS->Phase != ETreasureRoundPhase::ScoutDrawing))
        return;
    bMapOpen = !bMapOpen;
    bShowMouseCursor = bMapOpen;
    SetIgnoreLookInput(bMapOpen);
    SetIgnoreMoveInput(bMapOpen);
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
    if (IsLocalScout() && GS && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
    {
        ServerSubmitSketch(Strokes);
        bMapOpen = false;
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());
        StatusMessage = TEXT("地图已交给寻宝者，请等待对方寻宝。");
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

void ATreasureSketchPlayerController::ClearSketch()
{
    if (IsLocalScout()) Strokes.Reset();
}

void ATreasureSketchPlayerController::NewRound()
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) GM->StartNewRound();
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
