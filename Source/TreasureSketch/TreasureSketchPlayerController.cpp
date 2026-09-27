#include "TreasureSketchPlayerController.h"

#include "TreasureSketchGameMode.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureSketchCharacter.h"
#include "TreasureMarker.h"
#include "ProceduralIsland.h"
#include "TreasureOnlineSubsystem.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/SpringArmComponent.h"

int32 ATreasureSketchPlayerController::GetTestSeed() const
{
    int64 Value = 0;
    return LexTryParseString(Value, *TestSeedText) && Value > 0 && Value <= MAX_int32
        ? static_cast<int32>(Value) : 0;
}

bool ATreasureSketchPlayerController::CommitMapScale()
{
    if (!bMapScaleEditing) return true;
    float Scale = 0.f;
    ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>();
    if (!GM || !LexTryParseString(Scale, *MapScaleText) || !GM->SetRoomMapScale(Scale)) return false;
    bMapScaleEditing = false;
    return true;
}

bool ATreasureSketchPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (bMapScaleEditing && IsFrontEndVisible() && (Params.Event == IE_Pressed || Params.Event == IE_Repeat))
    {
        const FKey Digits[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
            EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
        const FKey Numpad[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree,
            EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
        for (int32 Digit = 0; Digit < 10; ++Digit)
            if (Params.Key == Digits[Digit] || Params.Key == Numpad[Digit])
            {
                if (bReplaceMapScaleText) { MapScaleText.Empty(); bReplaceMapScaleText = false; }
                if (MapScaleText.Len() < 7) MapScaleText += FString::FromInt(Digit);
                return true;
            }
        if (Params.Key == EKeys::Period || Params.Key == EKeys::Decimal)
        {
            if (bReplaceMapScaleText) { MapScaleText.Empty(); bReplaceMapScaleText = false; }
            if (!MapScaleText.Contains(TEXT(".")) && MapScaleText.Len() < 7) MapScaleText += TEXT(".");
        }
        else if (Params.Key == EKeys::BackSpace)
        {
            MapScaleText = bReplaceMapScaleText ? FString() : MapScaleText.LeftChop(1);
            bReplaceMapScaleText = false;
        }
        else if (Params.Key == EKeys::Delete) { MapScaleText.Empty(); bReplaceMapScaleText = false; }
        else if (Params.Key == EKeys::Enter) CommitMapScale();
        else if (Params.Key == EKeys::Escape) bMapScaleEditing = false;
        return true;
    }
    if (bTestSeedEditing && IsFrontEndVisible() && FrontEndPage == EFrontEndPage::SoloTest
        && (Params.Event == IE_Pressed || Params.Event == IE_Repeat))
    {
        const FKey Digits[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
            EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
        const FKey Numpad[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree,
            EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
        for (int32 Digit = 0; Digit < 10; ++Digit)
        {
            if (Params.Key == Digits[Digit] || Params.Key == Numpad[Digit])
            {
                if (TestSeedText.Len() < 10) TestSeedText += FString::FromInt(Digit);
                return true;
            }
        }
        if (Params.Key == EKeys::BackSpace) { TestSeedText = TestSeedText.LeftChop(1); return true; }
        if (Params.Key == EKeys::Delete) { TestSeedText.Empty(); return true; }
        if (Params.Key == EKeys::Enter || Params.Key == EKeys::Escape)
        { bTestSeedEditing = false; return true; }
    }
    return Super::InputKey(Params);
}

ATreasureSketchPlayerController::ATreasureSketchPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void ATreasureSketchPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;

    FrontEndPage = GetNetMode() == NM_Standalone ? EFrontEndPage::MainMenu : EFrontEndPage::RoomLobby;
    MenuCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
        FVector(-9800.f, -9800.f, 7600.f), FRotator(-24.f, 45.f, 0.f));
    if (MenuCamera)
    {
        MenuCamera->SetActorRotation((FVector(0.f, 0.f, 350.f) - MenuCamera->GetActorLocation()).Rotation());
        MenuCamera->SetActorEnableCollision(false);
        SetViewTarget(MenuCamera);
    }
    UpdateFrontEnd();
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
    InputComponent->BindAction("ToggleWeatherFog", IE_Pressed, this, &ATreasureSketchPlayerController::ToggleWeatherFog);
}

void ATreasureSketchPlayerController::ToggleWeatherFog()
{
    for (TActorIterator<AProceduralIsland> It(GetWorld()); It; ++It)
    {
        const bool bEnabled = It->ToggleDebugFog();
        StatusMessage = bEnabled ? TEXT("调试天气：迷雾已开启（F 关闭）") : TEXT("调试天气：迷雾已关闭（F 开启）");
        StatusUntil = GetWorld()->GetTimeSeconds() + 4.f;
        break;
    }
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
    UpdateFrontEnd();
    if (IsFrontEndVisible()) return;
    const ATreasureSketchGameState* CursorGS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    const bool bCanSpray = CursorGS && CursorGS->bGameStarted && CursorGS->bSurfacePaintEnabled
        && IsLocalScout() && !HasSubmittedSketch() && CursorGS->Phase == ETreasureRoundPhase::ScoutDrawing && !bMapOpen;
    SetSprayCursorMode(bCanSpray);
    UpdateReplayInput();
    UpdateWaitingSketchInput();
    if (IsScoutSpectating() && WasInputKeyJustPressed(EKeys::Q)) ServerCycleSpectatedHunter();
    UpdateSpectatorCamera(DeltaTime);
    ApplyPhaseInputRules();
    ApplyKeyboardMovementFallback();
    if ((bMapOpen || IsHunterWaiting()) && !IsLocalScout())
    {
        if (WasInputKeyJustPressed(EKeys::Left)) CycleSketchPage(-1);
        if (WasInputKeyJustPressed(EKeys::Right)) CycleSketchPage(1);
    }
    if (!StatusMessage.IsEmpty() && GetWorld()->GetTimeSeconds() >= StatusUntil)
        StatusMessage.Empty();
    const ATreasureSketchGameState* PaintGS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (PaintGS && PaintGS->bGameStarted && PaintGS->bSurfacePaintEnabled && IsLocalScout()
        && !HasSubmittedSketch() && PaintGS->Phase == ETreasureRoundPhase::ScoutDrawing && !bMapOpen
        && IsInputKeyDown(EKeys::RightMouseButton) && GetWorld()->GetTimeSeconds() >= NextSpraySampleTime)
    {
        NextSpraySampleTime = GetWorld()->GetTimeSeconds() + 0.06f;
        FVector Origin; FRotator Rotation;
        GetPlayerViewPoint(Origin, Rotation);
        ServerSpraySurface(Origin, Rotation.Vector());
    }
    if (!IsLocalController()) return;
    if (!bMapOpen || !IsLocalScout() || !PaintGS || !PaintGS->bGameStarted
        || PaintGS->Phase != ETreasureRoundPhase::ScoutDrawing || HasSubmittedSketch())
    {
        FlushDrawingPoints();
        bWasDrawing = false;
        if (PaintGS && PaintGS->Phase == ETreasureRoundPhase::HunterSearching && IsLocalScout())
        {
            bMapOpen = false;
            bShowMouseCursor = false;
            SetInputMode(FInputModeGameOnly());
        }
        return;
    }

    float X = 0.f, Y = 0.f;
    const bool bPressed = IsInputKeyDown(EKeys::LeftMouseButton) && GetMousePosition(X, Y) && IsPointOnPaper(FVector2D(X,Y));
    if (bPressed)
    {
        if (!bWasDrawing)
        {
            FlushDrawingPoints();
            Strokes.AddDefaulted();
        }
        const FVector2D Normalized = (FVector2D(X,Y) - GetPaperMin()) / GetPaperSize();
        if (Strokes.Last().Points.IsEmpty() || FVector2D::Distance(Strokes.Last().Points.Last(), Normalized) > 0.003f)
        {
            Strokes.Last().Points.Add(Normalized);
            PendingDrawingPoints.Add(Normalized);
        }
    }
    bWasDrawing = bPressed;
    if (!bPressed || GetWorld()->GetTimeSeconds() >= NextDrawingSyncTime || PendingDrawingPoints.Num() >= 128)
        FlushDrawingPoints();
}

void ATreasureSketchPlayerController::FlushDrawingPoints()
{
    if (PendingDrawingPoints.IsEmpty()) return;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (GS && GS->bGameStarted && GS->Phase == ETreasureRoundPhase::ScoutDrawing && IsLocalScout() && !HasSubmittedSketch())
        ServerAppendDrawing(GS->RoundSerial, Strokes.Num() - 1, PendingDrawingPoints);
    PendingDrawingPoints.Reset();
    NextDrawingSyncTime = GetWorld()->GetTimeSeconds() + 0.1f;
}

const TArray<FSketchStroke>& ATreasureSketchPlayerController::GetServerDrawing() const
{
    if (IsLocalController()) return Strokes;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    static const TArray<FSketchStroke> EmptyDrawing;
    return GS && ServerDrawingRoundSerial == GS->RoundSerial ? ServerDrawing : EmptyDrawing;
}

void ATreasureSketchPlayerController::ServerAppendDrawing_Implementation(int32 RoundSerial, int32 StrokeIndex, const TArray<FVector2D>& Points)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || GS->RoundSerial != RoundSerial || !IsLocalScout() || HasSubmittedSketch() || StrokeIndex < 0 || Points.Num() > 128) return;
    if (ServerDrawingRoundSerial != RoundSerial)
    {
        ServerDrawing.Reset();
        ServerDrawingRoundSerial = RoundSerial;
    }
    if (StrokeIndex > ServerDrawing.Num()) return;
    for (const FVector2D& Point : Points)
        if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y)
            || Point.X < 0.f || Point.X > 1.f || Point.Y < 0.f || Point.Y > 1.f) return;
    if (StrokeIndex == ServerDrawing.Num()) ServerDrawing.AddDefaulted();
    ServerDrawing[StrokeIndex].Points.Append(Points);
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->BroadcastSketchDelta(GetPlayerState<ATreasureSketchPlayerState>(), StrokeIndex, Points);
}

