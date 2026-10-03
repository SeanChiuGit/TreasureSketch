#pragma once

#include "CoreMinimal.h"
#include "CanyonSolidMesh.h"

// Canyon navigation is generated independently from the Island terrain and asset pools.
enum class ECanyonGrayboxProblem : uint8
{
    ForkRejoin, LoopShortcut, HighLow, HubPocket, ChainAlcoves, BraidedGorge
};

enum class ECanyonGrayboxLandmark : uint8
{
    None, Arch, TwinPillars, SplitPeak, BrokenBridge, Needle, StoneRing, CaveBeacon
};

enum class ECanyonRouteLayer : uint8 { Lower, Ramp, Upper, CaveAccess };
enum class ECanyonUpperPattern : uint8 { Lookout, Traverse, SplitTraverse };
enum class ECanyonCavePattern : uint8 { ThroughShortcut, LongWindingThrough, ThreeMouthHall, LongLoop, BranchedThrough, PillarChamber, FissureHall, TreasureAlcove };

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

struct FCanyonCaveHall
{
    int32 Node = INDEX_NONE;
    float Radius = 650.f;
    float Clearance = 560.f;
};

enum class ECanyonDeadEndKind : uint8 { ShortAlcove, LongWinding, Forked };
enum class ECanyonCaveElevation : uint8 { Descending, Ascending, Level };

struct FCanyonDeadEnd
{
    ECanyonDeadEndKind Kind = ECanyonDeadEndKind::ShortAlcove;
    TArray<TArray<int32>> Paths;
    TArray<int32> EndNodes;
    TArray<FCanyonCaveHall> Rooms;
};

struct FCanyonCaveNetwork
{
    ECanyonCavePattern Pattern = ECanyonCavePattern::ThroughShortcut;
    ECanyonCaveElevation Elevation = ECanyonCaveElevation::Descending;
    TArray<int32> MouthNodes;
    TArray<int32> PathNodes;
    TArray<TArray<int32>> Branches;
    TArray<bool> BranchOpen;
    TArray<FCanyonCaveHall> Halls;
    TArray<FCanyonDeadEnd> DeadEnds; // Optional additions to the base cave topology.
};

struct FCanyonCaveSegment
{
    FVector A, B;
    float Width = 0.f, Clearance = 0.f;
    int32 Group = 0;
    bool bExterior = false;
    bool bPortalA = false, bPortalB = false;
    bool bMountainCover = false;
};

struct FCanyonSurfaceGuide { TArray<float> Heights; };

struct FCanyonGrayboxLayout
{
    int32 Seed = 0;
    float LengthScale = 1.f;
    ECanyonGrayboxProblem Problem = ECanyonGrayboxProblem::ForkRejoin;
    ECanyonUpperPattern UpperPattern = ECanyonUpperPattern::Traverse;
    ECanyonCavePattern CavePattern = ECanyonCavePattern::ThroughShortcut;
    TArray<FCanyonGrayboxNode> Nodes;
    TArray<FCanyonGrayboxEdge> Edges;
    TArray<int32> CaveMouthNodes; // Actual exterior openings only.
    TArray<int32> CavePathNodes; // Ordered main route between the first two mouths.
    TArray<TArray<int32>> CaveBranches; // Continuous paths: hall legs, or loop stems and arms.
    TArray<bool> CaveBranchOpen; // True: exterior mouth. False: enclosed dead end.
    TArray<FCanyonCaveHall> CaveHalls;
    TArray<FCanyonDeadEnd> CaveDeadEnds;
    TArray<FCanyonCaveNetwork> CaveNetworks;
    TArray<FCanyonCaveSegment> CaveSegments;
    int32 CaveSegmentGroups = 0;
    TArray<FCanyonSurfaceGuide> SurfaceGuides;
    TArray<float> SurfaceNodeHeights;
    bool bBuildingSurfaceGuides = false;
    TArray<int32> AllCaveMouthNodes;
    TArray<FCanyonCaveHall> AllCaveHalls;
    int32 SpawnNode = INDEX_NONE;
    int32 TreasureNode = INDEX_NONE;

    static FCanyonGrayboxLayout Generate(int32 Seed, float MapScale = 1.f,
        bool bHallPreview = false, bool bLoopPreview = false, bool bBranchPreview = false);
    void ScaleForMap(float Scale);
    void RebuildCaveSegments();
    void RebuildSurfaceGuides();
    void SampleCaveTunnels(float X, float Y, float Margin, float SurfaceZ,
        TArray<FCanyonSolidTunnel, TInlineAllocator<4>>& Tunnels) const;
    float RequiredCaveCover(float X, float Y, float SurfaceZ) const;
    float CavePortalWeight(float X, float Y) const;
    const TCHAR* ProblemName() const;
    const TCHAR* CaveName(int32 NetworkIndex = INDEX_NONE) const;
    bool Validate() const;
    bool SampleCave(float T, FVector& Position, FVector& Tangent) const;
    bool SampleCaveBranch(int32 Branch, float T, FVector& Position, FVector& Tangent) const;
    bool ProjectCave(float X, float Y, float& T, float& Lateral, float& FloorZ,
        int32* NetworkIndex = nullptr) const;
    float CaveHalfWidth(float T, int32 NetworkIndex = INDEX_NONE) const;
    float CaveClearance(float T, int32 NetworkIndex = INDEX_NONE) const;
    bool IsCaveVoid(float X, float Y) const;
    float SurfaceHeightAt(float X, float Y, float* DistanceFromRoute = nullptr,
        ECanyonRouteLayer* SurfaceLayer = nullptr) const;
    float HeightAt(float X, float Y, float* DistanceFromRoute = nullptr,
        ECanyonRouteLayer* SurfaceLayer = nullptr) const;
};
