#pragma once

#include "CoreMinimal.h"

// Canyon navigation is generated independently from the Island terrain and asset pools.
enum class ECanyonGrayboxProblem : uint8
{
    ForkRejoin, LoopShortcut, HighLow, HubPocket, ChainAlcoves, BraidedGorge
};

enum class ECanyonGrayboxLandmark : uint8
{
    None, Arch, TwinPillars, SplitPeak, BrokenBridge, Needle, StoneRing, CaveBeacon
};

enum class ECanyonRouteLayer : uint8 { Lower, Ramp, Upper };
enum class ECanyonCavePattern : uint8 { ThroughShortcut, ThreeMouthHall, PillarChamber, FissureHall, TreasureAlcove };

struct FCanyonGrayboxNode
{
    FVector Position = FVector::ZeroVector; // XY and relative route floor height, in cm.
    ECanyonGrayboxLandmark Landmark = ECanyonGrayboxLandmark::None;
    ECanyonRouteLayer Layer = ECanyonRouteLayer::Lower;
};

struct FCanyonGrayboxEdge
{
    int32 A = INDEX_NONE;
    int32 B = INDEX_NONE;
    ECanyonRouteLayer Layer = ECanyonRouteLayer::Lower;
    float HalfWidth = 450.f;
    bool bCave = false;
};

struct FCanyonCaveChamber
{
    FVector Position = FVector::ZeroVector;
    float Radius = 0.f;
    float Clearance = 0.f;
    bool bPillar = false;
};

struct FCanyonGrayboxLayout
{
    int32 Seed = 0;
    float LengthScale = 1.f;
    ECanyonGrayboxProblem Problem = ECanyonGrayboxProblem::ForkRejoin;
    ECanyonCavePattern CavePattern = ECanyonCavePattern::ThroughShortcut;
    TArray<FCanyonGrayboxNode> Nodes;
    TArray<FCanyonGrayboxEdge> Edges;
    TArray<FCanyonCaveChamber> CaveChambers;
    TArray<int32> CaveMouthNodes;
    int32 SpawnNode = INDEX_NONE;
    int32 TreasureNode = INDEX_NONE;

    static FCanyonGrayboxLayout Generate(int32 Seed);
    void ScaleForMap(float Scale);
    const TCHAR* ProblemName() const;
    const TCHAR* CaveName() const;
    bool Validate() const;
    float HeightAt(float X, float Y, float* DistanceFromRoute = nullptr,
        ECanyonRouteLayer* SurfaceLayer = nullptr) const;
};