void ATreasureSketchPlayerController::ServerClearDrawing_Implementation(int32 RoundSerial)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || GS->RoundSerial != RoundSerial || !IsLocalScout() || HasSubmittedSketch()) return;
    ServerDrawing.Reset();
    ServerDrawingRoundSerial = RoundSerial;
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->BroadcastSketchClear(GetPlayerState<ATreasureSketchPlayerState>());
}

bool ATreasureSketchPlayerController::IsFrontEndVisible() const
{
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    return FrontEndPage != EFrontEndPage::None && (!GS || !GS->bGameStarted);
}

void ATreasureSketchPlayerController::UpdateFrontEnd()
{
    const ATreasureSketchGameState* LobbyGS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (FrontEndPage == EFrontEndPage::None && LobbyGS && !LobbyGS->bGameStarted && GetNetMode() != NM_Standalone)
        FrontEndPage = EFrontEndPage::RoomLobby;
    const bool bVisible = IsFrontEndVisible();
    if (bVisible == bFrontEndInputActive) return;
    bFrontEndInputActive = bVisible;
    if (bVisible)
    {
        bShowMouseCursor = true;
        SetIgnoreMoveInput(true);
        SetIgnoreLookInput(true);
        FInputModeGameAndUI MenuInput;
        MenuInput.SetHideCursorDuringCapture(false);
        MenuInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(MenuInput);
    }
    else
    {
        FrontEndPage = EFrontEndPage::None;
        bShowMouseCursor = false;
        ResetIgnoreMoveInput();
        ResetIgnoreLookInput();
        bInputLocked = false;
        bLookInputLocked = false;
        SetInputMode(FInputModeGameOnly());
        if (GetPawn()) SetViewTarget(GetPawn());
        if (MenuCamera) MenuCamera->Destroy();
        MenuCamera = nullptr;
    }
}

