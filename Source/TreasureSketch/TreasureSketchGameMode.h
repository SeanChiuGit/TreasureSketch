#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SketchTypes.h"
#include "TreasureSketchGameMode.generated.h"

class AProceduralIsland;

UCLASS()
class TREASURESKETCH_API ATreasureSketchGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ATreasureSketchGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;

    FVector GetTreasureLocation() const { return TreasureLocation; }

    void HandoffToHunter(const TArray<struct FSketchStroke>& SubmittedStrokes);
    bool TryDig(const FVector& WorldLocation, float& OutDistance);
    void StartNewRound(bool bSwapRoles = false);
    void StartHostedRound();

private:
    UPROPERTY()
    TObjectPtr<AProceduralIsland> Island;

    FVector TreasureLocation = FVector::ZeroVector;
    int32 IslandSeed = 0;
    float HunterViewUpdateTime = 0.f;

    void BuildRound();
    bool FinishIfTimeExpired();
    void RevealTreasureToScout();
    void HideTreasureFromScout();
    void SendHunterViewToScout(float DeltaSeconds);
};
