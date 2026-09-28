#pragma once

#include "CoreMinimal.h"

namespace TreasureRules
{
constexpr float DigHorizontalRadius = 425.f;
constexpr float DigVerticalHalfHeight = 180.f;
constexpr float MinimumHunterSpawnDistance = 3000.f;

// Bands are relative to island side length (the room setting scales area).
inline int32 DigFeedbackBand(float Distance, float MapScale)
{
    const float Scale = FMath::Sqrt(FMath::Clamp(MapScale, 0.5f, 5.f));
    if (Distance <= 600.f * Scale) return 0;
    if (Distance <= 1000.f * Scale) return 1;
    if (Distance <= 2000.f * Scale) return 2;
    if (Distance <= 5000.f * Scale) return 3;
    return 4;
}

inline int32 RaceProximityPoints(float BestMissDistance, float MapScale)
{
    return FMath::IsFinite(BestMissDistance) ? 4 - DigFeedbackBand(BestMissDistance, MapScale) : 0;
}
}
