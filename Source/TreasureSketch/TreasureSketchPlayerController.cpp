#include "TreasureSketchPlayerController.h"

#include "TreasureSketchGameMode.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureSketchCharacter.h"
#include "TreasureRules.h"
#include "TreasureMarker.h"
#include "ProceduralIsland.h"
#include "TreasureOnlineSubsystem.h"
#include "SketchRotation.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "InputKeyEventArgs.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
constexpr TCHAR HistorySlot[] = TEXT("TreasureSketchHistory");

// Small synthesized cues keep the prototype's shove and water feedback audible without editor-only assets.
void PlayActionCue(UObject* WorldContext, uint8 Kind)
{
    if (!WorldContext) return;
    constexpr int32 SampleRate = 22050;
    const bool bSplash = Kind == 5 || Kind == 6;
    const float Duration = bSplash ? 0.24f : 0.15f;
    const int32 Count = FMath::RoundToInt(SampleRate * Duration);
    TArray<int16> Samples;
    Samples.SetNumUninitialized(Count);
    uint32 Noise = 0x14265u + Kind * 7919u;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Time = static_cast<float>(Index) / SampleRate;
        const float Fade = FMath::Square(1.f - Time / Duration);
        Noise = Noise * 1664525u + 1013904223u;
        const float Random = static_cast<float>((Noise >> 16) & 0xffffu) / 32767.5f - 1.f;
        const float Pitch = Kind == 1 || Kind == 3 ? 125.f : bSplash ? 210.f : 340.f;
        const float Tone = FMath::Sin(2.f * PI * (Pitch * Time + 85.f * Time * Time));
        const float Value = Fade * (bSplash ? 0.32f * Random + 0.12f * Tone
            : Kind == 1 || Kind == 3 ? 0.30f * Tone + 0.18f * Random : 0.16f * Tone + 0.20f * Random);
        Samples[Index] = static_cast<int16>(FMath::Clamp(Value, -1.f, 1.f) * 32767.f);
    }
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(WorldContext);
    Wave->NumChannels = 1;
    Wave->SetSampleRate(SampleRate);
    Wave->Duration = Duration;
    Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
    UGameplayStatics::PlaySound2D(WorldContext, Wave, 0.45f);
}
}

void ATreasureSketchPlayerController::LoadHistory()
{
    if (HistorySave || !IsLocalController()) return;
    HistorySave = Cast<UTreasureHistorySave>(UGameplayStatics::LoadGameFromSlot(HistorySlot, 0));
    if (!HistorySave || HistorySave->Version != 1)
        HistorySave = Cast<UTreasureHistorySave>(UGameplayStatics::CreateSaveGameObject(UTreasureHistorySave::StaticClass()));
}

const TArray<FPlayedRoundRecord>& ATreasureSketchPlayerController::GetHistoryRecords() const
{
    static const TArray<FPlayedRoundRecord> Empty;
    return HistorySave ? HistorySave->Records : Empty;
}

int32 ATreasureSketchPlayerController::GetHistoryGroupStart(int32 GroupIndex) const
{
    const TArray<FPlayedRoundRecord>& Records = GetHistoryRecords();
    int32 Group = -1;
    TSet<FString> SeenSeries;
    for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
    {
        const FString& Series = Records[Index].SeriesId;
        if (!Series.IsEmpty() && SeenSeries.Contains(Series)) continue;
        if (!Series.IsEmpty()) SeenSeries.Add(Series);
        if (++Group == GroupIndex) return Index;
    }
    return INDEX_NONE;
}

int32 ATreasureSketchPlayerController::GetHistoryGroupCount() const
{
    const TArray<FPlayedRoundRecord>& Records = GetHistoryRecords();
    TSet<FString> SeenSeries;
    int32 Count = 0;
    for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
    {
        const FString& Series = Records[Index].SeriesId;
        if (!Series.IsEmpty() && SeenSeries.Contains(Series)) continue;
        if (!Series.IsEmpty()) SeenSeries.Add(Series);
        ++Count;
    }
    return Count;
}

const FPlayedRoundRecord* ATreasureSketchPlayerController::GetSelectedHistoryRecord() const
{
    const TArray<FPlayedRoundRecord>& Records = GetHistoryRecords();
    return Records.IsValidIndex(SelectedHistoryIndex) ? &Records[SelectedHistoryIndex] : nullptr;
}

void ATreasureSketchPlayerController::ClientRecordCompletedRound_Implementation(const FPlayedRoundRecord& Record)
{
    if (!IsLocalController()) return;
    LoadHistory();
    if (!HistorySave || HistorySave->Records.ContainsByPredicate([&Record](const FPlayedRoundRecord& Existing)
        { return Existing.RecordId == Record.RecordId; })) return;
    FPlayedRoundRecord LocalRecord = Record;
    LocalRecord.LocalTimeText = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"));
    if (CurrentSketchRoundSerial == Record.RoundSerial)
        LocalRecord.Pages = SketchPages;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (GS && GS->RoomMode == ETreasureRoomMode::TeamVersus && LocalRecord.Pages.IsEmpty()
        && Record.LocalRole == ETreasurePlayerRole::Scout)
    {
        FSketchPage OwnPage;
        if (const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>())
        { OwnPage.MapmakerId = PS->GetPlayerId(); OwnPage.MapmakerName = PS->GetPlayerName(); }
        OwnPage.Strokes = Strokes;
        OwnPage.PhotoJpeg = LocalPhotoJpeg;
        LocalRecord.Pages.Add(MoveTemp(OwnPage));
    }
    HistorySave->Records.Add(MoveTemp(LocalRecord));
    if (!UGameplayStatics::SaveGameToSlot(HistorySave, HistorySlot, 0))
        UE_LOG(LogTemp, Error, TEXT("TREASURE_HISTORY_SAVE_FAILED"));
}

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
    if (!GM || !LexTryParseString(Scale, *MapScaleText)) return false;
    if (!(RoomNumericSetting == TEXT("MapSize") ? GM->SetRoomMapScale(Scale)
        : GM->SetRoleMovementSpeed(RoomNumericSetting, Scale))) return false;
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
    if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed && !IsFrontEndVisible())
    {
        if (bPropSelectionMode || bPropButtonHeld)
        {
            CancelPropSelection();
            return true;
        }
        if (bCameraMode)
        {
            bCameraMode = false;
            return true;
        }
        const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
        if (GS && GS->bReviewingRound && GS->IsRoundOver())
        {
            RequestRoundReview(false);
            return true;
        }
        if (GS && GS->bGameStarted && !GS->IsRoundOver())
        {
            SetPauseMenuOpen(!bPauseMenuOpen);
            return true;
        }
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

    LoadHistory();

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
    if (FParse::Param(FCommandLine::Get(), TEXT("MagePreview")))
        if (ATreasureSketchGameMode* GameMode = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
            GameMode->StartSoloTest(1);
}

void ATreasureSketchPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("Map", IE_Pressed, this, &ATreasureSketchPlayerController::ToggleMap);
    InputComponent->BindAction("Handoff", IE_Pressed, this, &ATreasureSketchPlayerController::Handoff);
    InputComponent->BindAction("Dig", IE_Pressed, this, &ATreasureSketchPlayerController::Dig);
    InputComponent->BindAction("Dig", IE_Released, this, &ATreasureSketchPlayerController::StopDig);
    InputComponent->BindAction("Shove", IE_Pressed, this, &ATreasureSketchPlayerController::Shove);
    InputComponent->BindAction("PropDisguise", IE_Pressed, this, &ATreasureSketchPlayerController::TransformIntoProp);
    InputComponent->BindAction("PropDisguise", IE_Released, this, &ATreasureSketchPlayerController::ReleasePropSelection);
    InputComponent->BindAction("RestoreHuman", IE_Pressed, this, &ATreasureSketchPlayerController::RestoreHumanForm);
    InputComponent->BindAction("TakePhoto", IE_Pressed, this, &ATreasureSketchPlayerController::TakePhoto);
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

void ATreasureSketchPlayerController::ResetRoundPhoto()
{
    CancelPropSelection();
    ServerPhotoJpeg.Reset();
    bServerDrawingOverheadView = false;
}

void ATreasureSketchPlayerController::TakePhoto()
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bPhotoClueEnabled || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || !IsLocalScout() || HasSubmittedSketch() || bMapOpen || bPauseMenuOpen
        || bDrawingOverheadView || SpectatorCamera || !GetPawn() || !LocalPhotoJpeg.IsEmpty()) return;

    if (!bCameraMode)
    {
        bCameraMode = true;
        SetSprayCursorMode(false);
        return;
    }

    FVector ViewOrigin;
    FRotator ViewRotation;
    GetPlayerViewPoint(ViewOrigin, ViewRotation);
    constexpr int32 Width = 384, Height = 216;
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
    Target->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(this);
    Capture->TextureTarget = Target;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->FOVAngle = PlayerCameraManager ? PlayerCameraManager->GetFOVAngle() : 90.f;
    Capture->HiddenActors.Add(GetPawn());
    if (LocalScoutMarker) Capture->HiddenActors.Add(LocalScoutMarker);
    Capture->RegisterComponentWithWorld(GetWorld());
    Capture->SetWorldLocationAndRotation(ViewOrigin, ViewRotation);
    Capture->CaptureScene();
    TArray<FColor> Pixels;
    const bool bCaptured = Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
    Capture->DestroyComponent();
    if (!bCaptured || Pixels.Num() != Width * Height) return;

    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    const TSharedPtr<IImageWrapper> Jpeg = Images.CreateImageWrapper(EImageFormat::JPEG);
    if (!Jpeg.IsValid() || !Jpeg->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor),
        Width, Height, ERGBFormat::BGRA, 8)) return;
    TArray64<uint8> Compressed = Jpeg->GetCompressed(55);
    if (Compressed.Num() > 48 * 1024) Compressed = Jpeg->GetCompressed(25);
    if (Compressed.IsEmpty() || Compressed.Num() > 48 * 1024)
    {
        StatusMessage = TEXT("照片太复杂，请换一个角度再拍。");
        StatusUntil = GetWorld()->GetTimeSeconds() + 4.f;
        return;
    }
    LocalPhotoJpeg.Append(Compressed.GetData(), static_cast<int32>(Compressed.Num()));
    bCameraMode = false;
    ServerSubmitPhoto(GS->RoundSerial, ViewOrigin, ViewRotation, LocalPhotoJpeg);
    StatusMessage = TEXT("照片已拍好；打开画纸可查看，交图后探索者也能看到。");
    StatusUntil = GetWorld()->GetTimeSeconds() + 5.f;
}

