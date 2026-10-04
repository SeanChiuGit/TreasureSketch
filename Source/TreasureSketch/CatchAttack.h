#pragma once
#include "CoreMinimal.h"

namespace CatchAttack
{
    constexpr float HalfAngleDegrees = 80.f;
    constexpr float VerticalHalfHeight = 300.f;
    constexpr float WindupSeconds = 0.15f;
    inline bool Contains(const FVector& Delta, const FVector& Facing, float Radius)
    {
        return FMath::Abs(Delta.Z) <= VerticalHalfHeight
            && Delta.SizeSquared2D() <= FMath::Square(Radius)
            && (Delta.IsNearlyZero(0.01f) || Delta.SizeSquared2D() < 0.0001f
                || FVector::DotProduct(Facing.GetSafeNormal2D(), Delta.GetSafeNormal2D())
                    >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees)));
    }
}
