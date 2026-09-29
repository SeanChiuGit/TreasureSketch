#include "TreasureGameplayLayout.h"

namespace
{
constexpr int32 Side = 5;
int32 CellId(int32 X, int32 Y) { return Y * Side + X; }

float DistanceToRoutes(const FTreasureGameplayLayout& Layout, FVector2D Point)
{
    float Best = TNumericLimits<float>::Max();
    for (const FTreasureLayoutEdge& Edge : Layout.Edges)
    {
        const FVector2D A = Layout.Nodes[Edge.A].Position;
        const FVector2D Delta = Layout.Nodes[Edge.B].Position - A;
        const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, Delta) / Delta.SizeSquared(), 0.f, 1.f);
        Best = FMath::Min(Best, FVector2D::Distance(Point, A + T * Delta));
    }
    return Best;
}

bool IsOccludedByCanyon(const FTreasureGameplayLayout& Layout, FVector2D Target)
{
    const FVector2D Start = Layout.Nodes[Layout.SpawnNode].Position;
    for (int32 Step = 2; Step <= 30; ++Step)
        if (DistanceToRoutes(Layout, FMath::Lerp(Start, Target, Step / 32.f)) > 1100.f) return true;
    return false;
}
}

const TCHAR* FTreasureGameplayLayout::TopologyName(ETreasureMacroLayout Value)
{
    switch (Value)
    {
    case ETreasureMacroLayout::Switchback: return TEXT("Switchback");
    case ETreasureMacroLayout::YFork: return TEXT("YFork");
    case ETreasureMacroLayout::Ring: return TEXT("Ring");
    case ETreasureMacroLayout::ShortLongRoutes: return TEXT("ShortLongRoutes");
    case ETreasureMacroLayout::CentralHub: return TEXT("CentralHub");
    case ETreasureMacroLayout::Ladder: return TEXT("Ladder");
    default: return TEXT("Basin");
    }
}

const TCHAR* FTreasureGameplayLayout::RelationshipName(ETreasureRelationship Value)
{
    switch (Value)
    {
    case ETreasureRelationship::BehindLandmark: return TEXT("BehindLandmark");
    case ETreasureRelationship::NearJunction: return TEXT("NearJunction");
    case ETreasureRelationship::BelowCliff: return TEXT("BelowCliff");
    default: return TEXT("RouteSequence");
    }
}

int32 FTreasureGameplayLayout::GetLandmarkCount() const
{
    int32 Count = 0;
    for (const FTreasureLayoutNode& Node : Nodes) Count += Node.Landmark != ETreasureLandmark::None;
    return Count;
}

int32 FTreasureGameplayLayout::GetJunctionCount() const
{
    TArray<int32> Degree;
    Degree.Init(0, Nodes.Num());
    for (const FTreasureLayoutEdge& Edge : Edges)
        if (Degree.IsValidIndex(Edge.A) && Degree.IsValidIndex(Edge.B)) { ++Degree[Edge.A]; ++Degree[Edge.B]; }
    int32 Count = 0;
    for (int32 Value : Degree) Count += Value >= 3;
    return Count;
}

int32 FTreasureGameplayLayout::GetRouteCount() const
{
    // A connected tree offers one route. Every additional edge introduces another cycle.
    return 1 + FMath::Max(0, Edges.Num() - Nodes.Num() + 1);
}

