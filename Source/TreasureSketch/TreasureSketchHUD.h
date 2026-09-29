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
    class UTexture2D* GetPhotoTexture(const TArray<uint8>& PhotoJpeg);
    void EnsureGameFonts();

    UPROPERTY(Transient)
    TObjectPtr<UFont> DisplayFont;

    UPROPERTY(Transient)
    TObjectPtr<UFont> BodyFont;

    TWeakObjectPtr<class AProceduralIsland> CachedTemplateIsland;
    int32 CachedTemplateSeed = 0;
    TArray<uint8> IslandTemplateMask;
    TArray<uint8> CachedPhotoJpeg;
    UPROPERTY()
    TObjectPtr<class UTexture2D> CachedPhotoTexture;
};