void ATreasureSketchPlayerController::ApplyKeyboardMovementFallback()
{
    // UE 5.6 projects using EnhancedPlayerInput can stop forwarding legacy AxisMappings
    // after switching from a GameAndUI menu back to gameplay. Polling the four movement
    // keys here keeps the prototype and packaged builds controllable without an IMC asset.
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn || IsMoveInputIgnored() || bMapOpen) return;

    const float Forward = (IsInputKeyDown(EKeys::W) ? 1.f : 0.f)
        - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
    const float Right = (IsInputKeyDown(EKeys::D) ? 1.f : 0.f)
        - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f);
    const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
    if (!FMath::IsNearlyZero(Forward))
        ControlledPawn->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Forward);
    if (!FMath::IsNearlyZero(Right))
        ControlledPawn->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Right);
}

void ATreasureSketchPlayerController::OpenFrontEndPage(EFrontEndPage NewPage)
{
    FrontEndPage = NewPage;
    bFrontEndInputActive = false;
    UpdateFrontEnd();
}

void ATreasureSketchPlayerController::HandleFrontEndAction(FName ActionName)
{
    if (ActionName == TEXT("PreviousSketchPage") || ActionName == TEXT("NextSketchPage"))
    {
        CycleSketchPage(ActionName == TEXT("NextSketchPage") ? 1 : -1);
        return;
    }
    if (ActionName == TEXT("MapScaleInput"))
    {
        if (!IsLocalController() || (FrontEndPage != EFrontEndPage::RoomLobby && FrontEndPage != EFrontEndPage::SoloTest)
            || !GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) return;
        const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
        if (!GS || GS->bGameStarted) return;
        MapScaleText = FString::Printf(TEXT("%.6g"), GS->RoomMapScale);
        bMapScaleEditing = true;
        bReplaceMapScaleText = true;
        bTestSeedEditing = false;
        return;
    }
    if (ActionName == TEXT("MenuBack") || ActionName == TEXT("RoomBack")) bMapScaleEditing = false;
    if (!CommitMapScale()) return;
    if (ActionName == TEXT("TestSeedInput")) { bTestSeedEditing = true; return; }
    if (ActionName == TEXT("TestSeedClear")) { TestSeedText.Empty(); bTestSeedEditing = false; return; }
    bTestSeedEditing = false;
    if (ActionName == TEXT("MenuCreate"))
    {
        OpenFrontEndPage(EFrontEndPage::RoomLobby);
        HostOnlineGame();
    }
    else if (ActionName == TEXT("MenuJoin"))
    {
        OpenFrontEndPage(EFrontEndPage::JoinBrowser);
        JoinOnlineGame();
    }
    else if (ActionName == TEXT("MenuJoinFirst")) ConfirmJoinOnlineGame();
    else if (ActionName == TEXT("MenuSolo")) OpenFrontEndPage(EFrontEndPage::SoloTest);
    else if (ActionName == TEXT("SoloBeach") || ActionName == TEXT("SoloForest") || ActionName == TEXT("SoloRandom") || ActionName == TEXT("SoloHunter") || ActionName == TEXT("SoloFullFlow"))
    {
        if (ATreasureSketchGameMode* GameMode = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        {
            const int32 ThemeChoice = ActionName == TEXT("SoloBeach") ? 0
                : ActionName == TEXT("SoloForest") ? 1 : ActionName == TEXT("SoloHunter") ? -2
                : ActionName == TEXT("SoloFullFlow") ? -3 : -1;
            if (ThemeChoice <= -2 && !TestSeedText.IsEmpty() && GetTestSeed() == 0) return;
            GameMode->StartSoloTest(ThemeChoice);
        }
    }
    else if (ActionName == TEXT("MenuSettings")) OpenFrontEndPage(EFrontEndPage::Settings);
    else if (ActionName == TEXT("MenuBack")) OpenFrontEndPage(EFrontEndPage::MainMenu);
    else if (ActionName == TEXT("RoomInvite")) InviteSteamFriend();
    else if (ActionName == TEXT("ToggleSurfacePaint"))
    {
        if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) GM->ToggleSurfacePaint();
    }
    else if (ActionName == TEXT("RoomModeCoop") || ActionName == TEXT("RoomModeOneExplorer"))
    {
        if (!IsLocalController() || FrontEndPage != EFrontEndPage::RoomLobby) return;
        if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
            GM->SelectRoomMode(ActionName == TEXT("RoomModeOneExplorer") ? ETreasureRoomMode::OneExplorer : ETreasureRoomMode::OneMapmaker);
    }
    else if (ActionName == TEXT("RoomStart")) StartOnlineRound();
    else if (ActionName == TEXT("ReturnToSetup"))
    {
        if (IsLocalController())
            if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) GM->ReturnToSetup();
    }
    else if (ActionName == TEXT("ToggleTreasureRange") || ActionName == TEXT("ToggleSpreadPlayerSpawns"))
    {
        if (!IsLocalController() || (FrontEndPage != EFrontEndPage::RoomLobby && FrontEndPage != EFrontEndPage::SoloTest)) return;
        if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
            GM->AdjustRoomSetting(ActionName == TEXT("ToggleTreasureRange") ? TEXT("TreasureRange") : TEXT("SpreadPlayerSpawns"), 1);
    }
    else if (ActionName == TEXT("RoomMapSmaller") || ActionName == TEXT("RoomMapLarger")
        || ActionName == TEXT("RoomDrawingLess") || ActionName == TEXT("RoomDrawingMore")
        || ActionName == TEXT("RoomSearchingLess") || ActionName == TEXT("RoomSearchingMore")
        || ActionName == TEXT("RoomSpeedLess") || ActionName == TEXT("RoomSpeedMore"))
    {
        if (!IsLocalController() || (FrontEndPage != EFrontEndPage::RoomLobby && FrontEndPage != EFrontEndPage::SoloTest)) return;
        if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        {
            const bool bMap = ActionName == TEXT("RoomMapSmaller") || ActionName == TEXT("RoomMapLarger");
            const bool bDrawing = ActionName == TEXT("RoomDrawingLess") || ActionName == TEXT("RoomDrawingMore");
            const bool bSpeed = ActionName == TEXT("RoomSpeedLess") || ActionName == TEXT("RoomSpeedMore");
            const bool bIncrease = ActionName == TEXT("RoomMapLarger") || ActionName == TEXT("RoomDrawingMore")
                || ActionName == TEXT("RoomSearchingMore") || ActionName == TEXT("RoomSpeedMore");
            GM->AdjustRoomSetting(bSpeed ? TEXT("MovementSpeed") : bMap ? TEXT("MapSize") : bDrawing ? TEXT("DrawingTime") : TEXT("SearchingTime"),
                bIncrease ? 1 : -1);
        }
    }
    else if (ActionName == TEXT("RoomBack"))
    {
        if (UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>())
            Online->LeaveRoom();
    }
    else if (ActionName == TEXT("MenuQuit")) ConsoleCommand(TEXT("quit"));
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

