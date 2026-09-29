#pragma once

#include "CoreMinimal.h"

// Gameplay decisions are made before terrain or meshes are built. The graph is theme-agnostic;
// Canyon is the first adapter. Forest and Island can consume the same node/edge contract later.
enum class ETreasureMacroLayout : uint8
{
    Switchback, YFork, Ring, ShortLongRoutes, CentralHub, Ladder, Basin
};

enum class ETreasureNodeRole : uint8 { Route, Spawn, Junction, Landmark, Treasure };
enum class ETreasureRegion : uint8 { CanyonFloor, Mesa, Cliff, Cave, RockField };
enum class ETreasureRoute : uint8 { Main, Branch, High, Low, Crossing };
enum class ETreasureLandmark : uint8
{
    None, StoneArch, TwinSpires, BrokenBridge, GiantSkull, StoneRing,
    Watchtower, Cairn, BalancedBoulder
};
enum class ETreasureRelationship : uint8 { BehindLandmark, NearJunction, BelowCliff, RouteSequence };

struct FTreasureLayoutNode
{
    FIntPoint Cell;
    FVector2D Position = FVector2D::ZeroVector;
    float FloorHeight = 480.f;
    ETreasureNodeRole Role = ETreasureNodeRole::Route;
    ETreasureRegion Region = ETreasureRegion::CanyonFloor;
    ETreasureLandmark Landmark = ETreasureLandmark::None;
};

struct FTreasureLayoutEdge
{
    int32 A = INDEX_NONE;
    int32 B = INDEX_NONE;
    ETreasureRoute Route = ETreasureRoute::Branch;
    bool bPrimary = false;
};

struct FTreasureGameplayLayout
{
    int32 Seed = 0;
    ETreasureMacroLayout Topology = ETreasureMacroLayout::Switchback;
    ETreasureRelationship TreasureRelationship = ETreasureRelationship::BehindLandmark;
    TArray<FTreasureLayoutNode> Nodes;
    TArray<FTreasureLayoutEdge> Edges;
    int32 SpawnNode = INDEX_NONE;
    int32 TreasureNode = INDEX_NONE;
    FVector2D TreasurePosition = FVector2D::ZeroVector;
    float TreasureHeight = 480.f;

    bool ValidateGraph(FString* Reason = nullptr) const;
    uint32 GetSignature() const;
    int32 GetLandmarkCount() const;
    int32 GetJunctionCount() const;
    int32 GetRouteCount() const;
    static const TCHAR* TopologyName(ETreasureMacroLayout Value);
    static const TCHAR* RelationshipName(ETreasureRelationship Value);
};

class FTreasureGameplayLayoutGenerator
{
public:
    static FTreasureGameplayLayout GenerateCanyon(int32 Seed, float ModuleLength = 2400.f);
};
