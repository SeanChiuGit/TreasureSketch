#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SketchTypes.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchGameMode.generated.h"

class AProceduralIsland;
class ATreasureSurfacePaint;
class ATreasureSketchPlayerState;
class ATreasureSketchPlayerController;
enum class EIslandTheme : uint8;

UCLASS()
class TREASURESKETCH_API ATreasureSketchGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    static constexpr float HeldDigSeconds = 1.2f;
    static constexpr float HideDigSeconds = 3.f;
    ATreasureSketchGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void Logout(AController* Exiting) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;

    FVector GetTreasureLocation() const { return TreasureLocation; }

    void HandoffToHunter(const TArray<struct FSketchStroke>& SubmittedStrokes);
    void SubmitPlayerSketch(ATreasureSketchPlayerState* Scout, const TArray<FSketchStroke>& SubmittedStrokes);
    void BroadcastSketchDelta(ATreasureSketchPlayerState* Scout, int32 StrokeIndex, uint8 ColorIndex, uint8 EraserSize, const TArray<FVector2D>& Points);
    void BroadcastSketchClear(ATreasureSketchPlayerState* Scout);
    void BroadcastSketchPhoto(ATreasureSketchPlayerState* Scout, const TArray<uint8>& PhotoJpeg);
    bool TryDig(ATreasureSketchPlayerState* Hunter, const FVector& WorldLocation, float& OutDistance, bool& bAttempted);
    bool StartHeldDig(ATreasureSketchPlayerController* HunterController);
    void CancelHeldDig(ATreasureSketchPlayerState* Hunter, bool bNotifyInterrupted = false);
    bool TryShove(ATreasureSketchPlayerController* ShovingPlayer);
    void StartNewRound(bool bSwapRoles = false, ATreasureSketchPlayerState* RoleRequester = nullptr);
    void SetRoundReview(bool bReviewing);
    void StartHostedRound();
    bool SelectRoomMode(ETreasureRoomMode Mode);
    bool ClaimSingleRoomRole(ATreasureSketchPlayerState* Player);
    bool ToggleRoomMapPool(EIslandTheme Theme);
    void NormalizeRoomRoles(APlayerState* Excluded = nullptr, ATreasureSketchPlayerState* PreferredSinglePlayer = nullptr);
    void AdjustRoomSetting(FName Setting, int32 Direction);
    bool SetRoomMapScale(float Scale);
    bool SetRoleMovementSpeed(FName Setting, float Multiplier);
    void ReturnToSetup();
    void StartSoloTest(int32 ThemeChoice, bool bForceNewSeed = false);
    bool RefreshSoloMap();
    void ToggleSurfacePaint();
    void SpraySurface(const FHitResult& Hit);

private:
    friend class FMultiMapmakerFlowTest;
    friend class FExplorerRaceFlowTest;
    friend class FHideAndSeekFlowTest;
    void BeginHideAndSeek();
    void FinishHideAndSeek(bool bCaught);
    void ResolveShove(ATreasureSketchPlayerController* ShovingPlayer,
        class ATreasureSketchCharacter* ShovingCharacter, int32 RoundSerial);
    UPROPERTY()
    TObjectPtr<AProceduralIsland> Island;

    FVector TreasureLocation = FVector::ZeroVector;
    int32 IslandSeed = 0;
    int32 SoloThemeChoice = INDEX_NONE;
    float HunterViewUpdateTime = 0.f;
    UPROPERTY() TObjectPtr<ATreasureSurfacePaint> SurfacePaint;
    void ResetSurfacePaint();
    void PlaceRoundPlayers();
    void RecoverFallenPlayers();
    void UpdateHeldDigs();
    TArray<FVector> MapmakerLandingSpawns;

    FVector FindPlayerSpawn(TArray<FVector>& UsedSpawns) const;
    FVector FindHunterSpawn(TArray<FVector>& UsedSpawns) const;
    void ScoreRaceRound(ATreasureSketchPlayerState* Finder);
    void RecordCompletedRound();
    FString CurrentRaceSeriesId;
    void BuildRound();
    bool FinishIfTimeExpired();
    void BeginHunterSearching(const TArray<FSketchPage>& Pages);
    TArray<FSketchPage> CollectSketchPages() const;
    void ResetSubmittedSketches();
    UPROPERTY()
    TMap<int32, FSketchPage> SubmittedSketches;
    void RevealTreasureToScout();
    void HideTreasureFromScout();
    void SendHunterViewToScout(float DeltaSeconds);
};
