#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TreasureSketchHUD.generated.h"

class UFont;

UCLASS()
class TREASURESKETCH_API ATreasureSketchHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;

private:
    void EnsureGameFonts();

    UPROPERTY(Transient)
    TObjectPtr<UFont> DisplayFont;

    UPROPERTY(Transient)
    TObjectPtr<UFont> BodyFont;
};
