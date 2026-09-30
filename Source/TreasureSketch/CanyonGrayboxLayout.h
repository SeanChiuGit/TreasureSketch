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
enum class ECanyonUpperPattern : uint8 { Lookout, Traverse, SplitTraverse };
enum class ECanyonCavePattern : uint8 { ThroughShortcut, ThreeMouthHall, PillarChamber, FissureHall, TreasureAlcove };

struct FCanyonGrayboxNode
{
    FVector Position = FVector::ZeroVector; // XY and relative route floor height, in cm.
    ECanyonGrayboxLandmark Landmark = ECanyonGrayboxLandmark::None;
    ECanyonRouteLayer Layer = ECanyonRouteLayer::Lower;
    bool bCaveInterior = false;
};

struct FCanyonGrayboxEdge
{
    int32 A = INDEX_NONE;
    int32 B = INDEX_NONE;
    ECanyonRouteLayer Layer = ECanyonRouteLayer::Lower;
    float HalfWidth = 450.f;
    bool bCave = false;
};

struct FCanyonGrayboxLayout
{
    int32 Seed = 0;
    float LengthScale = 1.f;
    ECanyonGrayboxProblem Problem = ECanyonGrayboxProblem::ForkRejoin;
    ECanyonUpperPattern UpperPattern = ECanyonUpperPattern::Traverse;
    ECanyonCavePattern CavePattern = ECanyonCavePattern::ThroughShortcut;
    TArray<FCanyonGrayboxNode> Nodes;
    TArray<FCanyonGrayboxEdge> Edges;
    TArray<int32> CaveMouthNodes;
    TArray<int32> CavePathNodes; // Ordered entrance, curved interior, exit.
    int32 SpawnNode = INDEX_NONE;
    int32 TreasureNode = INDEX_NONE;

    static FCanyonGrayboxLayout Generate(int32 Seed, float MapScale = 1.f);
    void ScaleForMap(float Scale);
    const TCHAR* ProblemName() const;
    const TCHAR* CaveName() const;
    bool Validate() const;
    bool SampleCave(float T, FVector& Position, FVector& Tangent) const;
    bool ProjectCave(float X, float Y, float& T, float& Lateral, float& FloorZ) const;
    float CaveHalfWidth(float T) const;
    float CaveClearance(float T) const;
    bool IsCaveVoid(float X, float Y) const;
    float SurfaceHeightAt(float X, float Y, float* DistanceFromRoute = nullptr,
        ECanyonRouteLayer* SurfaceLayer = nullptr) const;
    float HeightAt(float X, float Y, float* DistanceFromRoute = nullptr,
        ECanyonRouteLayer* SurfaceLayer = nullptr) const;
};