void ATreasureSketchPlayerController::UpdateWaitingSketchInput()
{
    if (!IsLocalController()) return;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    const bool bWatching = GS && GS->bGameStarted && IsHunterWaiting();
    if (bWatching == bWaitingSketchInputActive) return;
    bWaitingSketchInputActive = bWatching;
    bShowMouseCursor = bWatching;
    if (bWatching)
    {
        FInputModeGameAndUI WaitingInput;
        WaitingInput.SetHideCursorDuringCapture(false);
        WaitingInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(WaitingInput);
    }
    else SetInputMode(FInputModeGameOnly());
}

bool ATreasureSketchPlayerController::HasSubmittedSketch() const
{
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    return bLocalSketchSubmitted || (PS && PS->bSketchSubmitted);
}

void ATreasureSketchPlayerController::CycleSketchPage(int32 Direction)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!IsLocalController() || (!bMapOpen && !IsHunterWaiting()) || IsLocalScout() || SketchPages.Num() < 2
        || !GS || (GS->Phase != ETreasureRoundPhase::ScoutDrawing && GS->Phase != ETreasureRoundPhase::HunterSearching)
        || (Direction != -1 && Direction != 1)) return;
    ActiveSketchPage = (ActiveSketchPage + Direction + SketchPages.Num()) % SketchPages.Num();
}