void ATreasureSketchPlayerController::ServerSetDrawingOverheadView_Implementation(bool bOverhead)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (GS && GS->bGameStarted && GS->Phase == ETreasureRoundPhase::ScoutDrawing && IsLocalScout())
        bServerDrawingOverheadView = bOverhead;
}

void ATreasureSketchPlayerController::ServerSubmitPhoto_Implementation(
    int32 RoundSerial, FVector_NetQuantize ViewOrigin, FRotator ViewRotation, const TArray<uint8>& PhotoJpeg)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bPhotoClueEnabled || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || GS->RoundSerial != RoundSerial || !IsLocalScout() || HasSubmittedSketch()
        || bServerDrawingOverheadView || !GetPawn() || !ServerPhotoJpeg.IsEmpty()
        || PhotoJpeg.Num() < 100 || PhotoJpeg.Num() > 48 * 1024
        || FVector::DistSquared(ViewOrigin, GetPawn()->GetActorLocation()) > FMath::Square(700.f)
        || FVector::DotProduct(ViewRotation.Vector(), GetControlRotation().Vector()) < 0.6f) return;
    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    const TSharedPtr<IImageWrapper> Jpeg = Images.CreateImageWrapper(EImageFormat::JPEG);
    if (!Jpeg.IsValid() || !Jpeg->SetCompressed(PhotoJpeg.GetData(), PhotoJpeg.Num())
        || Jpeg->GetWidth() != 384 || Jpeg->GetHeight() != 216) return;
    ServerPhotoJpeg = PhotoJpeg;
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->BroadcastSketchPhoto(GetPlayerState<ATreasureSketchPlayerState>(), ServerPhotoJpeg);
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
    if (bPhotoExpanded) return false;
    const FVector2D Min = GetPaperMin(), Size = GetPaperSize();
    const FVector2D ContentMin = Min + FVector2D(0.f, 92.f);
    const FVector2D ContentSize = Size - FVector2D(0.f, 92.f);
    const FVector2D Unrotated = SketchRotation::FromScreen(Point, ContentMin, ContentSize, PaperRotationSteps);
    return Unrotated.X >= ContentMin.X && Unrotated.Y >= ContentMin.Y
        && Unrotated.X <= ContentMin.X + ContentSize.X && Unrotated.Y <= ContentMin.Y + ContentSize.Y;
}

void ATreasureSketchPlayerController::RotatePaper(int32 Direction)
{
    if ((Direction != -1 && Direction != 1) || bPhotoExpanded
        || (!bMapOpen && !IsHunterWaiting() && FrontEndPage != EFrontEndPage::History)) return;
    FlushDrawingPoints();
    bWasDrawing = false;
    PaperRotationSteps = SketchRotation::NormalizeSteps(PaperRotationSteps + Direction);
}

int32 ATreasureSketchPlayerController::GetInkUsed() const
{
    int32 Used = 0;
    for (const FSketchStroke& Stroke : Strokes)
        if (Stroke.ColorIndex != 5) Used += Stroke.Points.Num();
    return Used;
}

float ATreasureSketchPlayerController::GetDigFeedbackRemaining() const
{
    return GetWorld() ? FMath::Max(0.f, DigFeedbackUntil - GetWorld()->GetTimeSeconds()) : 0.f;
}

float ATreasureSketchPlayerController::GetActionFeedbackRemaining() const
{
    return GetWorld() ? FMath::Max(0.f, ActionFeedbackUntil - GetWorld()->GetTimeSeconds()) : 0.f;
}

float ATreasureSketchPlayerController::GetHoldDigProgress() const
{
    return bLocalDigHeld && GetWorld() ? FMath::Clamp((GetWorld()->GetTimeSeconds() - LocalDigStartedAt)
        / (GetWorld()->GetGameState<ATreasureSketchGameState>()
            && GetWorld()->GetGameState<ATreasureSketchGameState>()->RoomMode == ETreasureRoomMode::HideAndSeek
            ? ATreasureSketchGameMode::HideDigSeconds : ATreasureSketchGameMode::HeldDigSeconds), 0.f, 1.f) : 0.f;
}

void ATreasureSketchPlayerController::UpdateHideTreasureMarkers()
{
    if (!IsLocalController()) return;
    const auto* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    TArray<FVector> Desired;
    if (GS && GS->RoomMode == ETreasureRoomMode::HideAndSeek && GS->bGameStarted
        && !GS->IsHidePreparation()
        && (!GS->IsRoundOver() || GS->bReviewingRound))
        for (int32 Index = 0; Index < GS->HideTreasures.Num(); ++Index)
            if (!(GS->HideCollectedMask & (1 << Index))) Desired.Add(GS->HideTreasures[Index]);
    if (Desired == VisibleHideTreasures) return;
    for (auto& Marker : HideMarkers) if (Marker) Marker->Destroy();
    HideMarkers.Reset();
    VisibleHideTreasures = Desired;
    for (const FVector& Point : Desired)
        HideMarkers.Add(GetWorld()->SpawnActor<ATreasureMarker>(ATreasureMarker::StaticClass(), Point, FRotator::ZeroRotator));
}

void ATreasureSketchPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    UpdateHideTreasureMarkers();
    UpdateFrontEnd();
    UpdatePropSelection(DeltaTime);
    UpdateVersusCounterpartVisibility();
    if (IsFrontEndVisible()) { bCameraMode = false; return; }
    const ATreasureSketchGameState* CursorGS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (bLocalDigHeld && (bMapOpen || !CursorGS || CursorGS->Phase != ETreasureRoundPhase::HunterSearching
        || !GetPawn() || FVector::Dist2D(GetPawn()->GetActorLocation(), LocalDigStartLocation) > 100.f
        || (Cast<ATreasureSketchCharacter>(GetPawn())
            && !Cast<ATreasureSketchCharacter>(GetPawn())->GetCharacterMovement()->IsMovingOnGround())))
        StopDig();
    if (bCameraMode && (!CursorGS || !CursorGS->bGameStarted || !CursorGS->bPhotoClueEnabled
        || CursorGS->Phase != ETreasureRoundPhase::ScoutDrawing || !IsLocalScout()
        || HasSubmittedSketch() || bMapOpen || bDrawingOverheadView || SpectatorCamera || !GetPawn()))
        bCameraMode = false;
    UpdateReplayInput();
    if (bPauseMenuOpen)
    {
        SetSprayCursorMode(false);
        ApplyPhaseInputRules();
        return;
    }
    const bool bCanSpray = CursorGS && CursorGS->bGameStarted && CursorGS->bSurfacePaintEnabled
        && IsLocalScout() && !HasSubmittedSketch() && CursorGS->Phase == ETreasureRoundPhase::ScoutDrawing
        && !bMapOpen && !bDrawingOverheadView && !bCameraMode;
    SetSprayCursorMode(bCanSpray);
    UpdateWaitingSketchInput();
    if (bMapOpen || IsHunterWaiting())
    {
        if (WasInputKeyJustPressed(EKeys::Z)) RotatePaper(-1);
        if (WasInputKeyJustPressed(EKeys::X)) RotatePaper(1);
    }
    if (IsScoutSpectating() && !bMapOpen && WasInputKeyJustPressed(EKeys::Q)) ServerCycleSpectatedHunter();
    UpdateSpectatorCamera(DeltaTime);
    ApplyPhaseInputRules();
    ApplyKeyboardMovementFallback();
    if ((bMapOpen || IsHunterWaiting()) && (!IsLocalScout()
        || (CursorGS && (CursorGS->bReviewingRound || CursorGS->Phase == ETreasureRoundPhase::HunterSearching))))
    {
        if (WasInputKeyJustPressed(EKeys::Left)) CycleSketchPage(-1);
        if (WasInputKeyJustPressed(EKeys::Right)) CycleSketchPage(1);
    }
    if (!StatusMessage.IsEmpty() && GetWorld()->GetTimeSeconds() >= StatusUntil)
        StatusMessage.Empty();
    const ATreasureSketchGameState* PaintGS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (PaintGS && PaintGS->bGameStarted && PaintGS->bSurfacePaintEnabled && IsLocalScout()
        && !HasSubmittedSketch() && PaintGS->Phase == ETreasureRoundPhase::ScoutDrawing && !bMapOpen && !bDrawingOverheadView
        && !bCameraMode
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
        return;
    }

    float X = 0.f, Y = 0.f;
    const bool bPressed = IsInputKeyDown(EKeys::LeftMouseButton) && GetMousePosition(X, Y) && IsPointOnPaper(FVector2D(X,Y));
    const bool bHasInk = SelectedInkColor == 5 || !PaintGS->bLimitedInk || GetInkUsed() < PaintGS->InkLimit;
    if (bPressed && bHasInk)
    {
        if (!bWasDrawing)
        {
            FlushDrawingPoints();
            Strokes.AddDefaulted();
            Strokes.Last().ColorIndex = SelectedInkColor;
            Strokes.Last().EraserSize = SelectedInkColor == 5 ? SelectedEraserSize : 0;
        }
        const FVector2D PaperMin = GetPaperMin(), PaperSize = GetPaperSize();
        const FVector2D Unrotated = SketchRotation::FromScreen(FVector2D(X, Y),
            PaperMin + FVector2D(0.f, 92.f), PaperSize - FVector2D(0.f, 92.f), PaperRotationSteps);
        const FVector2D Normalized = (Unrotated - PaperMin) / PaperSize;
        if (Strokes.Last().Points.IsEmpty() || FVector2D::Distance(Strokes.Last().Points.Last(), Normalized) > 0.003f)
        {
            Strokes.Last().Points.Add(Normalized);
            PendingDrawingPoints.Add(Normalized);
        }
    }
    bWasDrawing = bPressed && bHasInk;
    if (!bPressed || GetWorld()->GetTimeSeconds() >= NextDrawingSyncTime || PendingDrawingPoints.Num() >= 128)
        FlushDrawingPoints();
}

