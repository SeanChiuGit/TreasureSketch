#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SketchTypes.h"
#include "TreasureSketchPlayerController.generated.h"

class ATreasureMarker;

UCLASS()
class TREASURESKETCH_API ATreasureSketchPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ATreasureSketchPlayerController();
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;

    bool IsMapOpen() const { return bMapOpen; }
    const TArray<FSketchStroke>& GetStrokes() const { return Strokes; }
    FString GetStatusMessage() const { return StatusMessage; }
    FVector2D GetPaperMin() const;
    FVector2D GetPaperSize() const;
    void ClearSketch();
    void RequestReplay(bool bSwapRoles = false);
    bool IsLocalScout() const;
    bool IsHunterWaiting() const;

    UFUNCTION(Client, Reliable)
    void ClientReceiveSketch(const TArray<FSketchStroke>& CompletedStrokes);

    UFUNCTION(Client, Reliable)
    void ClientDigResult(bool bFound, float Distance);

    UFUNCTION(Client, Reliable)
    void ClientStartNewRound();

    UFUNCTION(Client, Reliable)
    void ClientRevealTreasure(FVector_NetQuantize TreasureLocation);

    UFUNCTION(Client, Reliable)
    void ClientHideTreasure();

private:
    UPROPERTY()
    TArray<FSketchStroke> Strokes;

    UPROPERTY()
    TObjectPtr<ATreasureMarker> LocalScoutMarker;

    bool bMapOpen = false;
    bool bWasDrawing = false;
    bool bReplayInputActive = false;
    bool bInputLocked = false;
    FString StatusMessage;
    float StatusUntil = 0.f;

    void ToggleMap();
    void Handoff();
    void Dig();
    void NewRound();
    void HostOnlineGame();
    void JoinOnlineGame();
    void ConfirmJoinOnlineGame();
    void StartOnlineRound();
    void InviteSteamFriend();
    bool IsPointOnPaper(const FVector2D& Point) const;
    void ApplyPhaseInputRules();
    void UpdateReplayInput();

    UFUNCTION(Server, Reliable)
    void ServerSubmitSketch(const TArray<FSketchStroke>& CompletedStrokes);

    UFUNCTION(Server, Reliable)
    void ServerTryDig(FVector_NetQuantize WorldLocation);

    UFUNCTION(Server, Reliable)
    void ServerRequestReplay(bool bSwapRoles);
};
