#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "../TreasureHistorySave.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHistorySaveTest, "TreasureSketch.RoomSettings.HistorySave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHistorySaveTest::RunTest(const FString& Parameters)
{
    UTreasureHistorySave* Original = NewObject<UTreasureHistorySave>();
    FPlayedRoundRecord First;
    First.RecordId = TEXT("one");
    First.SeriesId = TEXT("series-A");
    First.UtcTimeIso = TEXT("2026-09-28T20:00:00Z");
    First.LocalTimeText = TEXT("2026-09-28 21:00");
    First.IslandSeed = 12345;
    First.Theme = EIslandTheme::MistForest;
    First.LocalRole = ETreasurePlayerRole::Scout;
    First.RoomMode = ETreasureRoomMode::ExplorerRace;
    First.Outcome = ETreasureRoundPhase::Won;
    First.WinnerName = TEXT("Player Two");
    First.RaceRoundIndex = 1;
    First.RaceTotalRounds = 3;
    First.bPreprintedIsland = true;
    First.IslandTemplateMask = { 0, 1, 1, 0 };
    FSketchPage Page;
    Page.MapmakerName = TEXT("Player One");
    FSketchStroke Eraser;
    Eraser.ColorIndex = 5;
    Eraser.EraserSize = 1;
    Eraser.Points = { FVector2D(0.1f, 0.2f), FVector2D(0.3f, 0.4f) };
    Page.Strokes.Add(Eraser);
    First.Pages.Add(Page);
    Original->Records.Add(First);
    FPlayedRoundRecord Second = First;
    Second.RecordId = TEXT("two");
    Second.RaceRoundIndex = 2;
    Second.LocalRole = ETreasurePlayerRole::Hunter;
    Original->Records.Add(Second);

    TArray<uint8> Bytes;
    TestTrue(TEXT("History serializes without writing a player save"),
        UGameplayStatics::SaveGameToMemory(Original, Bytes));
    const UTreasureHistorySave* Reloaded = Cast<UTreasureHistorySave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("History deserializes"), Reloaded)) return false;
    TestEqual(TEXT("Both rounds remain in the same series"), Reloaded->Records.Num(), 2);
    if (Reloaded->Records.Num() != 2) return false;
    TestEqual(TEXT("Seed survives"), Reloaded->Records[0].IslandSeed, 12345);
    TestEqual(TEXT("Local timestamp survives"), Reloaded->Records[0].LocalTimeText,
        FString(TEXT("2026-09-28 21:00")));
    TestEqual(TEXT("Winner survives"), Reloaded->Records[0].WinnerName, FString(TEXT("Player Two")));
    TestEqual(TEXT("Round order survives"), Reloaded->Records[1].RaceRoundIndex, 2);
    TestEqual(TEXT("Preprinted outline survives"), Reloaded->Records[0].IslandTemplateMask.Num(), 4);
    if (!TestEqual(TEXT("Final drawing survives"), Reloaded->Records[0].Pages.Num(), 1)) return false;
    TestEqual(TEXT("Sketch author survives"), Reloaded->Records[0].Pages[0].MapmakerName, FString(TEXT("Player One")));
    if (!TestEqual(TEXT("Eraser stroke survives"), Reloaded->Records[0].Pages[0].Strokes.Num(), 1)) return false;
    TestEqual(TEXT("Large eraser survives"), Reloaded->Records[0].Pages[0].Strokes[0].EraserSize, static_cast<uint8>(1));
    return true;
}

#endif
