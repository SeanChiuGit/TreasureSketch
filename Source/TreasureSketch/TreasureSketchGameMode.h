#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SketchTypes.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchGameMode.generated.h"

class AProceduralIsland;
class ATreasureSurfacePaint;

UCLASS()
class TREASURESKETCH_API ATreasureSketchGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ATreasureSketchGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void Logout(AController* Exiting) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;

    FVector GetTreasureLocation() const { return TreasureLocation; }

    void HandoffToHunter(const TArray<struct FSketchStroke>& SubmittedStrokes);
    bool TryDig(const FVector& WorldLocation, float& OutDistance);
    void StartNewRound(bool bSwapRoles = false);
    void StartHostedRound();
    bool SelectRoomMode(ETreasureRoomMode Mode);
    void AdjustRoomSetting(FName Setting, int32 Direction);
    void ReturnToSetup();
    void StartSoloTest(int32 ThemeChoice);
    void ToggleSurfacePaint();
    void SpraySurface(const FHitResult& Hit);

private:
    UPROPERTY()
    TObjectPtr<AProceduralIsland> Island;

    FVector TreasureLocation = FVector::ZeroVector;
    int32 IslandSeed = 0;
    float HunterViewUpdateTime = 0.f;
    UPROPERTY() TObjectPtr<ATreasureSurfacePaint> SurfacePaint;
    void ResetSurfacePaint();

    FVector FindPlayerSpawn(TArray<FVector>& UsedSpawns) const;
    void BuildRound();
    bool FinishIfTimeExpired();
    void RevealTreasureToScout();
    void HideTreasureFromScout();
    void SendHunterViewToScout(float DeltaSeconds);
};