void ATreasureSketchPlayerController::ApplyPhaseInputRules()
{
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (!PS || !GS) return;
    const bool bShouldWait = (PS->PlayerRole == ETreasurePlayerRole::Hunter && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
        || (PS->PlayerRole == ETreasurePlayerRole::Scout && GS->Phase != ETreasureRoundPhase::ScoutDrawing)
        || GS->IsRoundOver();
    const bool bShouldLock = bMapOpen || bShouldWait || (IsLocalScout() && HasSubmittedSketch());
    if (bInputLocked != bShouldLock)
    {
        SetIgnoreMoveInput(bShouldLock);
        bInputLocked = bShouldLock;
    }
    const bool bShouldLockLook = bShouldLock;
    if (bLookInputLocked != bShouldLockLook)
    {
        SetIgnoreLookInput(bShouldLockLook);
        bLookInputLocked = bShouldLockLook;
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
    if (ViewedHunterState) return Cast<ATreasureSketchCharacter>(ViewedHunterState->GetPawn());
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS) return nullptr;
    int32 Index = 0;
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
            if (PS->PlayerRole == ETreasurePlayerRole::Hunter && Index++ == SpectatedHunterIndex)
                return Cast<ATreasureSketchCharacter>(PS->GetPawn());
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
    FVector_NetQuantize ViewLocation, FRotator ViewRotation, APlayerState* ViewedPlayer)
{
    ViewedHunterState = ViewedPlayer;
    HunterViewLocation = ViewLocation;
    HunterViewRotation = ViewRotation;
    HunterViewUpdatedAt = GetWorld()->GetTimeSeconds();
    bHasHunterView = true;
}

void ATreasureSketchPlayerController::SetSprayCursorMode(bool bEnabled)
{
    // Retain the existing field and function to avoid a class-layout change.
    // It now controls an over-shoulder aiming view, not a free mouse cursor.
    if (bSprayCursorMode == bEnabled) return;
    bSprayCursorMode = bEnabled;
    if (bEnabled)
    {
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());
    }
    if (GetPawn())
        if (USpringArmComponent* Boom = GetPawn()->FindComponentByClass<USpringArmComponent>())
            Boom->SocketOffset = bEnabled ? FVector(0.f, 130.f, 90.f) : FVector::ZeroVector;
}

