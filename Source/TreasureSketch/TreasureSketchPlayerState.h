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

    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bDigging = false;

    UPROPERTY(Replicated, BlueprintReadOnly)
    float DigStartedServerTime = 0.f;

    // Server-only anchor: moving away cancels the held dig.
    FVector DigStartLocation = FVector::ZeroVector;
    int32 DigStartRoundSerial = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 RacePoints = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 RaceFinds = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 RaceLastRoundPoints = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 RoundShoveHits = 0;

    // Server-only progress for the current race round.
    float RaceBestMissDistance = TNumericLimits<float>::Max();
    int32 RaceFarMisses = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    float NextShoveServerTime = 0.f;

    UPROPERTY(Replicated, BlueprintReadOnly)
    float ShoveProtectedUntilServerTime = 0.f;

    float GetDigCooldownRemaining(int32 CurrentRoundSerial, float ServerTime) const
    {
        return DigCooldownRoundSerial == CurrentRoundSerial ? FMath::Max(0.f, NextDigServerTime - ServerTime) : 0.f;
    }
};
