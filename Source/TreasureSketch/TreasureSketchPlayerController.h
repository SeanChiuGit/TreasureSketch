#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SketchTypes.h"
#include "TreasureHistorySave.h"
#include "TreasureSketchPlayerController.generated.h"

class ATreasureMarker;
class ACameraActor;
class ATreasureSketchCharacter;
class APlayerState;

enum class EScoutSpectatorView : uint8
{
    FreeFlight,
    HunterFirstPerson
};

enum class EFrontEndPage : uint8
{
    None,
    MainMenu,
    JoinBrowser,
    RoomLobby,
    RoomDrawingRules,
    SoloTest,
    Settings,
    History
};

UCLASS()
class TREASURESKETCH_API ATreasureSketchPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ATreasureSketchPlayerController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    FString GetTestSeedText() const { return TestSeedText; }
    bool IsTestSeedEditing() const { return bTestSeedEditing; }
    bool IsMapScaleEditing() const { return bMapScaleEditing; }
    FString GetMapScaleText() const { return MapScaleText; }
    int32 GetTestSeed() const;

    int32 GetSpectatedHunterIndex() const { return SpectatedHunterIndex; }
    UFUNCTION(Client, Reliable)
    void ClientReturnToLobby();
    bool IsSprayCursorMode() const { return bSprayCursorMode; }
    bool IsMapOpen() const { return bMapOpen; }
    const TArray<FSketchStroke>& GetStrokes() const { return SketchPages.IsValidIndex(ActiveSketchPage) ? SketchPages[ActiveSketchPage].Strokes : Strokes; }
    const TArray<uint8>& GetPhotoJpeg() const { return SketchPages.IsValidIndex(ActiveSketchPage) ? SketchPages[ActiveSketchPage].PhotoJpeg : LocalPhotoJpeg; }
    bool HasTakenPhoto() const { return !LocalPhotoJpeg.IsEmpty(); }
    bool IsPhotoExpanded() const { return bPhotoExpanded; }
    int32 GetPaperRotationSteps() const { return PaperRotationSteps; }
    void RotatePaper(int32 Direction);
    uint8 GetSelectedInkColor() const { return SelectedInkColor; }
    uint8 GetSelectedEraserSize() const { return SelectedEraserSize; }
    const TArray<FPlayedRoundRecord>& GetHistoryRecords() const;
    int32 GetHistoryGroupCount() const;
    int32 GetHistoryListOffset() const { return HistoryListOffset; }
    int32 GetSelectedHistoryIndex() const { return SelectedHistoryIndex; }
    int32 GetHistorySketchPageIndex() const { return HistorySketchPageIndex; }
    const FPlayedRoundRecord* GetSelectedHistoryRecord() const;
    int32 GetHistoryGroupStart(int32 GroupIndex) const;
    int32 GetInkUsed() const;
    int32 GetDigFeedbackBand() const { return DigFeedbackBand; }
    float GetDigFeedbackRemaining() const;
    int32 GetSketchPageCount() const { return SketchPages.Num(); }
    int32 GetActiveSketchPage() const { return ActiveSketchPage; }
    FString GetActiveMapmakerName() const { return SketchPages.IsValidIndex(ActiveSketchPage) ? SketchPages[ActiveSketchPage].MapmakerName : FString(); }
    bool HasSubmittedSketch() const;
    void CycleSketchPage(int32 Direction);
    const TArray<FSketchStroke>& GetServerDrawing() const;
    const TArray<uint8>& GetServerPhoto() const { return ServerPhotoJpeg; }
    void ResetRoundPhoto();
    FString GetStatusMessage() const { return StatusMessage; }
    FVector2D GetPaperMin() const;
    FVector2D GetPaperSize() const;
    void ClearSketch();
    void Shove();
    void RequestReplay(bool bSwapRoles = false);
    void RequestRoundReview(bool bReviewing);
    bool IsLocalScout() const;
    bool IsHunterWaiting() const;
    bool IsScoutSpectating() const { return SpectatorCamera != nullptr; }
    bool IsDrawingOverheadView() const { return bDrawingOverheadView; }
    bool IsHunterFirstPersonView() const { return SpectatorView == EScoutSpectatorView::HunterFirstPerson; }
    bool IsSpectatorTreasureVisible() const { return bTreasureMarkerVisible; }
    bool IsPauseMenuOpen() const { return bPauseMenuOpen; }
    bool IsFrontEndVisible() const;
    EFrontEndPage GetFrontEndPage() const { return FrontEndPage; }
    void HandleFrontEndAction(FName ActionName);

    UFUNCTION(Client, Reliable)
    void ClientReceiveSketch(const TArray<FSketchStroke>& CompletedStrokes);

    UFUNCTION(Client, Reliable)
    void ClientReceiveSketchPages(int32 RoundSerial, const TArray<FSketchPage>& Pages);

    UFUNCTION(Client, Reliable)
    void ClientInitializeLiveSketch(int32 RoundSerial, const TArray<FSketchPage>& Pages);

    UFUNCTION(Client, Reliable)
    void ClientAppendLiveSketch(int32 RoundSerial, int32 MapmakerId, int32 StrokeIndex, uint8 ColorIndex, uint8 EraserSize, const TArray<FVector2D>& Points);

    UFUNCTION(Client, Reliable)
    void ClientRecordCompletedRound(const FPlayedRoundRecord& Record);

    UFUNCTION(Client, Reliable)
    void ClientClearLiveSketch(int32 RoundSerial, int32 MapmakerId);

    UFUNCTION(Client, Reliable)
    void ClientReplaceLiveSketch(int32 RoundSerial, const FSketchPage& Page);

    UFUNCTION(Client, Reliable)
    void ClientReceiveLivePhoto(int32 RoundSerial, int32 MapmakerId, const TArray<uint8>& PhotoJpeg);

    UFUNCTION(Client, Reliable)
    void ClientDigResult(bool bFound, int32 FeedbackValue, bool bRace);

    UFUNCTION(Client, Reliable)
    void ClientShoveFeedback(uint8 Result);

    UFUNCTION(Client, Reliable)
    void ClientStartNewRound(int32 NewRoundSerial);

    UFUNCTION(Client, Reliable)
    void ClientRevealTreasure(FVector_NetQuantize TreasureLocation);

    UFUNCTION(Client, Reliable)
    void ClientHideTreasure();

    UFUNCTION(Client, Reliable)
    void ClientBeginReview(int32 RoundSerial, const TArray<FSketchPage>& Pages, FVector_NetQuantize TreasureLocation);

    UFUNCTION(Client, Reliable)
    void ClientEndReview(int32 RoundSerial);

    UFUNCTION(Client, Unreliable)
    void ClientUpdateHunterView(FVector_NetQuantize ViewLocation, FRotator ViewRotation, APlayerState* ViewedPlayer);

