#pragma once

#include "CoreMinimal.h"

namespace SketchRotation
{
inline int32 NormalizeSteps(int32 Steps)
{
    return (Steps % 4 + 4) % 4;
}

inline float FitScale(const FVector2D& ContentSize, int32 Steps)
{
    return (NormalizeSteps(Steps) & 1) && ContentSize.X > 0.f && ContentSize.Y > 0.f
        ? FMath::Min(ContentSize.X / ContentSize.Y, ContentSize.Y / ContentSize.X) : 1.f;
}

inline FVector2D RotateDelta(const FVector2D& Delta, int32 Steps)
{
    switch (NormalizeSteps(Steps))
    {
    case 1: return FVector2D(-Delta.Y, Delta.X);
    case 2: return -Delta;
    case 3: return FVector2D(Delta.Y, -Delta.X);
    default: return Delta;
    }
}

// Rotate around the drawable area, leaving the toolbar in place. Odd turns fit
// the original landscape content inside the same paper without stretching it.
inline FVector2D ToScreen(const FVector2D& UnrotatedPoint,
    const FVector2D& ContentMin, const FVector2D& ContentSize, int32 Steps)
{
    const FVector2D Center = ContentMin + ContentSize * 0.5f;
    return Center + RotateDelta(UnrotatedPoint - Center, Steps) * FitScale(ContentSize, Steps);
}

inline FVector2D FromScreen(const FVector2D& ScreenPoint,
    const FVector2D& ContentMin, const FVector2D& ContentSize, int32 Steps)
{
    const FVector2D Center = ContentMin + ContentSize * 0.5f;
    return Center + RotateDelta((ScreenPoint - Center) / FitScale(ContentSize, Steps), -Steps);
}

inline FIntPoint SourceCellForDisplay(int32 X, int32 Y, int32 Count, int32 Steps)
{
    switch (NormalizeSteps(Steps))
    {
    case 1: return FIntPoint(Y, Count - 1 - X);
    case 2: return FIntPoint(Count - 1 - X, Count - 1 - Y);
    case 3: return FIntPoint(Count - 1 - Y, X);
    default: return FIntPoint(X, Y);
    }
}
}
