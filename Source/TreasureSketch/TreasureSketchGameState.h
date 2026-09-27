#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TreasureSketchGameState.generated.h"

UENUM(BlueprintType)
enum class ETreasureRoundPhase : uint8
{
    ScoutDrawing,
    HunterSearching,
    Won,
    ScoutTimedOut,
    HunterTimedOut
};

UENUM(BlueprintType)
enum class ETreasureRoomMode : uint8
{
    OneMapmaker,
    OneExplorer,
    TeamVersus
};

UCLASS()
class TREASURESKETCH_API ATreasureSketchGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxRoomPlayers = 4;
    ATreasureSketchGameState();

    UPROPERTY(Replicated, BlueprintReadOnly)
    ETreasureRoomMode RoomMode = ETreasureRoomMode::OneMapmaker;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // The room's random map pool. Ruins remain outside the normal pool.
    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bBeachInMapPool = true;

    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bForestInMapPool = true;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 IslandSeed = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 RoundSerial = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    ETreasureRoundPhase Phase = ETreasureRoundPhase::ScoutDrawing;

    UPROPERTY(Replicated, BlueprintReadOnly)
    float RoundEndServerTime = 60.f;

    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bGameStarted = false;

    // Keep the result phase while temporarily letting the party inspect the island.
    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bReviewingRound = false;

    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bSurfacePaintEnabled = false;

    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bTreasureRangeVisible = true;

    // Nearby uses the same landing area; spread out uses different safe land samples.
    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bSpreadPlayerSpawns = false;

    // Room settings survive replay and are authoritative on the host.
    UPROPERTY(Replicated, BlueprintReadOnly)
    float RoomMapScale = 1.f;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 DrawingDurationSeconds = 120;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 SearchingDurationSeconds = 120;

    UPROPERTY(ReplicatedUsing=ApplyMovementSpeed, BlueprintReadOnly)
    float MovementSpeedMultiplier = 1.f;

    static constexpr float MinMovementSpeed = 0.5f;
    static constexpr float MaxMovementSpeed = 5.f;
    UFUNCTION()
    void ApplyMovementSpeed();

    static constexpr float MinMapScale = 0.5f;
    static constexpr float MaxMapScale = 5.f;
    static constexpr int32 MinPhaseSeconds = 30;
    static constexpr int32 MaxPhaseSeconds = 600;
    static constexpr int32 PhaseSecondsStep = 30;

    bool IsRoundOver() const
    {
        return Phase == ETreasureRoundPhase::Won
            || Phase == ETreasureRoundPhase::ScoutTimedOut
            || Phase == ETreasureRoundPhase::HunterTimedOut;
    }

    int32 GetSecondsRemaining() const;
};
