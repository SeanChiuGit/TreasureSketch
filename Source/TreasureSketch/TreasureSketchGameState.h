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

    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bSurfacePaintEnabled = false;

    // Room settings survive replay and are authoritative on the host.
    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 RoomGridSize = 39;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 DrawingDurationSeconds = 120;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 SearchingDurationSeconds = 120;

    static constexpr int32 RoomGridSizes[] = {25, 39, 51, 61};
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