private:
    friend class FMultiMapmakerFlowTest;
    bool bMapScaleEditing = false;
    bool bReplaceMapScaleText = false;
    FString MapScaleText;
    bool CommitMapScale();
    bool bSprayCursorMode = false;
    bool bLookInputLocked = false;
    void SetSprayCursorMode(bool bEnabled);
    float NextSpraySampleTime = 0.f;
    float NextServerSprayTime = 0.f;
    UFUNCTION(Server, Unreliable)
    void ServerSpraySurface(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection);
    int32 SpectatedHunterIndex = 0;
    UPROPERTY()
    TObjectPtr<APlayerState> ViewedHunterState;
    UFUNCTION(Server, Reliable)
    void ServerCycleSpectatedHunter();
    UPROPERTY()
    FString TestSeedText;
    UPROPERTY()
    bool bTestSeedEditing = false;
    UPROPERTY()
    TArray<FSketchStroke> Strokes;
    UPROPERTY()
    TArray<FSketchPage> SketchPages;
    int32 ActiveSketchPage = 0;
    bool bLiveSketchActive = false;
    bool bLocalSketchSubmitted = false;
    int32 CurrentSketchRoundSerial = 0;

    UPROPERTY()
    TArray<FSketchStroke> ServerDrawing;
    UPROPERTY()
    TArray<uint8> LocalPhotoJpeg;
    UPROPERTY()
    TArray<uint8> ServerPhotoJpeg;
    bool bServerDrawingOverheadView = false;
    TArray<FVector2D> PendingDrawingPoints;
    float NextDrawingSyncTime = 0.f;
    int32 ServerDrawingRoundSerial = 0;
    uint8 SelectedInkColor = 0;
    uint8 SelectedEraserSize = 0;
    UPROPERTY() TObjectPtr<UTreasureHistorySave> HistorySave;
    int32 HistoryListOffset = 0;
    int32 SelectedHistoryIndex = -1;
    int32 HistorySketchPageIndex = 0;
    void LoadHistory();
    bool bSketchSceneCommitted = false;
    int32 DigFeedbackBand = -1;
    float DigFeedbackUntil = 0.f;
    void FlushDrawingPoints();
    UFUNCTION(Server, Reliable)
    void ServerAppendDrawing(int32 RoundSerial, int32 StrokeIndex, uint8 ColorIndex, uint8 EraserSize, const TArray<FVector2D>& Points);
    UFUNCTION(Server, Reliable)
    void ServerClearDrawing(int32 RoundSerial);

    UFUNCTION(Server, Reliable)
    void ServerSubmitPhoto(int32 RoundSerial, FVector_NetQuantize ViewOrigin, FRotator ViewRotation,
        const TArray<uint8>& PhotoJpeg);

    UFUNCTION(Server, Reliable)
    void ServerSetDrawingOverheadView(bool bOverhead);

    UPROPERTY()
    TObjectPtr<ATreasureMarker> LocalScoutMarker;

    UPROPERTY()
    TObjectPtr<ACameraActor> SpectatorCamera;

    UPROPERTY()
    TObjectPtr<ACameraActor> MenuCamera;

    FVector ScoutTreasureLocation = FVector::ZeroVector;
    FVector HunterViewLocation = FVector::ZeroVector;
    FRotator HunterViewRotation = FRotator::ZeroRotator;
    float HunterViewUpdatedAt = 0.f;
    EScoutSpectatorView SpectatorView = EScoutSpectatorView::FreeFlight;
    bool bHasScoutTreasureLocation = false;
    bool bTreasureMarkerVisible = false;
    bool bHasHunterView = false;
    bool bDrawingOverheadView = false;
    int32 PendingSpectatorRoundSerial = 0;

    bool bMapOpen = false;
    uint8 PaperRotationSteps = 0;
    bool bPhotoExpanded = false;
    bool bWaitingSketchInputActive = false;
    bool bWasDrawing = false;
    bool bReplayInputActive = false;
    bool bPauseMenuOpen = false;
    bool bInputLocked = false;
    FString StatusMessage;
    float StatusUntil = 0.f;
    EFrontEndPage FrontEndPage = EFrontEndPage::None;
    bool bFrontEndInputActive = false;

    void ToggleMap();
    void TakePhoto();
    void Handoff();
    void Dig();
    void NewRound();
    void HostOnlineGame();
    void JoinOnlineGame();
    void ConfirmJoinOnlineGame();
    void StartOnlineRound();
    void InviteSteamFriend();
    void ToggleSpectatorView();
    void ToggleSpectatorTreasure();
    void ToggleWeatherFog();
    bool IsPointOnPaper(const FVector2D& Point) const;
    void ApplyPhaseInputRules();
    void UpdateWaitingSketchInput();
    void ApplyKeyboardMovementFallback();
    void UpdateReplayInput();
    void UpdateSpectatorCamera(float DeltaTime);
    void StartSpectating(bool bDrawingView = false);
    void StopSpectating(bool bHideTreasure = true);
    void SetLocalTreasureMarkerVisible(bool bVisible);
    void UpdateFrontEnd();
    void SetPauseMenuOpen(bool bOpen);
    void OpenFrontEndPage(EFrontEndPage NewPage);
    ATreasureSketchCharacter* FindHunterCharacter() const;

    UFUNCTION(Server, Reliable)
    void ServerSubmitSketch(int32 RoundSerial, const TArray<FSketchStroke>& CompletedStrokes);

    UFUNCTION(Server, Reliable)
    void ServerTryDig();

    UFUNCTION(Server, Reliable)
    void ServerTryShove();

    UFUNCTION(Server, Reliable)
    void ServerRequestReplay(bool bSwapRoles);

    UFUNCTION(Server, Reliable)
    void ServerRequestRoundReview(bool bReviewing);

    UFUNCTION(Server, Reliable)
    void ServerClaimSingleRoomRole();
};