void ATreasureSketchPlayerController::ToggleMap()
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if ((GS && GS->IsRoundOver()) || IsHunterWaiting()
        || (IsLocalScout() && (HasSubmittedSketch() || (GS && GS->Phase != ETreasureRoundPhase::ScoutDrawing))))
        return;
    SetSprayCursorMode(false);
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
    if (IsLocalScout() && !HasSubmittedSketch() && GS && GS->bGameStarted && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
    {
        SetSprayCursorMode(false);
        SetLocalTreasureMarkerVisible(false);
        FlushDrawingPoints();
        bLocalSketchSubmitted = true;
        ServerSubmitSketch(GS->RoundSerial, Strokes);
        bMapOpen = false;
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());
        ApplyPhaseInputRules();
        const ATreasureSketchGameState* CurrentGS = GetWorld()->GetGameState<ATreasureSketchGameState>();
        StatusMessage = CurrentGS && CurrentGS->PlayerArray.Num() == 1
            ? TEXT("地图已交付，切换为探索者；按 M 查看自己的地图，按 E 挖掘。")
            : CurrentGS && CurrentGS->RoomMode == ETreasureRoomMode::OneExplorer && CurrentGS->Phase == ETreasureRoundPhase::ScoutDrawing
                ? TEXT("你的地图已交付，等待其他地图师；全部交齐或时间到后开始寻宝。")
                : TEXT("地图已交给寻宝者；现在可以观战。");
        StatusUntil = GetWorld()->GetTimeSeconds() + 5.f;
    }
}

