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
};
