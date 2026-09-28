#pragma once

#include "CoreMinimal.h"
#include "SketchTypes.generated.h"

USTRUCT()
struct FSketchStroke
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FVector2D> Points;

    // 0 ink, 1 red, 2 blue, 3 green, 4 gold, 5 eraser.
    UPROPERTY()
    uint8 ColorIndex = 0;

    // Only used by the eraser: 0 small, 1 large.
    UPROPERTY()
    uint8 EraserSize = 0;
};

USTRUCT()
struct FSketchPage
{
    GENERATED_BODY()

    UPROPERTY()
    int32 MapmakerId = 0;

    UPROPERTY()
    FString MapmakerName;

    UPROPERTY()
    TArray<FSketchStroke> Strokes;
};
