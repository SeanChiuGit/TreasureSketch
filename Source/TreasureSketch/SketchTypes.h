#pragma once

#include "CoreMinimal.h"
#include "SketchTypes.generated.h"

USTRUCT()
struct FSketchStroke
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FVector2D> Points;
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
