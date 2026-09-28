#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TreasureSketchPlayerState.generated.h"

UENUM(BlueprintType)
enum class ETreasurePlayerRole : uint8
{
    Unassigned,
    Scout,
    Hunter
};

UCLASS()
class TREASURESKETCH_API ATreasureSketchPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    ATreasureSketchPlayerState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly)
    ETreasurePlayerRole PlayerRole = ETreasurePlayerRole::Unassigned;

    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bSketchSubmitted = false;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 DigCooldownRoundSerial = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    float NextDigServerTime = 0.f;

    float GetDigCooldownRemaining(int32 CurrentRoundSerial, float ServerTime) const
    {
        return DigCooldownRoundSerial == CurrentRoundSerial ? FMath::Max(0.f, NextDigServerTime - ServerTime) : 0.f;
    }
};
