#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ProceduralIsland.h"
#include "SketchTypes.h"
#include "TreasureSketchGameState.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureHistorySave.generated.h"

USTRUCT()
struct FPlayedRoundRecord
{
    GENERATED_BODY()

    UPROPERTY() FString RecordId;
    UPROPERTY() FString SeriesId;
    UPROPERTY() FString UtcTimeIso;
    UPROPERTY() FString LocalTimeText;
    UPROPERTY() int32 RoundSerial = 0;
    UPROPERTY() int32 IslandSeed = 0;
    UPROPERTY() EIslandTheme Theme = EIslandTheme::PirateBeach;
    UPROPERTY() float MapScale = 1.f;
    UPROPERTY() ETreasureRoomMode RoomMode = ETreasureRoomMode::OneMapmaker;
    UPROPERTY() ETreasureRoundPhase Outcome = ETreasureRoundPhase::HunterTimedOut;
    UPROPERTY() FString LocalPlayerName;
    UPROPERTY() ETreasurePlayerRole LocalRole = ETreasurePlayerRole::Unassigned;
    UPROPERTY() int32 HideTreasureCount = 0;
    UPROPERTY() int32 HideTreasureTotal = 3;
    UPROPERTY() bool bHideCaught = false;
    UPROPERTY() FString WinnerName;
    UPROPERTY() float SearchSeconds = 0.f;
    UPROPERTY() int32 RaceRoundIndex = 0;
    UPROPERTY() int32 RaceTotalRounds = 0;
    UPROPERTY() int32 RaceRoundPoints = 0;
    UPROPERTY() int32 RaceTotalPoints = 0;
    UPROPERTY() bool bPreprintedIsland = false;
    // 48 x 48 land mask, stored so old preprinted outlines survive map generator changes.
    UPROPERTY() TArray<uint8> IslandTemplateMask;
    UPROPERTY() TArray<FSketchPage> Pages;
};

UCLASS()
class TREASURESKETCH_API UTreasureHistorySave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() TArray<FPlayedRoundRecord> Records;
};