void ATreasureSketchPlayerController::FlushDrawingPoints()
{
    if (PendingDrawingPoints.IsEmpty()) return;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (GS && GS->bGameStarted && GS->Phase == ETreasureRoundPhase::ScoutDrawing && IsLocalScout() && !HasSubmittedSketch())
        ServerAppendDrawing(GS->RoundSerial, Strokes.Num() - 1, Strokes.Last().ColorIndex,
            Strokes.Last().EraserSize, PendingDrawingPoints);
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

void ATreasureSketchPlayerController::ServerAppendDrawing_Implementation(int32 RoundSerial, int32 StrokeIndex, uint8 ColorIndex, uint8 EraserSize, const TArray<FVector2D>& Points)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing
        || GS->RoundSerial != RoundSerial || !IsLocalScout() || HasSubmittedSketch() || StrokeIndex < 0
        || Points.Num() > 128 || ColorIndex > 5 || EraserSize > 1 || (ColorIndex != 5 && EraserSize != 0)) return;
    if (ServerDrawingRoundSerial != RoundSerial)
    {
        ServerDrawing.Reset();
        ServerDrawingRoundSerial = RoundSerial;
    }
    if (StrokeIndex > ServerDrawing.Num()) return;
    for (const FVector2D& Point : Points)
        if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y)
            || Point.X < 0.f || Point.X > 1.f || Point.Y < 0.f || Point.Y > 1.f) return;
    if (StrokeIndex < ServerDrawing.Num() && (ServerDrawing[StrokeIndex].ColorIndex != ColorIndex
        || ServerDrawing[StrokeIndex].EraserSize != EraserSize)) return;
    if (GS->bLimitedInk && ColorIndex != 5)
    {
        int32 Used = 0;
        for (const FSketchStroke& Stroke : ServerDrawing)
            if (Stroke.ColorIndex != 5) Used += Stroke.Points.Num();
        if (Used + Points.Num() > GS->InkLimit) return;
    }
    if (StrokeIndex == ServerDrawing.Num())
    {
        ServerDrawing.AddDefaulted();
        ServerDrawing.Last().ColorIndex = ColorIndex;
        ServerDrawing.Last().EraserSize = EraserSize;
    }
    ServerDrawing[StrokeIndex].Points.Append(Points);
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->BroadcastSketchDelta(GetPlayerState<ATreasureSketchPlayerState>(), StrokeIndex, ColorIndex, EraserSize, Points);
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

void ATreasureSketchPlayerController::SetPauseMenuOpen(bool bOpen)
{
    if (!IsLocalController() || bPauseMenuOpen == bOpen) return;
    bPauseMenuOpen = bOpen;
    if (bOpen)
    {
        CancelPropSelection();
        ServerCancelDig();
        bLocalDigHeld = false;
        bCameraMode = false;
        FlushDrawingPoints();
        bWasDrawing = false;
        bMapOpen = false;
        SetSprayCursorMode(false);
    }
    bShowMouseCursor = bOpen || IsHunterWaiting();
    ApplyPhaseInputRules();
    if (bShowMouseCursor)
    {
        FInputModeGameAndUI MenuInput;
        MenuInput.SetHideCursorDuringCapture(false);
        MenuInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(MenuInput);
    }
    else SetInputMode(FInputModeGameOnly());
}

