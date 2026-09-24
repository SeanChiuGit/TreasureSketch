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
