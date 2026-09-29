#pragma once

#include "CoreMinimal.h"

// Canyon navigation is generated independently from the Island terrain and asset pools.
enum class ECanyonGrayboxProblem : uint8
{
    ForkRejoin, LoopShortcut, HighLow, HubPocket, ChainAlcoves, BraidedGorge
};

enum class ECanyonGrayboxLandmark : uint8
{
    None, Arch, TwinPillars, SplitPeak, BrokenBridge, Needle, StoneRing
};

struct FCanyonGrayboxNode
{
    FVector Position = FVector::ZeroVector; // XY and relative route floor height, in cm.
    ECanyonGrayboxLandmark Landmark = ECanyonGrayboxLandmark::None;
};

struct FCanyonGrayboxEdge
{
    int32 A = INDEX_NONE;
    int32 B = INDEX_NONE;
};

struct FCanyonGrayboxLayout
{
    int32 Seed = 0;
    ECanyonGrayboxProblem Problem = ECanyonGrayboxProblem::ForkRejoin;
    TArray<FCanyonGrayboxNode> Nodes;
    TArray<FCanyonGrayboxEdge> Edges;
    int32 SpawnNode = INDEX_NONE;
    int32 TreasureNode = INDEX_NONE;

    static FCanyonGrayboxLayout Generate(int32 Seed);
    const TCHAR* ProblemName() const;
    bool Validate() const;
    float HeightAt(float X, float Y, float* DistanceFromRoute = nullptr) const;
};
