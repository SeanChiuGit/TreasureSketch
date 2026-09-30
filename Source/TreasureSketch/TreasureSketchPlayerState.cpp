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
    DOREPLIFETIME(ATreasureSketchPlayerState, DigCooldownRoundSerial);
    DOREPLIFETIME(ATreasureSketchPlayerState, NextDigServerTime);
    DOREPLIFETIME(ATreasureSketchPlayerState, bDigging);
    DOREPLIFETIME(ATreasureSketchPlayerState, DigStartedServerTime);
    DOREPLIFETIME(ATreasureSketchPlayerState, RacePoints);
    DOREPLIFETIME(ATreasureSketchPlayerState, RaceFinds);
    DOREPLIFETIME(ATreasureSketchPlayerState, RaceLastRoundPoints);
    DOREPLIFETIME(ATreasureSketchPlayerState, RoundShoveHits);
    DOREPLIFETIME(ATreasureSketchPlayerState, NextShoveServerTime);
    DOREPLIFETIME(ATreasureSketchPlayerState, ShoveProtectedUntilServerTime);
}
