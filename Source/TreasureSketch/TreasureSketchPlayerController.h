#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SketchTypes.h"
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
    SoloTest,
    Settings
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
    int32 GetSketchPageCount() const { return SketchPages.Num(); }
    int32 GetActiveSketchPage() const { return ActiveSketchPage; }
    FString GetActiveMapmakerName() const { return SketchPages.IsValidIndex(ActiveSketchPage) ? SketchPages[ActiveSketchPage].MapmakerName : FString(); }
    bool HasSubmittedSketch() const;
    void CycleSketchPage(int32 Direction);
    const TArray<FSketchStroke>& GetServerDrawing() const;
    FString GetStatusMessage() const { return StatusMessage; }
    FVector2D GetPaperMin() const;
    FVector2D GetPaperSize() const;
    void ClearSketch();
    void RequestReplay(bool bSwapRoles = false);
    bool IsLocalScout() const;
    bool IsHunterWaiting() const;
    bool IsScoutSpectating() const { return SpectatorCamera != nullptr; }
    bool IsHunterFirstPersonView() const { return SpectatorView == EScoutSpectatorView::HunterFirstPerson; }
    bool IsSpectatorTreasureVisible() const { return bTreasureMarkerVisible; }
    bool IsFrontEndVisible() const;
    EFrontEndPage GetFrontEndPage() const { return FrontEndPage; }
    void HandleFrontEndAction(FName ActionName);

    UFUNCTION(Client, Reliable)
    void ClientReceiveSketch(const TArray<FSketchStroke>& CompletedStrokes);

    UFUNCTION(Client, Reliable)
    void ClientReceiveSketchPages(int32 RoundSerial, const TArray<FSketchPage>& Pages);

    UFUNCTION(Client, Reliable)
    void ClientDigResult(bool bFound, float Distance);

    UFUNCTION(Client, Reliable)
    void ClientStartNewRound(int32 NewRoundSerial);

    UFUNCTION(Client, Reliable)
    void ClientRevealTreasure(FVector_NetQuantize TreasureLocation);

    UFUNCTION(Client, Reliable)
    void ClientHideTreasure();

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
    bool bLocalSketchSubmitted = false;
    int32 CurrentSketchRoundSerial = 0;

    UPROPERTY()
    TArray<FSketchStroke> ServerDrawing;
    TArray<FVector2D> PendingDrawingPoints;
    float NextDrawingSyncTime = 0.f;
    int32 ServerDrawingRoundSerial = 0;
    void FlushDrawingPoints();
    UFUNCTION(Server, Reliable)
    void ServerAppendDrawing(int32 RoundSerial, int32 StrokeIndex, const TArray<FVector2D>& Points);
    UFUNCTION(Server, Reliable)
    void ServerClearDrawing(int32 RoundSerial);

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
    int32 PendingSpectatorRoundSerial = 0;

    bool bMapOpen = false;
    bool bWasDrawing = false;
    bool bReplayInputActive = false;
    bool bInputLocked = false;
    FString StatusMessage;
    float StatusUntil = 0.f;
    EFrontEndPage FrontEndPage = EFrontEndPage::None;
    bool bFrontEndInputActive = false;

    void ToggleMap();
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
    void ApplyKeyboardMovementFallback();
    void UpdateReplayInput();
    void UpdateSpectatorCamera(float DeltaTime);
    void StartSpectating();
    void StopSpectating();
    void SetLocalTreasureMarkerVisible(bool bVisible);
    void UpdateFrontEnd();
    void OpenFrontEndPage(EFrontEndPage NewPage);
    ATreasureSketchCharacter* FindHunterCharacter() const;

    UFUNCTION(Server, Reliable)
    void ServerSubmitSketch(int32 RoundSerial, const TArray<FSketchStroke>& CompletedStrokes);

    UFUNCTION(Server, Reliable)
    void ServerTryDig(FVector_NetQuantize WorldLocation);

    UFUNCTION(Server, Reliable)
    void ServerRequestReplay(bool bSwapRoles);
};