bool FTreasureGameplayLayout::ValidateGraph(FString* Reason) const
{
    auto Fail = [&](const TCHAR* Message) { if (Reason) *Reason = Message; return false; };
    if (!Nodes.IsValidIndex(SpawnNode) || !Nodes.IsValidIndex(TreasureNode)) return Fail(TEXT("Missing spawn or treasure node"));
    if (Nodes.Num() < 7 || Edges.Num() < Nodes.Num() - 1) return Fail(TEXT("Layout is too small"));
    TArray<bool> Seen;
    Seen.Init(false, Nodes.Num());
    TArray<int32> Queue = { SpawnNode };
    Seen[SpawnNode] = true;
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
        for (const FTreasureLayoutEdge& Edge : Edges)
        {
            if (!Nodes.IsValidIndex(Edge.A) || !Nodes.IsValidIndex(Edge.B)) return Fail(TEXT("Invalid edge node"));
            const float Length = FVector2D::Distance(Nodes[Edge.A].Position, Nodes[Edge.B].Position);
            if (Length < 2390.f || Length > 2410.f) return Fail(TEXT("Edge does not fit a 24m module"));
            if (FMath::Abs(Nodes[Edge.A].FloorHeight - Nodes[Edge.B].FloorHeight) / Length > 0.126f)
                return Fail(TEXT("Unwalkable ramp"));
            const int32 Next = Edge.A == Queue[Head] ? Edge.B : Edge.B == Queue[Head] ? Edge.A : INDEX_NONE;
            if (Next != INDEX_NONE && !Seen[Next]) { Seen[Next] = true; Queue.Add(Next); }
        }
    if (Queue.Num() != Nodes.Num() || !Seen[TreasureNode]) return Fail(TEXT("Disconnected treasure or landmark"));
    if (FVector2D::Distance(Nodes[SpawnNode].Position, TreasurePosition) < 3600.f)
        return Fail(TEXT("Treasure too near spawn"));
    if (!IsOccludedByCanyon(*this, TreasurePosition)) return Fail(TEXT("Treasure directly visible from spawn"));
    if (GetLandmarkCount() < 3) return Fail(TEXT("Too few major landmarks"));
    TSet<ETreasureLandmark> Types;
    bool bEarlyLandmark = false;
    for (int32 Index = 0; Index < Nodes.Num(); ++Index)
    {
        if (Nodes[Index].Landmark == ETreasureLandmark::None) continue;
        if (Types.Contains(Nodes[Index].Landmark)) return Fail(TEXT("Repeated major landmark silhouette"));
        Types.Add(Nodes[Index].Landmark);
        bEarlyLandmark |= FVector2D::Distance(Nodes[Index].Position, Nodes[SpawnNode].Position) <= 2450.f;
        for (int32 Other = Index + 1; Other < Nodes.Num(); ++Other)
            if (Nodes[Other].Landmark != ETreasureLandmark::None
                && FVector2D::Distance(Nodes[Index].Position, Nodes[Other].Position) < 2350.f)
                return Fail(TEXT("Landmarks overlap"));
    }
    if (!bEarlyLandmark) return Fail(TEXT("No early landmark"));
    return true;
}

uint32 FTreasureGameplayLayout::GetSignature() const
{
    uint32 Hash = HashCombine(GetTypeHash(static_cast<uint8>(Topology)), GetTypeHash(static_cast<uint8>(TreasureRelationship)));
    Hash = HashCombine(Hash, GetTypeHash(TreasureNode));
    for (const FTreasureLayoutNode& Node : Nodes)
    {
        Hash = HashCombine(Hash, GetTypeHash(Node.Cell));
        Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Node.FloorHeight)));
        Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Node.Landmark)));
    }
    for (const FTreasureLayoutEdge& Edge : Edges)
        Hash = HashCombine(Hash, GetTypeHash(Edge.A * 31 + Edge.B));
    return Hash;
}