void ATreasureSketchPlayerController::ServerSubmitSketch_Implementation(int32 RoundSerial, const TArray<FSketchStroke>& CompletedStrokes)
{
    ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!PS || PS->PlayerRole != ETreasurePlayerRole::Scout || !GS || GS->RoundSerial != RoundSerial) return;
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->SubmitPlayerSketch(PS, CompletedStrokes);
}

void ATreasureSketchPlayerController::ClientReceiveSketch_Implementation(const TArray<FSketchStroke>& CompletedStrokes)
{
    SketchPages.Reset();
    bLiveSketchActive = false;
    ActiveSketchPage = 0;
    Strokes = CompletedStrokes;
    PendingDrawingPoints.Reset();
    bMapOpen = false;
    bWasDrawing = false;
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
    StatusMessage = TEXT("侦察者的地图已送达！按 M 查看，按 E 挖掘。");
    StatusUntil = GetWorld()->GetTimeSeconds() + 8.f;
}

void ATreasureSketchPlayerController::ClientReceiveSketchPages_Implementation(int32 RoundSerial, const TArray<FSketchPage>& Pages)
{
    if (RoundSerial < CurrentSketchRoundSerial) return;
    const int32 PreviousPage = ActiveSketchPage;
    CurrentSketchRoundSerial = RoundSerial;
    // Keep the single-paper reset/input behavior, then retain every independent page.
    ClientReceiveSketch_Implementation(TArray<FSketchStroke>());
    SketchPages = Pages;
    ActiveSketchPage = FMath::Clamp(PreviousPage, 0, FMath::Max(0, SketchPages.Num() - 1));
    StatusMessage = FString::Printf(TEXT("收到 %d 张地图！M 查看，左右方向键切换图纸，E 挖掘。"), Pages.Num());
}

void ATreasureSketchPlayerController::ClientInitializeLiveSketch_Implementation(int32 RoundSerial, const TArray<FSketchPage>& Pages)
{
    if (RoundSerial < CurrentSketchRoundSerial) return;
    CurrentSketchRoundSerial = RoundSerial;
    SketchPages = Pages;
    ActiveSketchPage = 0;
    bLiveSketchActive = true;
}

void ATreasureSketchPlayerController::ClientAppendLiveSketch_Implementation(
    int32 RoundSerial, int32 MapmakerId, int32 StrokeIndex, const TArray<FVector2D>& Points)
{
    if (!bLiveSketchActive || RoundSerial != CurrentSketchRoundSerial || StrokeIndex < 0) return;
    FSketchPage* Page = SketchPages.FindByPredicate([MapmakerId](const FSketchPage& Candidate)
        { return Candidate.MapmakerId == MapmakerId; });
    if (!Page || StrokeIndex > Page->Strokes.Num()) return;
    if (StrokeIndex == Page->Strokes.Num()) Page->Strokes.AddDefaulted();
    Page->Strokes[StrokeIndex].Points.Append(Points);
}

void ATreasureSketchPlayerController::ClientClearLiveSketch_Implementation(int32 RoundSerial, int32 MapmakerId)
{
    if (!bLiveSketchActive || RoundSerial != CurrentSketchRoundSerial) return;
    if (FSketchPage* Page = SketchPages.FindByPredicate([MapmakerId](const FSketchPage& Candidate)
        { return Candidate.MapmakerId == MapmakerId; })) Page->Strokes.Reset();
}

void ATreasureSketchPlayerController::ClientReplaceLiveSketch_Implementation(int32 RoundSerial, const FSketchPage& Page)
{
    if (!bLiveSketchActive || RoundSerial != CurrentSketchRoundSerial) return;
    if (FSketchPage* Existing = SketchPages.FindByPredicate([&Page](const FSketchPage& Candidate)
        { return Candidate.MapmakerId == Page.MapmakerId; })) *Existing = Page;
}

