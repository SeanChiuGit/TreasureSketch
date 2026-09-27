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

    bool IsRoundOver() const
    {
        return Phase == ETreasureRoundPhase::Won
            || Phase == ETreasureRoundPhase::ScoutTimedOut
            || Phase == ETreasureRoundPhase::HunterTimedOut;
    }

    int32 GetSecondsRemaining() const;
};