FTreasureGameplayLayout FTreasureGameplayLayoutGenerator::GenerateCanyon(int32 Seed, float ModuleLength)
{
    FTreasureGameplayLayout Layout;
    Layout.Seed = Seed;
    Layout.Topology = static_cast<ETreasureMacroLayout>(static_cast<uint32>(Seed) % 7u);
    Layout.TreasureRelationship = static_cast<ETreasureRelationship>((static_cast<uint32>(Seed) / 7u) % 4u);
    const bool bMirror = (static_cast<uint32>(Seed) & 0x10u) != 0;
    TMap<int32, int32> NodeByCell;
    auto GetNode = [&](int32 X, int32 Y)
    {
        if (bMirror) Y = 4 - Y;
        const int32 Key = CellId(X, Y);
        if (const int32* Existing = NodeByCell.Find(Key)) return *Existing;
        FTreasureLayoutNode Node;
        Node.Cell = FIntPoint(X, Y);
        Node.Position = FVector2D((X - 2) * ModuleLength, (Y - 2) * ModuleLength);
        const int32 EastRise = FMath::Clamp((X + (Seed & 1)) / 2, 0, 2);
        const int32 NorthRise = (bMirror ? Y - 2 : 2 - Y);
        Node.FloorHeight = 780.f + 300.f * (EastRise + NorthRise);
        Node.Region = Y <= 1 ? ETreasureRegion::Mesa : Y >= 3 ? ETreasureRegion::RockField : ETreasureRegion::CanyonFloor;
        const int32 Index = Layout.Nodes.Add(Node);
        NodeByCell.Add(Key, Index);
        return Index;
    };
    auto Add = [&](int32 AX, int32 AY, int32 BX, int32 BY)
    {
        const int32 A = GetNode(AX, AY), B = GetNode(BX, BY);
        FTreasureLayoutEdge Edge;
        Edge.A = A; Edge.B = B;
        Edge.Route = AY <= 1 && BY <= 1 ? ETreasureRoute::High
            : AY >= 3 && BY >= 3 ? ETreasureRoute::Low
            : AY != BY ? ETreasureRoute::Crossing : ETreasureRoute::Branch;
        Layout.Edges.Add(Edge);
    };

    // Seven deliberately different navigational questions, with 24m socket-aligned edges.
    switch (Layout.Topology)
    {
    case ETreasureMacroLayout::Switchback:
        Add(0,2,1,2); Add(1,2,1,1); Add(1,1,2,1); Add(2,1,2,2);
        Add(2,2,3,2); Add(3,2,3,3); Add(3,3,4,3); Add(4,3,4,2);
        Add(2,2,2,3); Add(1,1,1,0); break;
    case ETreasureMacroLayout::YFork:
        Add(0,2,1,2); Add(1,2,2,2); Add(2,2,2,1); Add(2,1,3,1);
        Add(3,1,4,1); Add(2,2,2,3); Add(2,3,3,3); Add(3,3,4,3);
        Add(3,1,3,0); break;
    case ETreasureMacroLayout::Ring:
        Add(0,2,1,2); Add(1,2,1,1); Add(1,1,2,1); Add(2,1,3,1);
        Add(3,1,3,2); Add(3,2,3,3); Add(3,3,2,3); Add(2,3,1,3);
        Add(1,3,1,2); Add(3,2,4,2); Add(2,1,2,0); break;
    case ETreasureMacroLayout::ShortLongRoutes:
        Add(0,2,1,2); Add(1,2,2,2); Add(2,2,3,2); Add(3,2,4,2);
        Add(1,2,1,3); Add(1,3,2,3); Add(2,3,3,3); Add(3,3,3,2);
        Add(2,2,2,1); Add(2,1,3,1); break;
    case ETreasureMacroLayout::CentralHub:
        Add(0,2,1,2); Add(1,2,2,2); Add(2,2,3,2); Add(3,2,4,2);
        Add(2,2,2,1); Add(2,1,2,0); Add(2,1,3,1); Add(3,1,3,2);
        Add(2,2,2,3); Add(2,3,2,4); Add(2,3,3,3); break;
    case ETreasureMacroLayout::Ladder:
        Add(0,2,1,2); Add(1,2,2,2); Add(2,2,3,2); Add(3,2,4,2);
        Add(1,2,1,1); Add(1,1,2,1); Add(2,1,3,1); Add(3,1,3,2);
        Add(2,2,2,3); Add(2,3,3,3); Add(3,3,4,3); break;
    default:
        Add(0,2,1,2); Add(1,2,1,1); Add(1,1,2,1); Add(2,1,3,1);
        Add(3,1,3,2); Add(3,2,4,2); Add(1,2,1,3); Add(1,3,2,3);
        Add(2,3,3,3); Add(3,3,3,2); Add(2,1,2,2); Add(2,2,2,3); break;
    }

    Layout.SpawnNode = NodeByCell[CellId(0,2)];
    Layout.Nodes[Layout.SpawnNode].Role = ETreasureNodeRole::Spawn;
    TArray<int32> Degree;
    Degree.Init(0, Layout.Nodes.Num());
    for (const FTreasureLayoutEdge& Edge : Layout.Edges) { ++Degree[Edge.A]; ++Degree[Edge.B]; }
    for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
    {
        if (Degree[Index] >= 3) { Layout.Nodes[Index].Role = ETreasureNodeRole::Junction; Layout.Nodes[Index].Region = ETreasureRegion::Cliff; }
    }

    // Prefer a far goal whose direct sightline crosses rock rather than a straight corridor.
    TArray<int32> Distance;
    Distance.Init(-1, Layout.Nodes.Num());
    TArray<int32> Parent;
    Parent.Init(INDEX_NONE, Layout.Nodes.Num());
    TArray<int32> Queue = { Layout.SpawnNode };
    Distance[Layout.SpawnNode] = 0;
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
        for (const FTreasureLayoutEdge& Edge : Layout.Edges)
        {
            const int32 Next = Edge.A == Queue[Head] ? Edge.B : Edge.B == Queue[Head] ? Edge.A : INDEX_NONE;
            if (Next != INDEX_NONE && Distance[Next] < 0)
            { Distance[Next] = Distance[Queue[Head]] + 1; Parent[Next] = Queue[Head]; Queue.Add(Next); }
        }
    auto TreasureForNode = [&](int32 Index)
    {
        const FTreasureLayoutNode& Node = Layout.Nodes[Index];
        const FVector2D Forward = Parent[Index] != INDEX_NONE
            ? (Node.Position - Layout.Nodes[Parent[Index]].Position).GetSafeNormal() : FVector2D(1.f, 0.f);
        switch (Layout.TreasureRelationship)
        {
        case ETreasureRelationship::BehindLandmark: return Node.Position + Forward * 500.f;
        case ETreasureRelationship::NearJunction:
            for (const FTreasureLayoutEdge& Edge : Layout.Edges)
            {
                const int32 Other = Edge.A == Index ? Edge.B : Edge.B == Index ? Edge.A : INDEX_NONE;
                if (Other == INDEX_NONE || Other == Parent[Index]) continue;
                const FVector2D Direction = (Layout.Nodes[Other].Position - Node.Position).GetSafeNormal();
                if (FMath::Abs(Direction.Y) > 0.5f) return Node.Position + Direction * 1900.f;
            }
            return Node.Position + FVector2D(0.f, Node.Cell.Y <= 2 ? -750.f : 750.f);
        case ETreasureRelationship::BelowCliff:
            return Node.Position + FVector2D(0.f, Node.Cell.Y <= 2 ? -750.f : 750.f);
        default: return Node.Position + Forward * 350.f;
        }
    };
    int32 BestScore = TNumericLimits<int32>::Lowest();
    for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
    {
        const FTreasureLayoutNode& Node = Layout.Nodes[Index];
        if (Node.Cell.X < 2 || Index == Layout.SpawnNode) continue;
        if (Layout.TreasureRelationship == ETreasureRelationship::NearJunction && Degree[Index] < 3) continue;
        const FVector2D Candidate = TreasureForNode(Index);
        if (!IsOccludedByCanyon(Layout, Candidate)) continue;
        const int32 Score = Distance[Index] * 10 + Node.Cell.X * 2 + (Degree[Index] == 1 ? 2 : 0);
        if (Score > BestScore) { BestScore = Score; Layout.TreasureNode = Index; }
    }
    if (Layout.TreasureNode == INDEX_NONE)
        for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
            if (Layout.Nodes[Index].Cell.X >= 2 && Distance[Index] > BestScore)
            { BestScore = Distance[Index]; Layout.TreasureNode = Index; }
    Layout.Nodes[Layout.TreasureNode].Role = ETreasureNodeRole::Treasure;
    const FTreasureLayoutNode& Goal = Layout.Nodes[Layout.TreasureNode];
    Layout.TreasurePosition = TreasureForNode(Layout.TreasureNode);
    Layout.TreasureHeight = Goal.FloorHeight;

    // Place four distinct, recognizable landmarks at an early clue, a junction, a
    // side route, and the goal. Every choice is tied to the gameplay graph.
    TArray<int32> Chosen;
    for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
        if (Distance[Index] == 1) { Chosen.Add(Index); break; }
    for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
        if (Degree[Index] >= 3 && Index != Layout.TreasureNode && !Chosen.Contains(Index))
        { Chosen.Add(Index); break; }
    int32 Remote = INDEX_NONE;
    int32 RemoteScore = -1;
    for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
        if (Index != Layout.TreasureNode && !Chosen.Contains(Index)
            && Distance[Index] > RemoteScore)
        { Remote = Index; RemoteScore = Distance[Index]; }
    if (Remote != INDEX_NONE) Chosen.Add(Remote);
    if (!Chosen.Contains(Layout.TreasureNode)) Chosen.Add(Layout.TreasureNode);
    for (int32 I = 0; I < Chosen.Num(); ++I)
    {
        Layout.Nodes[Chosen[I]].Landmark = static_cast<ETreasureLandmark>(1 + (static_cast<uint32>(Seed) + I * 3u) % 8u);
        if (Layout.Nodes[Chosen[I]].Role == ETreasureNodeRole::Route)
            Layout.Nodes[Chosen[I]].Role = ETreasureNodeRole::Landmark;
    }

    for (int32 Node = Layout.TreasureNode; Parent[Node] != INDEX_NONE; Node = Parent[Node])
        for (FTreasureLayoutEdge& Edge : Layout.Edges)
            if ((Edge.A == Node && Edge.B == Parent[Node]) || (Edge.B == Node && Edge.A == Parent[Node]))
            { Edge.bPrimary = true; Edge.Route = ETreasureRoute::Main; break; }
    return Layout;
}
