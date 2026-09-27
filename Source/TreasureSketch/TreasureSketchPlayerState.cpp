#include "TreasureSketchPlayerState.h"

#include "Net/UnrealNetwork.h"

ATreasureSketchPlayerState::ATreasureSketchPlayerState()
{
    bReplicates = true;
}

void ATreasureSketchPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATreasureSketchPlayerState, PlayerRole);
    DOREPLIFETIME(ATreasureSketchPlayerState, bSketchSubmitted);
}