void ATreasureSketchPlayerController::ApplyKeyboardMovementFallback()
{
    // UE 5.6 projects using EnhancedPlayerInput can stop forwarding legacy AxisMappings
    // after switching from a GameAndUI menu back to gameplay. Polling the four movement
    // keys here keeps the prototype and packaged builds controllable without an IMC asset.
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn || IsMoveInputIgnored() || bMapOpen || SpectatorCamera) return;

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
    if (ActionName == TEXT("RotatePaperLeft") || ActionName == TEXT("RotatePaperRight"))
    {
        RotatePaper(ActionName == TEXT("RotatePaperRight") ? 1 : -1);
        return;
    }
    if (ActionName == TEXT("TogglePhoto") || ActionName == TEXT("ClosePhotoOverlay"))
    {
        if ((bMapOpen || IsHunterWaiting()) && !GetPhotoJpeg().IsEmpty())
            bPhotoExpanded = ActionName == TEXT("ClosePhotoOverlay") ? false : !bPhotoExpanded;
        return;
    }
    if (ActionName == TEXT("InkBlack") || ActionName == TEXT("InkRed") || ActionName == TEXT("InkBlue")
        || ActionName == TEXT("InkGreen") || ActionName == TEXT("InkGold")
        || ActionName == TEXT("InkEraserSmall") || ActionName == TEXT("InkEraserLarge"))
    {
        const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
        if (bMapOpen && IsLocalScout() && !HasSubmittedSketch() && GS && GS->bGameStarted
            && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
        {
            FlushDrawingPoints();
            bWasDrawing = false;
            SelectedInkColor = ActionName == TEXT("InkRed") ? 1 : ActionName == TEXT("InkBlue") ? 2
                : ActionName == TEXT("InkGreen") ? 3 : ActionName == TEXT("InkGold") ? 4
                : ActionName == TEXT("InkEraserSmall") || ActionName == TEXT("InkEraserLarge") ? 5 : 0;
            if (SelectedInkColor == 5) SelectedEraserSize = ActionName == TEXT("InkEraserLarge") ? 1 : 0;
        }
        return;
    }
    if (ActionName == TEXT("PreviousSketchPage") || ActionName == TEXT("NextSketchPage"))
    {
        CycleSketchPage(ActionName == TEXT("NextSketchPage") ? 1 : -1);
        return;
    }
    if (ActionName == TEXT("BeginRoundReview"))
    {
        RequestRoundReview(true);
        return;
    }
    if (ActionName == TEXT("ToggleReviewTreasure"))
    {
        if (IsLocalController() && bMapOpen) ToggleSpectatorTreasure();
        return;
    }
    if (ActionName == TEXT("PauseResume")) { SetPauseMenuOpen(false); return; }
    if (ActionName == TEXT("PauseOpenMap"))
    {
        if (bPauseMenuOpen) { SetPauseMenuOpen(false); ToggleMap(); }
        return;
    }
    if (ActionName == TEXT("PauseReturnToLobby"))
    {
        if (bPauseMenuOpen && IsLocalController() && GetNetMode() == NM_ListenServer)
            if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) GM->ReturnToSetup();
        return;
    }
    if (ActionName == TEXT("RoomClaimSingleRole"))
    {
        const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
        if (IsLocalController() && FrontEndPage == EFrontEndPage::RoomLobby && GS && !GS->bGameStarted)
            ServerClaimSingleRoomRole();
        return;
    }
    if (ActionName == TEXT("MapScaleInput") || ActionName == TEXT("HiderSpeedInput") || ActionName == TEXT("CatcherSpeedInput"))
    {
        if (!IsLocalController() || (FrontEndPage != EFrontEndPage::RoomLobby && FrontEndPage != EFrontEndPage::SoloTest)
            || !GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) return;
        const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
        if (!GS || GS->bGameStarted) return;
        if (!CommitMapScale()) return;
        RoomNumericSetting = ActionName == TEXT("MapScaleInput") ? TEXT("MapSize")
            : ActionName == TEXT("HiderSpeedInput") ? TEXT("HiderSpeed") : TEXT("CatcherSpeed");
        if (RoomNumericSetting != TEXT("MapSize") && GS->RoomMode != ETreasureRoomMode::HideAndSeek) return;
        const float Value = RoomNumericSetting == TEXT("MapSize") ? GS->RoomMapScale
            : RoomNumericSetting == TEXT("HiderSpeed") ? GS->HiderSpeedMultiplier : GS->CatcherSpeedMultiplier;
        MapScaleText = FString::Printf(TEXT("%.6g"), Value);
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
    else if (ActionName == TEXT("MenuHistory"))
    {
        LoadHistory();
        HistoryListOffset = 0;
        SelectedHistoryIndex = INDEX_NONE;
        OpenFrontEndPage(EFrontEndPage::History);
    }
    else if (ActionName == TEXT("HistoryBack"))
    {
        if (SelectedHistoryIndex != INDEX_NONE) SelectedHistoryIndex = INDEX_NONE;
        else OpenFrontEndPage(EFrontEndPage::MainMenu);
    }
    else if (ActionName == TEXT("HistoryPrevList")) HistoryListOffset = FMath::Max(0, HistoryListOffset - 5);
    else if (ActionName == TEXT("HistoryNextList"))
    {
        if (HistoryListOffset + 5 < GetHistoryGroupCount()) HistoryListOffset += 5;
    }
    else if (ActionName.ToString().StartsWith(TEXT("HistorySelect")))
    {
        const int32 GroupIndex = HistoryListOffset + FCString::Atoi(*ActionName.ToString().RightChop(13));
        SelectedHistoryIndex = GetHistoryGroupStart(GroupIndex);
        HistorySketchPageIndex = 0;
    }
    else if (ActionName == TEXT("HistoryPrevRound") || ActionName == TEXT("HistoryNextRound"))
    {
        const TArray<FPlayedRoundRecord>& Records = GetHistoryRecords();
        if (Records.IsValidIndex(SelectedHistoryIndex) && !Records[SelectedHistoryIndex].SeriesId.IsEmpty())
        {
            const int32 Step = ActionName == TEXT("HistoryNextRound") ? 1 : -1;
            const FString Series = Records[SelectedHistoryIndex].SeriesId;
            for (int32 Next = SelectedHistoryIndex + Step; Records.IsValidIndex(Next); Next += Step)
                if (Records[Next].SeriesId == Series)
                { SelectedHistoryIndex = Next; HistorySketchPageIndex = 0; break; }
        }
    }
    else if (ActionName == TEXT("HistoryPrevSketch") || ActionName == TEXT("HistoryNextSketch"))
    {
        if (const FPlayedRoundRecord* Record = GetSelectedHistoryRecord(); Record && Record->Pages.Num() > 0)
            HistorySketchPageIndex = (HistorySketchPageIndex + Record->Pages.Num()
                + (ActionName == TEXT("HistoryNextSketch") ? 1 : -1)) % Record->Pages.Num();
    }
    else if (ActionName == TEXT("MenuSolo")) OpenFrontEndPage(EFrontEndPage::SoloTest);
    else if (ActionName == TEXT("RoomDrawingRules")) OpenFrontEndPage(EFrontEndPage::RoomDrawingRules);
    else if (ActionName == TEXT("RoomDrawingRulesBack")) OpenFrontEndPage(EFrontEndPage::RoomLobby);
    else if (ActionName == TEXT("SoloBeach") || ActionName == TEXT("SoloForest") || ActionName == TEXT("SoloCanyon") || ActionName == TEXT("SoloRandom") || ActionName == TEXT("SoloHunter") || ActionName == TEXT("SoloFullFlow"))
    {
        if (ATreasureSketchGameMode* GameMode = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        {
            const int32 ThemeChoice = ActionName == TEXT("SoloBeach") ? 0
                : ActionName == TEXT("SoloForest") ? 1 : ActionName == TEXT("SoloCanyon") ? 2 : ActionName == TEXT("SoloHunter") ? -2
                : ActionName == TEXT("SoloFullFlow") ? -3 : -1;
            if ((ThemeChoice == 2 || ThemeChoice <= -2) && !TestSeedText.IsEmpty() && GetTestSeed() == 0) return;
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
    else if (ActionName == TEXT("RoomModeCoop") || ActionName == TEXT("RoomModeOneExplorer")
        || ActionName == TEXT("RoomModeRaceToggle") || ActionName == TEXT("RoomModeHideAndSeek") || ActionName == TEXT("RoomModeTeamVersus"))
    {
        if (!IsLocalController() || FrontEndPage != EFrontEndPage::RoomLobby) return;
        if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        {
            const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
            const ETreasureRoomMode Mode = ActionName == TEXT("RoomModeHideAndSeek") ? ETreasureRoomMode::HideAndSeek
                : ActionName == TEXT("RoomModeTeamVersus") ? ETreasureRoomMode::TeamVersus
                : ActionName == TEXT("RoomModeOneExplorer") ? ETreasureRoomMode::OneExplorer
                : ActionName == TEXT("RoomModeRaceToggle") && GS && GS->RoomMode != ETreasureRoomMode::ExplorerRace
                    ? ETreasureRoomMode::ExplorerRace : ETreasureRoomMode::OneMapmaker;
            GM->SelectRoomMode(Mode);
        }
    }
    else if (ActionName == TEXT("RoomPoolBeach") || ActionName == TEXT("RoomPoolForest")
        || ActionName == TEXT("RoomPoolCanyon"))
    {
        if (!IsLocalController() || FrontEndPage != EFrontEndPage::RoomLobby) return;
        if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
            GM->ToggleRoomMapPool(ActionName == TEXT("RoomPoolBeach") ? EIslandTheme::PirateBeach
                : ActionName == TEXT("RoomPoolForest") ? EIslandTheme::MistForest
                : EIslandTheme::CanyonGraybox);
    }
    else if (ActionName == TEXT("RoomStart")) StartOnlineRound();
    else if (ActionName == TEXT("ReturnToSetup"))
    {
        if (IsLocalController())
            if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>()) GM->ReturnToSetup();
    }
    else if (ActionName == TEXT("ToggleTreasureRange") || ActionName == TEXT("ToggleSpreadPlayerSpawns")
        || ActionName == TEXT("ToggleSketchSceneLock") || ActionName == TEXT("TogglePreprintedIsland")
        || ActionName == TEXT("ToggleLimitedInk") || ActionName == TEXT("TogglePhotoClue"))
    {
        if (!IsLocalController() || (FrontEndPage != EFrontEndPage::RoomLobby
            && FrontEndPage != EFrontEndPage::RoomDrawingRules && FrontEndPage != EFrontEndPage::SoloTest)) return;
        if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
            GM->AdjustRoomSetting(ActionName == TEXT("ToggleTreasureRange") ? TEXT("TreasureRange")
                : ActionName == TEXT("ToggleSpreadPlayerSpawns") ? TEXT("SpreadPlayerSpawns")
                : ActionName == TEXT("ToggleSketchSceneLock") ? TEXT("SketchSceneLock")
                : ActionName == TEXT("TogglePreprintedIsland") ? TEXT("PreprintedIsland")
                : ActionName == TEXT("TogglePhotoClue") ? TEXT("PhotoClue") : TEXT("LimitedInk"), 1);
    }
    else if (ActionName == TEXT("RoomMapSmaller") || ActionName == TEXT("RoomMapLarger")
        || ActionName == TEXT("RoomDrawingLess") || ActionName == TEXT("RoomDrawingMore")
        || ActionName == TEXT("RoomSearchingLess") || ActionName == TEXT("RoomSearchingMore")
        || ActionName == TEXT("RoomDigCooldownLess") || ActionName == TEXT("RoomDigCooldownMore")
        || ActionName == TEXT("RoomSpeedLess") || ActionName == TEXT("RoomSpeedMore")
        || ActionName == TEXT("HiderSpeedLess") || ActionName == TEXT("HiderSpeedMore")
        || ActionName == TEXT("CatcherSpeedLess") || ActionName == TEXT("CatcherSpeedMore")
        || ActionName == TEXT("RoomInkLess") || ActionName == TEXT("RoomInkMore"))
    {
        if (!IsLocalController() || (FrontEndPage != EFrontEndPage::RoomLobby
            && FrontEndPage != EFrontEndPage::RoomDrawingRules && FrontEndPage != EFrontEndPage::SoloTest)) return;
        if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        {
            if (ActionName == TEXT("HiderSpeedLess") || ActionName == TEXT("HiderSpeedMore")
                || ActionName == TEXT("CatcherSpeedLess") || ActionName == TEXT("CatcherSpeedMore"))
            {
                const bool bHider = ActionName == TEXT("HiderSpeedLess") || ActionName == TEXT("HiderSpeedMore");
                const bool bMore = ActionName == TEXT("HiderSpeedMore") || ActionName == TEXT("CatcherSpeedMore");
                GM->AdjustRoomSetting(bHider ? TEXT("HiderSpeed") : TEXT("CatcherSpeed"), bMore ? 1 : -1);
                return;
            }
            const bool bMap = ActionName == TEXT("RoomMapSmaller") || ActionName == TEXT("RoomMapLarger");
            const bool bDrawing = ActionName == TEXT("RoomDrawingLess") || ActionName == TEXT("RoomDrawingMore");
            const bool bDigCooldown = ActionName == TEXT("RoomDigCooldownLess") || ActionName == TEXT("RoomDigCooldownMore");
            const bool bSpeed = ActionName == TEXT("RoomSpeedLess") || ActionName == TEXT("RoomSpeedMore");
            const bool bInk = ActionName == TEXT("RoomInkLess") || ActionName == TEXT("RoomInkMore");
            const bool bIncrease = ActionName == TEXT("RoomMapLarger") || ActionName == TEXT("RoomDrawingMore")
                || ActionName == TEXT("RoomSearchingMore") || ActionName == TEXT("RoomSpeedMore")
                || ActionName == TEXT("RoomDigCooldownMore") || ActionName == TEXT("RoomInkMore");
            GM->AdjustRoomSetting(bDigCooldown ? TEXT("DigCooldown") : bSpeed ? TEXT("MovementSpeed")
                : bInk ? TEXT("InkLimit")
                : bMap ? TEXT("MapSize") : bDrawing ? TEXT("DrawingTime") : TEXT("SearchingTime"),
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
    return PS && GS && GS->RoomMode != ETreasureRoomMode::TeamVersus
        && PS->PlayerRole == ETreasurePlayerRole::Hunter && GS->Phase == ETreasureRoundPhase::ScoutDrawing;
}

void ATreasureSketchPlayerController::UpdateVersusCounterpartVisibility()
{
    if (!IsLocalController()) return;
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    TArray<TWeakObjectPtr<APawn>> DesiredHidden;
    if (GS && GS->bGameStarted && !GS->IsRoundOver()
        && GS->RoomMode == ETreasureRoomMode::TeamVersus && PS)
        for (TActorIterator<ATreasureSketchCharacter> It(GetWorld()); It; ++It)
            if (const ATreasureSketchPlayerState* Other = It->GetPlayerState<ATreasureSketchPlayerState>();
                Other && Other != PS && !PS->IsVersusCounterpart(Other)) DesiredHidden.Add(*It);
    for (const TWeakObjectPtr<APawn>& Previous : HiddenVersusPawns)
        if (APawn* OtherPawn = Previous.Get(); OtherPawn && !DesiredHidden.Contains(Previous))
        {
            if (USceneComponent* Root = OtherPawn->GetRootComponent()) Root->SetVisibility(true, true);
            if (APawn* LocalPawn = GetPawn()) LocalPawn->MoveIgnoreActorRemove(OtherPawn);
        }
    for (const TWeakObjectPtr<APawn>& Hidden : DesiredHidden)
        if (APawn* OtherPawn = Hidden.Get())
        {
            if (USceneComponent* Root = OtherPawn->GetRootComponent()) Root->SetVisibility(false, true);
            if (!HiddenVersusPawns.Contains(Hidden))
                if (APawn* LocalPawn = GetPawn()) LocalPawn->MoveIgnoreActorAdd(OtherPawn);
        }
    HiddenVersusPawns = MoveTemp(DesiredHidden);
}

bool ATreasureSketchPlayerController::IsCatcherStudyingMap() const
{
    const auto* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    return GS && GS->IsHidePreparation() && IsLocalScout();
}

void ATreasureSketchPlayerController::UpdateWaitingSketchInput()
{
    if (!IsLocalController() || bPauseMenuOpen) return;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (GS && GS->IsRoundOver()) { bWaitingSketchInputActive = false; return; }
    const bool bWatching = GS && GS->bGameStarted && (IsHunterWaiting() || IsCatcherStudyingMap());
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
    if (!IsLocalController() || (!bMapOpen && !IsHunterWaiting())
        || (IsLocalScout() && !(GS && (GS->bReviewingRound || GS->Phase == ETreasureRoundPhase::HunterSearching)))
        || SketchPages.Num() < 2
        || !GS || (GS->Phase != ETreasureRoundPhase::ScoutDrawing && GS->Phase != ETreasureRoundPhase::HunterSearching
            && !GS->bReviewingRound)
        || (Direction != -1 && Direction != 1)) return;
    ActiveSketchPage = (ActiveSketchPage + Direction + SketchPages.Num()) % SketchPages.Num();
}

void ATreasureSketchPlayerController::ApplyPhaseInputRules()
{
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (!PS || !GS) return;
    const bool bReviewing = GS->bReviewingRound && GS->IsRoundOver();
    const bool bVersus = GS->RoomMode == ETreasureRoomMode::TeamVersus;
    const bool bShouldWait = !bReviewing && !bVersus && ((PS->PlayerRole == ETreasurePlayerRole::Hunter && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
        || (PS->PlayerRole == ETreasurePlayerRole::Scout && GS->RoomMode != ETreasureRoomMode::HideAndSeek && GS->Phase != ETreasureRoundPhase::ScoutDrawing)
        || IsCatcherStudyingMap() || (GS->RoomMode == ETreasureRoomMode::HideAndSeek && PS->bHideEliminated)
        || GS->IsRoundOver()) || (!bReviewing && bVersus && GS->IsRoundOver());
    const bool bShouldLock = bPauseMenuOpen || bMapOpen || bDrawingOverheadView || bShouldWait
        || (!bReviewing && IsLocalScout() && HasSubmittedSketch()
            && (!bVersus || GS->Phase == ETreasureRoundPhase::ScoutDrawing));
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
    const bool bRoundOver = GS && GS->IsRoundOver() && !GS->bReviewingRound;
    if (bRoundOver && bPauseMenuOpen) SetPauseMenuOpen(false);
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

void ATreasureSketchPlayerController::StartSpectating(bool bDrawingView)
{
    FVector StartLocation = GetPawn() ? GetPawn()->GetActorLocation() + FVector(0.f, 0.f, bDrawingView ? 1200.f : 300.f)
        : FVector(0.f, 0.f, 500.f);
    FRotator StartRotation = bDrawingView ? FRotator(-70.f, GetControlRotation().Yaw, 0.f) : GetControlRotation();
    if (!bDrawingView)
    {
        if (ATreasureSketchCharacter* Hunter = FindHunterCharacter())
        {
            StartLocation = Hunter->GetActorLocation() + FVector(-400.f, 0.f, 350.f);
            StartRotation = (Hunter->GetActorLocation() - StartLocation).Rotation();
        }
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

void ATreasureSketchPlayerController::StopSpectating(bool bHideTreasure)
{
    if (!SpectatorCamera) return;
    if (GetPawn()) SetViewTarget(GetPawn());
    SpectatorCamera->Destroy();
    SpectatorCamera = nullptr;
    bHasHunterView = false;
    if (bHideTreasure) SetLocalTreasureMarkerVisible(false);
}

void ATreasureSketchPlayerController::UpdateSpectatorCamera(float DeltaTime)
{
    if (!IsLocalController()) return;
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (GS && PendingSpectatorRoundSerial > 0 && GS->RoundSerial >= PendingSpectatorRoundSerial)
        PendingSpectatorRoundSerial = 0;
    if (GS && GS->Phase != ETreasureRoundPhase::ScoutDrawing) bDrawingOverheadView = false;
    const bool bDrawingView = GS && GS->Phase == ETreasureRoundPhase::ScoutDrawing && bDrawingOverheadView;
    const bool bShouldSpectate = GS && GS->bGameStarted && PendingSpectatorRoundSerial == 0
        && GS->RoomMode != ETreasureRoomMode::HideAndSeek && GS->RoomMode != ETreasureRoomMode::TeamVersus && IsLocalScout() && (GS->Phase == ETreasureRoundPhase::HunterSearching || bDrawingView);
    if (!bShouldSpectate)
    {
        StopSpectating();
        return;
    }
    if (!SpectatorCamera) StartSpectating(bDrawingView);
    if (!SpectatorCamera || bMapOpen) return;

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
    const float MouseSensitivity = bDrawingView ? 0.15f : 0.45f;
    Rotation.Yaw += MouseX * MouseSensitivity;
    Rotation.Pitch = FMath::ClampAngle(Rotation.Pitch - MouseY * MouseSensitivity, -85.f, 85.f);
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
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (!GS || !GS->bGameStarted || !IsLocalScout() || bPauseMenuOpen) return;
    if (GS->Phase == ETreasureRoundPhase::ScoutDrawing)
    {
        if (GS->bSketchSceneLock && bSketchSceneCommitted) return;
        if (bMapOpen) ToggleMap();
        bCameraMode = false;
        bDrawingOverheadView = !bDrawingOverheadView;
        ServerSetDrawingOverheadView(bDrawingOverheadView);
        if (!bDrawingOverheadView) StopSpectating(false);
        else SetSprayCursorMode(false);
        ApplyPhaseInputRules();
        return;
    }
    if (GS->Phase != ETreasureRoundPhase::HunterSearching || !SpectatorCamera || bMapOpen) return;
    SpectatorView = SpectatorView == EScoutSpectatorView::FreeFlight
        ? EScoutSpectatorView::HunterFirstPerson : EScoutSpectatorView::FreeFlight;
    if (SpectatorView == EScoutSpectatorView::HunterFirstPerson && bHasHunterView)
        SpectatorCamera->SetActorLocationAndRotation(HunterViewLocation, HunterViewRotation);
}

void ATreasureSketchPlayerController::ToggleSpectatorTreasure()
{
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    const bool bReviewing = GS && GS->bReviewingRound && GS->IsRoundOver();
    if (((SpectatorCamera && IsLocalScout()) || bReviewing) && bHasScoutTreasureLocation)
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
    if (const auto* ModeState = GetWorld()->GetGameState<ATreasureSketchGameState>();
        ModeState && ModeState->RoomMode == ETreasureRoomMode::HideAndSeek) return;
    if (bPauseMenuOpen) return;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    const bool bReviewing = GS && GS->bReviewingRound && GS->IsRoundOver();
    if (GS && GS->RoomMode == ETreasureRoomMode::TeamVersus
        && GS->Phase == ETreasureRoundPhase::ScoutDrawing && !IsLocalScout()) return;
    const bool bSpectatingMapmaker = GS && GS->bGameStarted
        && GS->Phase == ETreasureRoundPhase::HunterSearching && IsLocalScout();
    if (!bReviewing && !bSpectatingMapmaker && ((GS && GS->IsRoundOver()) || IsHunterWaiting()
        || (IsLocalScout() && (HasSubmittedSketch() || (GS && GS->Phase != ETreasureRoundPhase::ScoutDrawing))))) return;
    if (GS && GS->bSketchSceneLock && GS->RoomMode != ETreasureRoomMode::TeamVersus
        && bSketchSceneCommitted && bMapOpen && IsLocalScout()
        && GS->Phase == ETreasureRoundPhase::ScoutDrawing) return;
    SetSprayCursorMode(false);
    bCameraMode = false;
    ServerCancelDig();
    CancelPropSelection();
    bLocalDigHeld = false;
    bMapOpen = !bMapOpen;
    if (bMapOpen)
    {
        if (ATreasureSketchCharacter* ExplorerCharacter = Cast<ATreasureSketchCharacter>(GetPawn()))
            ExplorerCharacter->PlayReadBookAnimation();
    }
    if (bMapOpen && GS && GS->bSketchSceneLock && IsLocalScout()
        && GS->Phase == ETreasureRoundPhase::ScoutDrawing) bSketchSceneCommitted = true;
    if (!bMapOpen) bPhotoExpanded = false;
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
        if (ATreasureSketchCharacter* ExplorerCharacter = Cast<ATreasureSketchCharacter>(GetPawn()))
            ExplorerCharacter->StopReadBookAnimation();
        SetInputMode(FInputModeGameOnly());
        UE_LOG(LogTemp, Display, TEXT("TREASURE_SKETCH_DRAWING_FOCUS Exploration controls restored"));
    }
}

void ATreasureSketchPlayerController::Handoff()
{
    if (bPauseMenuOpen) return;
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
            : CurrentGS && CurrentGS->RoomMode == ETreasureRoomMode::TeamVersus
                ? TEXT("地图已交给本队藏宝者；等待双方完成后，守护本队宝藏并推开对手。")
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
    bPhotoExpanded = false;
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
    bLiveSketchActive = false;
    SketchPages = Pages;
    ActiveSketchPage = FMath::Clamp(PreviousPage, 0, FMath::Max(0, SketchPages.Num() - 1));
    if (IsLocalScout())
        if (const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>())
            for (int32 PageIndex = 0; PageIndex < SketchPages.Num(); ++PageIndex)
                if (SketchPages[PageIndex].MapmakerId == PS->GetPlayerId())
                { ActiveSketchPage = PageIndex; break; }
    PendingDrawingPoints.Reset();
    bMapOpen = false;
    bPhotoExpanded = false;
    bWasDrawing = false;
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    StatusMessage = GS && GS->RoomMode == ETreasureRoomMode::TeamVersus
        ? TEXT("寻宝开始！按 M 查看队友画的图，长按 E 挖对方的宝藏。")
        : IsLocalScout() ? TEXT("已交图，按 M 查看地图并继续观战。")
        : FString::Printf(TEXT("收到 %d 张地图！M 查看，左右方向键切换图纸，E 挖掘。"), Pages.Num());
    StatusUntil = GetWorld()->GetTimeSeconds() + 8.f;
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
    int32 RoundSerial, int32 MapmakerId, int32 StrokeIndex, uint8 ColorIndex, uint8 EraserSize, const TArray<FVector2D>& Points)
{
    if (!bLiveSketchActive || RoundSerial != CurrentSketchRoundSerial || StrokeIndex < 0) return;
    FSketchPage* Page = SketchPages.FindByPredicate([MapmakerId](const FSketchPage& Candidate)
        { return Candidate.MapmakerId == MapmakerId; });
    if (!Page || StrokeIndex > Page->Strokes.Num()) return;
    if (StrokeIndex == Page->Strokes.Num())
    {
        Page->Strokes.AddDefaulted();
        Page->Strokes.Last().ColorIndex = ColorIndex;
        Page->Strokes.Last().EraserSize = EraserSize;
    }
    if (Page->Strokes[StrokeIndex].ColorIndex != ColorIndex || Page->Strokes[StrokeIndex].EraserSize != EraserSize) return;
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

void ATreasureSketchPlayerController::ClientReceiveLivePhoto_Implementation(
    int32 RoundSerial, int32 MapmakerId, const TArray<uint8>& PhotoJpeg)
{
    if (!bLiveSketchActive || RoundSerial != CurrentSketchRoundSerial) return;
    if (FSketchPage* Page = SketchPages.FindByPredicate([MapmakerId](const FSketchPage& Candidate)
        { return Candidate.MapmakerId == MapmakerId; })) Page->PhotoJpeg = PhotoJpeg;
}

void ATreasureSketchPlayerController::Dig()
{
    if (bPauseMenuOpen || bMapOpen) return;
    if (!GetPawn()) return;
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (PS && GS && GS->RoomMode == ETreasureRoomMode::TeamVersus
        && GS->bGameStarted && GS->Phase == ETreasureRoundPhase::ScoutDrawing
        && PS->PlayerRole == ETreasurePlayerRole::Hunter && !PS->bVersusTreasurePlaced)
    { ServerPlaceVersusTreasure(); return; }
    const ATreasureSketchCharacter* DigCharacter = Cast<ATreasureSketchCharacter>(GetPawn());
    if (PS && GS && DigCharacter && DigCharacter->GetCharacterMovement()->IsMovingOnGround()
        && PS->PlayerRole == ETreasurePlayerRole::Hunter && GS->bGameStarted
        && GS->Phase == ETreasureRoundPhase::HunterSearching
        && PS->GetDigCooldownRemaining(GS->RoundSerial, GS->GetServerWorldTimeSeconds()) <= 0.f)
    {
        if (ATreasureSketchCharacter* ExplorerCharacter = Cast<ATreasureSketchCharacter>(GetPawn())) ExplorerCharacter->PlayDigAnimation();
        bLocalDigHeld = true;
        LocalDigStartedAt = GetWorld()->GetTimeSeconds();
        LocalDigStartLocation = GetPawn()->GetActorLocation();
        ServerTryDig();
    }
}

void ATreasureSketchPlayerController::ServerPlaceVersusTreasure_Implementation()
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        ClientVersusPlacementResult(GM->TryPlaceVersusTreasure(this));
}

void ATreasureSketchPlayerController::ClientVersusPlacementResult_Implementation(bool bPlaced)
{
    StatusMessage = bPlaced ? TEXT("宝藏已埋好！对方地图师现在可以画下这里。")
        : TEXT("这里不能藏宝：请站在陆地上，并远离另一处宝藏。 ");
    StatusUntil = GetWorld()->GetTimeSeconds() + 3.f;
}

void ATreasureSketchPlayerController::StopDig()
{
    if (!bLocalDigHeld) return;
    bLocalDigHeld = false;
    ServerCancelDig();
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

bool ATreasureSketchPlayerController::CanSelectProp() const
{
    const auto* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    const auto* PS = GetPlayerState<ATreasureSketchPlayerState>();
    return GS && PS && GetPawn() && !bPauseMenuOpen && !bMapOpen && !IsFrontEndVisible()
        && GS->bGameStarted && GS->Phase == ETreasureRoundPhase::HunterSearching
        && GS->RoomMode == ETreasureRoomMode::HideAndSeek && PS->PlayerRole == ETreasurePlayerRole::Hunter
        && !PS->bHideEliminated;
}

bool ATreasureSketchPlayerController::HasPropSelectionTarget() const
{
    return bPropSelectionMode && PropSelectionHighlight && PropSelectionHighlight->IsVisible()
        && PropSelectionHighlight->GetStaticMesh();
}

void ATreasureSketchPlayerController::BeginPropSelectionView()
{
    APawn* SelectionPawn = GetPawn();
    if (!SelectionPawn) return;
    PropSelectionBoom = SelectionPawn->FindComponentByClass<USpringArmComponent>();
    PropSelectionCamera = SelectionPawn->FindComponentByClass<UCameraComponent>();
    if (auto* Boom = PropSelectionBoom.Get())
    {
        PropSelectionSavedArmLength = Boom->TargetArmLength;
        PropSelectionSavedOffset = Boom->TargetOffset;
        PropSelectionSavedSocketOffset = Boom->SocketOffset;
        Boom->TargetArmLength = 0.f;
        Boom->TargetOffset = FVector(0.f, 0.f, SelectionPawn->GetDefaultHalfHeight() * 0.8f);
        Boom->SocketOffset = FVector::ZeroVector;
    }
    TArray<UPrimitiveComponent*> Parts;
    SelectionPawn->GetComponents(Parts);
    for (auto* Part : Parts)
    {
        if (Part->ComponentHasTag(TEXT("PropSelectionHighlight"))) continue;
        PropSelectionOwnerVisibility.Add(Part, Part->bOwnerNoSee);
        Part->SetOwnerNoSee(true);
    }
    if (auto* Depth = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth")))
        Depth->Set(3, ECVF_SetByCode);
    if (auto* Camera = PropSelectionCamera.Get())
    {
        auto* Outline = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/UI/Materials/M_PropSelectionOutline.M_PropSelectionOutline"));
        if (Outline) Camera->PostProcessSettings.AddBlendable(Outline, 1.f);
    }
}

void ATreasureSketchPlayerController::CancelPropSelection()
{
    if (auto* Boom = PropSelectionBoom.Get())
    {
        Boom->TargetArmLength = PropSelectionSavedArmLength;
        Boom->TargetOffset = PropSelectionSavedOffset;
        Boom->SocketOffset = PropSelectionSavedSocketOffset;
    }
    for (const auto& Pair : PropSelectionOwnerVisibility)
        if (auto* Part = Pair.Key.Get()) Part->SetOwnerNoSee(Pair.Value);
    if (auto* Camera = PropSelectionCamera.Get())
        if (auto* Outline = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/UI/Materials/M_PropSelectionOutline.M_PropSelectionOutline")))
            Camera->PostProcessSettings.RemoveBlendable(Outline);
    PropSelectionOwnerVisibility.Empty();
    PropSelectionBoom.Reset();
    PropSelectionCamera.Reset();
    bPropSelectionMode = false;
    bPropButtonHeld = false;
    PropSelectionRoundSerial = 0;
    if (PropSelectionHighlight)
    {
        PropSelectionHighlight->SetVisibility(false);
        PropSelectionHighlight->SetHiddenInGame(true);
    }
}

void ATreasureSketchPlayerController::ReleasePropSelection()
{
    if (!bPropButtonHeld) return;
    const auto* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    const bool bConfirm = bPropSelectionMode && CanSelectProp() && GS
        && GS->RoundSerial == PropSelectionRoundSerial && HasPropSelectionTarget();
    // Use the ray that produced the displayed outline before restoring the camera.
    const FVector Origin = PropSelectionOrigin;
    const FVector Direction = PropSelectionDirection;
    const int32 RoundSerial = PropSelectionRoundSerial;
    CancelPropSelection();
    if (bConfirm) ServerTransformIntoProp(RoundSerial, Origin, Direction, false);
}

void ATreasureSketchPlayerController::UpdatePropSelectionTarget(const FVector& Origin, const FVector& Direction)
{
    PropSelectionOrigin = Origin;
    PropSelectionDirection = Direction;
    FPropDisguise Form;
    PropDisguise::FTarget Target;
    if (!PropDisguise::FindTarget(GetWorld(), Origin, Direction, Form, &Target))
    {
        if (PropSelectionHighlight)
        {
            PropSelectionHighlight->SetVisibility(false);
            PropSelectionHighlight->SetHiddenInGame(true);
        }
        return;
    }
    if (IsValid(PropSelectionHighlight) && PropSelectionHighlight->GetOwner() != GetPawn())
    {
        PropSelectionHighlight->DestroyComponent();
        PropSelectionHighlight = nullptr;
    }
    if (!IsValid(PropSelectionHighlight))
    {
        // Local depth proxy isolates the selected instance for the outline pass.
        PropSelectionHighlight = NewObject<UStaticMeshComponent>(GetPawn() ? static_cast<UObject*>(GetPawn()) : static_cast<UObject*>(this), TEXT("PropSelectionHighlight"));
        PropSelectionHighlight->ComponentTags.Add(TEXT("PropSelectionHighlight"));
        PropSelectionHighlight->SetIsReplicated(false);
        PropSelectionHighlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PropSelectionHighlight->SetGenerateOverlapEvents(false);
        PropSelectionHighlight->SetCastShadow(false);
        PropSelectionHighlight->SetMobility(EComponentMobility::Movable);
        PropSelectionHighlight->RegisterComponentWithWorld(GetWorld());
        // A depth-only proxy marks exactly this instance. Its visible source keeps its materials.
        PropSelectionHighlight->SetRenderInMainPass(false);
        PropSelectionHighlight->SetRenderInDepthPass(false);
        PropSelectionHighlight->SetRenderCustomDepth(true);
        PropSelectionHighlight->SetCustomDepthStencilValue(253);
    }
    PropSelectionHighlight->SetStaticMesh(Form.Mesh);
    PropSelectionHighlight->SetWorldTransform(Target.Transform);
    PropSelectionHighlight->SetVisibility(true);
    PropSelectionHighlight->SetHiddenInGame(false);
}

void ATreasureSketchPlayerController::UpdatePropSelection(float DeltaSeconds)
{
    if (!IsLocalController()) return;
    const auto* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!CanSelectProp() || ((bPropSelectionMode || bPropButtonHeld) && GS->RoundSerial != PropSelectionRoundSerial))
    {
        CancelPropSelection();
        return;
    }
    if (!bPropSelectionMode) return;
    FVector Origin;
    FRotator Rotation;
    GetPlayerViewPoint(Origin, Rotation);
    FVector AimDirection = Rotation.Vector();
    int32 ViewWidth = 0, ViewHeight = 0;
    GetViewportSize(ViewWidth, ViewHeight);
    if (ViewWidth > 0 && ViewHeight > 0)
        DeprojectScreenPositionToWorld(ViewWidth * 0.5f, ViewHeight * 0.5f, Origin, AimDirection);
    UpdatePropSelectionTarget(Origin, AimDirection);
}

void ATreasureSketchPlayerController::TransformIntoProp()
{
    if (!CanSelectProp()) { CancelPropSelection(); return; }
    const auto* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if ((bPropSelectionMode || bPropButtonHeld) && GS->RoundSerial != PropSelectionRoundSerial) CancelPropSelection();
    if (bPropButtonHeld) return;
    bPropButtonHeld = true;
    PropSelectionRoundSerial = GS->RoundSerial;
    bPropSelectionMode = true;
    bCameraMode = false;
    BeginPropSelectionView();
}

void ATreasureSketchPlayerController::RestoreHumanForm()
{
    const auto* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || bPauseMenuOpen || bMapOpen) return;
    CancelPropSelection();
    ServerTransformIntoProp(GS->RoundSerial, FVector::ZeroVector, FVector::ForwardVector, true);
}

void ATreasureSketchPlayerController::ServerTransformIntoProp_Implementation(int32 RoundSerial,
    FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, bool bRestore)
{
    const auto* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    const auto* PS = GetPlayerState<ATreasureSketchPlayerState>();
    auto* PropCharacter = Cast<ATreasureSketchCharacter>(GetPawn());
    if (!GS || !PS || !PropCharacter || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::HunterSearching
        || GS->RoundSerial != RoundSerial || GS->RoomMode != ETreasureRoomMode::HideAndSeek
        || PS->PlayerRole != ETreasurePlayerRole::Hunter || PS->bHideEliminated) return;
    if (bRestore)
    {
        PropCharacter->SetPropDisguise(FPropDisguise());
        ClientPropDisguiseFeedback(true, true);
        return;
    }
    // Validate the camera origin, without imposing a distance limit on the prop.
    if (FVector(Origin).ContainsNaN() || FVector(Direction).ContainsNaN()
        || FVector::DistSquared(Origin, PropCharacter->GetActorLocation())
            > FMath::Square(PropCharacter->GetDisguiseViewDistance() + 300.f)) return;
    FPropDisguise Form;
    const bool bFound = PropDisguise::FindTarget(GetWorld(), Origin, Direction, Form);
    if (bFound) PropCharacter->SetPropDisguise(Form);
    ClientPropDisguiseFeedback(bFound, false);
}

void ATreasureSketchPlayerController::ClientPropDisguiseFeedback_Implementation(bool bFound, bool bRestore)
{
    StatusMessage = bRestore ? TEXT("已恢复人形。") : bFound ? TEXT("已变形；按住左键选取，松开变形，Q 恢复人形。")
        : TEXT("未瞄准物品；按住左键打开取景框，瞄准高亮物品后松开变形。");
    StatusUntil = GetWorld()->GetTimeSeconds() + 3.f;
}

void ATreasureSketchPlayerController::Shove()
{
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    const ATreasureSketchPlayerState* PS = GetPlayerState<ATreasureSketchPlayerState>();
    if (!bPauseMenuOpen && !bMapOpen && GetPawn() && GS && PS
        && (GS->RoomMode == ETreasureRoomMode::ExplorerRace || GS->RoomMode == ETreasureRoomMode::HideAndSeek || GS->RoomMode == ETreasureRoomMode::TeamVersus) && GS->bGameStarted
        && GS->Phase == ETreasureRoundPhase::HunterSearching
        && PS->PlayerRole == ((GS->RoomMode == ETreasureRoomMode::HideAndSeek || GS->RoomMode == ETreasureRoomMode::TeamVersus) ? ETreasurePlayerRole::Scout : ETreasurePlayerRole::Hunter)
        && !PS->bDigging
        && PS->NextShoveServerTime <= GS->GetServerWorldTimeSeconds()) ServerTryShove();
}

void ATreasureSketchPlayerController::ServerTryShove_Implementation()
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->TryShove(this);
}

void ATreasureSketchPlayerController::ServerTryDig_Implementation()
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->StartHeldDig(this);
}

void ATreasureSketchPlayerController::ServerCancelDig_Implementation()
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->CancelHeldDig(GetPlayerState<ATreasureSketchPlayerState>());
}

void ATreasureSketchPlayerController::ClientDigResult_Implementation(bool bFound, int32 FeedbackValue, bool bRace)
{
    bLocalDigHeld = false;
    DigFeedbackBand = bFound ? -1 : FMath::Clamp(FeedbackValue, 0, 4);
    DigFeedbackUntil = bFound ? 0.f : GetWorld()->GetTimeSeconds() + 3.f;
    StatusMessage = bFound ? (bRace ? TEXT("你率先找到宝藏！") : TEXT("找到宝藏！")) : FString();
    StatusUntil = bFound ? GetWorld()->GetTimeSeconds() + 4.f : 0.f;
}

void ATreasureSketchPlayerController::ClientDigInterrupted_Implementation()
{
    bLocalDigHeld = false;
    StatusMessage = TEXT("挖掘被打断；未消耗挖掘冷却。");
    StatusUntil = GetWorld()->GetTimeSeconds() + 2.f;
}

void ATreasureSketchPlayerController::ClientShoveFeedback_Implementation(uint8 Result, const FString& OtherName)
{
    PlayActionCue(this, Result);
    const auto* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (GS && GS->RoomMode == ETreasureRoomMode::HideAndSeek)
    {
        StatusMessage = Result == 0 ? TEXT("正在抓捕……") : TEXT("没抓到，等待冷却后再试。");
        StatusUntil = GetWorld()->GetTimeSeconds() + 2.f;
        return;
    }
    StatusMessage = Result == 0 ? TEXT("正在出手推人……")
        : Result == 1 ? FString::Printf(TEXT("推中了 %s！"), *OtherName)
        : Result == 3 ? FString::Printf(TEXT("你被 %s 推开了！"), *OtherName)
        : Result == 4 ? TEXT("对手刚被推过，暂时推不动；本次出手已冷却。")
        : TEXT("推空了，等待冷却后再试。");
    StatusUntil = GetWorld()->GetTimeSeconds() + (Result == 0 ? 0.5f : 2.5f);
    if (Result != 0)
    {
        ActionFeedbackKind = Result;
        ActionFeedbackUntil = GetWorld()->GetTimeSeconds() + 1.2f;
    }
}

void ATreasureSketchPlayerController::ClientTerrainFeedback_Implementation(uint8 Kind)
{
    PlayActionCue(this, Kind == 1 ? 5 : Kind == 2 ? 6 : 7);
    ActionFeedbackKind = Kind == 1 ? 5 : Kind == 2 ? 6 : 7;
    ActionFeedbackUntil = GetWorld()->GetTimeSeconds() + 1.4f;
    if (Kind == 3 && StatusMessage.Contains(TEXT("推开"))) StatusMessage += TEXT(" 斜坡滑行！");
    else StatusMessage = Kind == 1 ? TEXT("落入浅水，短暂减速；跳跃可上岸！")
        : Kind == 2 ? TEXT("跌入深水，已送回安全地带。") : TEXT("被推上斜坡，滑了一段！");
    StatusUntil = GetWorld()->GetTimeSeconds() + 2.5f;
}

void ATreasureSketchPlayerController::ClientStartNewRound_Implementation(int32 NewRoundSerial)
{
    CancelPropSelection();
    bLocalDigHeld = false;
    bPauseMenuOpen = false;
    bDrawingOverheadView = false;
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
    LocalPhotoJpeg.Reset();
    if (HasAuthority()) ResetRoundPhoto();
    SketchPages.Reset();
    bLiveSketchActive = false;
    ActiveSketchPage = 0;
    bLocalSketchSubmitted = false;
    bSketchSceneCommitted = false;
    SelectedInkColor = 0;
    PaperRotationSteps = 0;
    DigFeedbackBand = -1;
    DigFeedbackUntil = 0.f;
    CurrentSketchRoundSerial = NewRoundSerial;
    PendingDrawingPoints.Reset();
    NextDrawingSyncTime = 0.f;
    bMapOpen = false;
    bPhotoExpanded = false;
    bCameraMode = false;
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

void ATreasureSketchPlayerController::ClientBeginReview_Implementation(
    int32 RoundSerial, const TArray<FSketchPage>& Pages, FVector_NetQuantize TreasureLocation)
{
    if (RoundSerial < CurrentSketchRoundSerial) return;
    CurrentSketchRoundSerial = RoundSerial;
    bLiveSketchActive = false;
    if (!Pages.IsEmpty())
    {
        SketchPages = Pages;
        ActiveSketchPage = FMath::Clamp(ActiveSketchPage, 0, SketchPages.Num() - 1);
    }
    bDrawingOverheadView = false;
    StopSpectating();
    ScoutTreasureLocation = TreasureLocation;
    bHasScoutTreasureLocation = true;
    SetLocalTreasureMarkerVisible(false);
    StatusMessage = TEXT("复盘中：可在岛上自由查看，按 M 对照图纸。");
    StatusUntil = GetWorld()->GetTimeSeconds() + 5.f;
}

void ATreasureSketchPlayerController::ClientEndReview_Implementation(int32 RoundSerial)
{
    if (RoundSerial != CurrentSketchRoundSerial) return;
    SetLocalTreasureMarkerVisible(false);
    bMapOpen = false;
    bShowMouseCursor = true;
    StatusMessage.Empty();
}

void ATreasureSketchPlayerController::ClearSketch()
{
    if (bPauseMenuOpen) return;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!IsLocalScout() || HasSubmittedSketch() || !GS || !GS->bGameStarted || GS->Phase != ETreasureRoundPhase::ScoutDrawing) return;
    Strokes.Reset();
    PendingDrawingPoints.Reset();
    bWasDrawing = false;
    ServerClearDrawing(GS->RoundSerial);
}

void ATreasureSketchPlayerController::ClientReturnToLobby_Implementation()
{
    bLocalDigHeld = false;
    bCameraMode = false;
    bPauseMenuOpen = false;
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
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
    {
        if (!GM->RefreshSoloMap()) GM->StartNewRound();
    }
}

void ATreasureSketchPlayerController::RequestReplay(bool bSwapRoles)
{
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (GS && GS->IsRoundOver() && !GS->bReviewingRound)
        ServerRequestReplay(bSwapRoles);
}

void ATreasureSketchPlayerController::RequestRoundReview(bool bReviewing)
{
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    if (GS && GS->bGameStarted && GS->IsRoundOver() && GS->bReviewingRound != bReviewing)
        ServerRequestRoundReview(bReviewing);
}

void ATreasureSketchPlayerController::ServerRequestRoundReview_Implementation(bool bReviewing)
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->SetRoundReview(bReviewing);
}

void ATreasureSketchPlayerController::ServerClaimSingleRoomRole_Implementation()
{
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->ClaimSingleRoomRole(GetPlayerState<ATreasureSketchPlayerState>());
}

void ATreasureSketchPlayerController::ServerRequestReplay_Implementation(bool bSwapRoles)
{
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (!GS || !GS->IsRoundOver() || GS->bReviewingRound) return;
    if (ATreasureSketchGameMode* GM = GetWorld()->GetAuthGameMode<ATreasureSketchGameMode>())
        GM->StartNewRound(bSwapRoles, bSwapRoles ? GetPlayerState<ATreasureSketchPlayerState>() : nullptr);
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