void ATreasureSketchPlayerController::Dig()
{
    if (!GetPawn()) return;
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    if (PS && PS->PlayerRole == ETreasurePlayerRole::Hunter)
        ServerTryDig(GetPawn()->GetActorLocation());
}

void ATreasureSketchPlayerController::ServerSpraySurface_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || !GS->bSurfacePaintEnabled || !IsLocalScout() || HasSubmittedSketch() || !GetPawn()
        || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextServerSprayTime) return;
    NextServerSprayTime = Now + 0.05f;
    const FVector Direction = FVector(ViewDirection).GetSafeNormal();
    if (FVector::DistSquared(ViewOrigin, GetPawn()->GetActorLocation()) > FMath::Square(900.f)
        || FVector::DotProduct(Direction, GetControlRotation().Vector()) < 0.7f) return;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SurfacePaintAim), true, GetPawn());
    FCollisionObjectQueryParams PaintObjects;
    PaintObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    PaintObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
    // Paint scenery, never players. The water mesh has no collision.
    if (!GetWorld()->LineTraceSingleByObjectType(Hit, ViewOrigin, FVector(ViewOrigin) + Direction * 1800.f,
        PaintObjects, Query)) return;
    const FVector Target = Hit.ImpactPoint;
    const FRotator BodyYaw(0.f, GetPawn()->GetActorRotation().Yaw, 0.f);
    const FVector SprayOrigin = GetPawn()->GetActorLocation() + BodyYaw.Vector() * 45.f
        + FRotationMatrix(BodyYaw).GetUnitAxis(EAxis::Y) * 40.f + FVector(0.f, 0.f, 40.f);
    const FVector SprayDirection = (Target - SprayOrigin).GetSafeNormal();
    if (SprayDirection.IsNearlyZero() || FVector::DistSquared(Target, SprayOrigin) > FMath::Square(1600.f)) return;
    if (!GetWorld()->LineTraceSingleByObjectType(Hit, SprayOrigin, Target + SprayDirection * 3.f,
        PaintObjects, Query)) return;
    UPrimitiveComponent* Component = Hit.GetComponent();
    if (!Component || !Component->IsVisible()) return;
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) GM->SpraySurface(Hit);
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
    SetSprayCursorMode(false);
    StopSpectating();
    SpectatedHunterIndex = 0;
    ViewedHunterState = nullptr;
    NextSpraySampleTime = 0.f;
    NextServerSprayTime = 0.f;
    PendingSpectatorRoundSerial = NewRoundSerial;
    bHasScoutTreasureLocation = false;
    SetLocalTreasureMarkerVisible(false);
    Strokes.Reset();
    SketchPages.Reset();
    bLiveSketchActive = false;
    ActiveSketchPage = 0;
    bLocalSketchSubmitted = false;
    CurrentSketchRoundSerial = NewRoundSerial;
    PendingDrawingPoints.Reset();
    NextDrawingSyncTime = 0.f;
    bMapOpen = false;
    bWaitingSketchInputActive = false;
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
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!IsLocalScout() || HasSubmittedSketch() || !GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    Strokes.Reset();
    PendingDrawingPoints.Reset();
    bWasDrawing = false;
    ServerClearDrawing(GS->RoundSerial);
}

void ATreasureSketchPlayerController::ClientReturnToLobby_Implementation()
{
    bReplayInputActive = false;
    OpenFrontEndPage(GetNetMode() == NM_Standalone ? EFrontEndPage::SoloTest : EFrontEndPage::RoomLobby);
    StatusMessage = TEXT("已返回设置，可调整后再次开始。");
}

void ATreasureSketchPlayerController::ServerCycleSpectatedHunter_Implementation()
{
    if (!IsLocalScout()) return;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || GS->Phase != ETreasureRoundPhase::HunterSearching) return;
    int32 Hunters = 0;
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* PS = Cast<ATreasureSketchPlayerState>(State))
            Hunters += PS->PlayerRole == ETreasurePlayerRole::Hunter;
    if (Hunters > 0) SpectatedHunterIndex = (SpectatedHunterIndex + 1) % Hunters;
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
