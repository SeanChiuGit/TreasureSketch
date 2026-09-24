#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TreasureSketchGameState.generated.h"

UENUM(BlueprintType)
enum class ETreasureRoundPhase : uint8
{
    ScoutDrawing,
    HunterSearching,
    Won
};

UCLASS()
class TREASURESKETCH_API ATreasureSketchGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ATreasureSketchGameState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 IslandSeed = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    ETreasureRoundPhase Phase = ETreasureRoundPhase::ScoutDrawing;

    UPROPERTY(Replicated, BlueprintReadOnly)
    float RoundEndServerTime = 60.f;

    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bGameStarted = false;

    int32 GetSecondsRemaining() const;
};
