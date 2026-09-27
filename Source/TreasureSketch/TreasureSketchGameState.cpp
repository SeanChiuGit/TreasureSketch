#include "TreasureSketchGameState.h"

#include "Net/UnrealNetwork.h"

ATreasureSketchGameState::ATreasureSketchGameState()
{
    bReplicates = true;
}

void ATreasureSketchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATreasureSketchGameState, RoomMode);
    DOREPLIFETIME(ATreasureSketchGameState, bSurfacePaintEnabled);
    DOREPLIFETIME(ATreasureSketchGameState, RoomGridSize);
    DOREPLIFETIME(ATreasureSketchGameState, RoomMapScale);
    DOREPLIFETIME(ATreasureSketchGameState, DrawingDurationSeconds);
    DOREPLIFETIME(ATreasureSketchGameState, SearchingDurationSeconds);
    DOREPLIFETIME(ATreasureSketchGameState, IslandSeed);
    DOREPLIFETIME(ATreasureSketchGameState, RoundSerial);
    DOREPLIFETIME(ATreasureSketchGameState, Phase);
    DOREPLIFETIME(ATreasureSketchGameState, RoundEndServerTime);
    DOREPLIFETIME(ATreasureSketchGameState, bGameStarted);
}

int32 ATreasureSketchGameState::GetSecondsRemaining() const
{
    return FMath::Max(0, FMath::CeilToInt(RoundEndServerTime - GetServerWorldTimeSeconds()));
}
