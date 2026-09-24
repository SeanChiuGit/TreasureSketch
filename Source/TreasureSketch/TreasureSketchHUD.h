#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TreasureSketchHUD.generated.h"

UCLASS()
class TREASURESKETCH_API ATreasureSketchHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
};
