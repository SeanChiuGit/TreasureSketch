#include "TreasureSketchGameState.h"

#include "Net/UnrealNetwork.h"
#include "TreasureSketchCharacter.h"
#include "EngineUtils.h"

ATreasureSketchGameState::ATreasureSketchGameState()
{
    bReplicates = true;
}

void ATreasureSketchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATreasureSketchGameState, RoomMode);
    DOREPLIFETIME(ATreasureSketchGameState, bBeachInMapPool);
    DOREPLIFETIME(ATreasureSketchGameState, bForestInMapPool);
    DOREPLIFETIME(ATreasureSketchGameState, bSurfacePaintEnabled);
    DOREPLIFETIME(ATreasureSketchGameState, SurfacePaintStampsUsed);
    DOREPLIFETIME(ATreasureSketchGameState, bTreasureRangeVisible);
    DOREPLIFETIME(ATreasureSketchGameState, bSpreadPlayerSpawns);
    DOREPLIFETIME(ATreasureSketchGameState, RoomMapScale);
    DOREPLIFETIME(ATreasureSketchGameState, MovementSpeedMultiplier);
    DOREPLIFETIME(ATreasureSketchGameState, DrawingDurationSeconds);
    DOREPLIFETIME(ATreasureSketchGameState, SearchingDurationSeconds);
    DOREPLIFETIME(ATreasureSketchGameState, IslandSeed);
    DOREPLIFETIME(ATreasureSketchGameState, RoundSerial);
    DOREPLIFETIME(ATreasureSketchGameState, Phase);
    DOREPLIFETIME(ATreasureSketchGameState, RoundEndServerTime);
    DOREPLIFETIME(ATreasureSketchGameState, bGameStarted);
    DOREPLIFETIME(ATreasureSketchGameState, bReviewingRound);
}

void ATreasureSketchGameState::ApplyMovementSpeed()
{
    for (TActorIterator<ATreasureSketchCharacter> It(GetWorld()); It; ++It)
        It->SetMovementSpeedMultiplier(MovementSpeedMultiplier);
}

int32 ATreasureSketchGameState::GetSecondsRemaining() const
{
    return FMath::Max(0, FMath::CeilToInt(RoundEndServerTime - GetServerWorldTimeSeconds()));
}
