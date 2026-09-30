#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;

struct FCanyonSolidColumn
{
    float SurfaceZ = 0.f;
    float FloorZ = 0.f;
    float Lateral = 0.f;
    float HalfWidth = 0.f;
    float Clearance = 0.f;
};

struct FCanyonSolidBounds
{
    FVector2D Min = FVector2D::ZeroVector;
    FVector2D Max = FVector2D::ZeroVector;
    float BottomZ = -350.f;
    float StepXY = 80.f;
    float StepZ = 80.f;
};

// Extract one connected solid surface: terrain, tunnel floor, cave walls and
// roof. The XY boundary is exactly on the caller's terrain grid vertices.
void BuildCanyonSolidMesh(UProceduralMeshComponent* Mesh, const FCanyonSolidBounds& Bounds,
    TFunctionRef<FCanyonSolidColumn(float, float)> SampleColumn);
