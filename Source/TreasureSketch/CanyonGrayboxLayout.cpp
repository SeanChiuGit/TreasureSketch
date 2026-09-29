#include "CanyonGrayboxLayout.h"

FCanyonGrayboxLayout FCanyonGrayboxLayout::Generate(int32 InSeed)
{
    FCanyonGrayboxLayout Plan;
    Plan.Seed = InSeed;
    FRandomStream R(InSeed ^ 0x4C7A21);
    Plan.Problem = static_cast<ECanyonGrayboxProblem>(R.RandRange(0, 5));
    const float L = R.FRandRange(13500.f, 18500.f);
    const float Branch = R.FRandRange(3300.f, 5700.f) * (R.RandRange(0, 1) == 0 ? -1.f : 1.f);
    const float ForkX = L * R.FRandRange(0.24f, 0.32f);
    const float MergeX = L * R.FRandRange(0.72f, 0.81f);
    const ECanyonGrayboxLandmark Early[] = {
        ECanyonGrayboxLandmark::Needle, ECanyonGrayboxLandmark::TwinPillars, ECanyonGrayboxLandmark::SplitPeak
    };
    const ECanyonGrayboxLandmark Crossing = R.RandRange(0, 1) == 0
        ? ECanyonGrayboxLandmark::Arch : ECanyonGrayboxLandmark::BrokenBridge;

    auto Add = [&](float X, float Y, float Z = 0.f,
                   ECanyonGrayboxLandmark Landmark = ECanyonGrayboxLandmark::None,
                   ECanyonRouteLayer Layer = ECanyonRouteLayer::Lower)
    {
        FCanyonGrayboxNode Node;
        Node.Position = FVector(X, Y, Z);
        Node.Landmark = Landmark;
        Node.Layer = Layer;
        return Plan.Nodes.Add(Node);
    };
    auto Link = [&](int32 A, int32 B, ECanyonRouteLayer Layer = ECanyonRouteLayer::Lower)
    {
        Plan.Edges.Add({ A, B, Layer, 450.f });
    };
    Plan.SpawnNode = Add(0.f, 0.f);
    const int32 First = Add(ForkX * 0.52f, R.FRandRange(-500.f, 500.f), 0.f, Early[R.RandRange(0, 2)]);
    const int32 Fork = Add(ForkX, 0.f);
    Link(Plan.SpawnNode, First);
    Link(First, Fork);

    switch (Plan.Problem)
    {
    case ECanyonGrayboxProblem::ForkRejoin:
    {
        const int32 High = Add((ForkX + MergeX) * 0.52f, Branch, R.FRandRange(200.f, 500.f), Crossing);
        const int32 Low = Add((ForkX + MergeX) * 0.57f, -Branch * 0.85f, -R.FRandRange(150.f, 350.f),
            ECanyonGrayboxLandmark::StoneRing);
        const int32 Merge = Add(MergeX, R.FRandRange(-500.f, 500.f));
        Plan.TreasureNode = Add(L, R.FRandRange(-900.f, 900.f));
        Link(Fork, High); Link(High, Merge); Link(Fork, Low); Link(Low, Merge); Link(Merge, Plan.TreasureNode);
        break;
    }
    case ECanyonGrayboxProblem::LoopShortcut:
    {
        const int32 Arc1 = Add(ForkX + (MergeX - ForkX) * 0.28f, Branch * 0.75f, 0.f, Crossing);
        const int32 Arc2 = Add(ForkX + (MergeX - ForkX) * 0.72f, Branch, 0.f,
            Plan.Nodes[First].Landmark == ECanyonGrayboxLandmark::TwinPillars
                ? ECanyonGrayboxLandmark::Needle : ECanyonGrayboxLandmark::TwinPillars);
        const int32 Merge = Add(MergeX, R.FRandRange(-600.f, 600.f));
        Plan.TreasureNode = Add(MergeX + R.FRandRange(1400.f, 2300.f), Branch * 1.32f);
        Link(Fork, Arc1); Link(Arc1, Arc2); Link(Arc2, Merge); Link(Fork, Merge); Link(Arc2, Plan.TreasureNode);
        break;
    }
    case ECanyonGrayboxProblem::HighLow:
    {
        const int32 Ridge = Add(ForkX + (MergeX - ForkX) * 0.44f, Branch, R.FRandRange(350.f, 500.f),
            Plan.Nodes[First].Landmark == ECanyonGrayboxLandmark::SplitPeak
                ? ECanyonGrayboxLandmark::Needle : ECanyonGrayboxLandmark::SplitPeak);
        const int32 Lookout = Add(MergeX, Branch * 0.95f, 450.f);
        const int32 Wash = Add(ForkX + (MergeX - ForkX) * 0.53f, -Branch * 0.8f, -250.f, Crossing);
        const int32 Bend = Add(MergeX, -Branch * 0.75f, -250.f);
        Plan.TreasureNode = Add(L, -Branch * 0.6f, -250.f);
        Link(Fork, Ridge); Link(Ridge, Lookout); Link(Fork, Wash); Link(Wash, Bend); Link(Bend, Plan.TreasureNode);
        break;
    }
    case ECanyonGrayboxProblem::HubPocket:
    {
        const int32 North = Add(ForkX + 1500.f, Branch, 0.f, Crossing);
        const int32 South = Add(ForkX + 1200.f, -Branch * 0.9f, 0.f, ECanyonGrayboxLandmark::StoneRing);
        const int32 Hub = Add(MergeX * 0.78f, Branch * 0.13f);
        Plan.TreasureNode = Add(MergeX, Branch * 0.5f);
        Link(Fork, North); Link(North, Hub); Link(Fork, South); Link(Fork, Hub); Link(Hub, Plan.TreasureNode);
        break;
    }
    case ECanyonGrayboxProblem::ChainAlcoves:
    {
        const int32 Hall1 = Add(ForkX + (MergeX - ForkX) * 0.28f, R.FRandRange(-800.f, 800.f), 0.f, Crossing);
        const int32 Hall2 = Add(ForkX + (MergeX - ForkX) * 0.75f, R.FRandRange(-800.f, 800.f), 0.f,
            ECanyonGrayboxLandmark::StoneRing);
        const int32 Alcove1 = Add(Plan.Nodes[Hall1].Position.X + 500.f, Branch);
        const int32 Alcove2 = Add(Plan.Nodes[Hall2].Position.X + 500.f, -Branch);
        const int32 End = Add(L, R.FRandRange(-500.f, 500.f));
        Link(Fork, Hall1); Link(Hall1, Hall2); Link(Hall2, End); Link(Hall1, Alcove1); Link(Hall2, Alcove2);
        Plan.TreasureNode = R.RandRange(0, 1) == 0 ? Alcove1 : Alcove2;
        break;
    }
    default: // Braided gorge
    {
        const int32 Upper1 = Add(ForkX + (MergeX - ForkX) * 0.30f, Branch, 0.f, Crossing);
        const int32 Upper2 = Add(ForkX + (MergeX - ForkX) * 0.75f, Branch * 0.87f);
        const int32 Middle = Add(ForkX + (MergeX - ForkX) * 0.50f, 0.f, 0.f,
            ECanyonGrayboxLandmark::StoneRing);
        const int32 Lower1 = Add(ForkX + (MergeX - ForkX) * 0.25f, -Branch);
        const int32 Lower2 = Add(ForkX + (MergeX - ForkX) * 0.72f, -Branch * 0.95f, 0.f,
            Plan.Nodes[First].Landmark == ECanyonGrayboxLandmark::Needle
                ? ECanyonGrayboxLandmark::SplitPeak : ECanyonGrayboxLandmark::Needle);
        const int32 Far = Add(MergeX, 0.f);
        Plan.TreasureNode = Add(L, Branch * (R.RandRange(0, 1) == 0 ? -0.7f : 0.7f));
        Link(Fork, Upper1); Link(Upper1, Upper2); Link(Upper2, Far);
        Link(Fork, Middle); Link(Middle, Far);
        Link(Fork, Lower1); Link(Lower1, Lower2); Link(Lower2, Far);
        Link(Upper1, Middle); Link(Middle, Lower2); Link(Far, Plan.TreasureNode);
        break;
    }
    }

    // Secondary loops keep the archetypes recognizable while adding local decisions.
    // The original edge remains as a shortcut; the new bend is an alternate route.
    const int32 CoreEdgeCount = Plan.Edges.Num();
    int32 Detours = 0;
    for (int32 Index = 0; Index < CoreEdgeCount && Detours < 5; ++Index)
    {
        const FCanyonGrayboxEdge Core = Plan.Edges[Index];
        const FVector A = Plan.Nodes[Core.A].Position, B = Plan.Nodes[Core.B].Position;
        const FVector2D Delta(B.X - A.X, B.Y - A.Y);
        if (Delta.Size() < 3200.f) continue;
        const FVector2D Side(-Delta.Y, Delta.X);
        const FVector2D Offset = Side.GetSafeNormal() * R.FRandRange(1500.f, 2400.f)
            * (R.RandRange(0, 1) == 0 ? -1.f : 1.f);
        const float T = R.FRandRange(0.38f, 0.62f);
        const FVector Mid = FMath::Lerp(A, B, T) + FVector(Offset.X, Offset.Y, 0.f);
        const int32 Bend = Add(Mid.X, Mid.Y, Mid.Z);
        Link(Core.A, Bend); Link(Bend, Core.B);
        ++Detours;
    }
    // Short dead ends make a wrong turn meaningful without making the goal unreachable.
    for (int32 Pocket = 0; Pocket < 2; ++Pocket)
    {
        const int32 Parent = Pocket == 0 ? First : Fork;
        const FVector P = Plan.Nodes[Parent].Position;
        const float Sign = Pocket == 0 ? 1.f : -1.f;
        const int32 End = Add(P.X + R.FRandRange(900.f, 1500.f),
            P.Y + Sign * R.FRandRange(1700.f, 2600.f), P.Z);
        Link(Parent, End);
    }

    // One playable upper corridor sits on the canyon wall, with two long grade-safe
    // ramps back to the lower graph and a second choice on the upper level.
    const float UpperSide = Plan.Nodes[Plan.TreasureNode].Position.Y >= 0.f ? -1.f : 1.f;
    const float UpperY = UpperSide * 10000.f;
    const float UpperZ = 1000.f;
    auto AddUpper = [&](float X, float Y)
    {
        return Add(X, Y, UpperZ, ECanyonGrayboxLandmark::None, ECanyonRouteLayer::Upper);
    };
    const int32 U0 = AddUpper(-2000.f, UpperY);
    const int32 U1 = AddUpper(ForkX + 1700.f, UpperY + UpperSide * 350.f);
    const int32 U2 = AddUpper((ForkX + MergeX) * 0.5f, UpperY - UpperSide * 450.f);
    const int32 U3 = AddUpper(MergeX - 1700.f, UpperY + UpperSide * 300.f);
    const int32 U4 = AddUpper(L + 2000.f, UpperY);
    const int32 EntryFlat = Add(-2500.f, UpperY * 0.10f, 0.f,
        ECanyonGrayboxLandmark::None, ECanyonRouteLayer::Ramp);
    const int32 EntryMiddle = Add(-3000.f, UpperY * 0.55f, 500.f,
        ECanyonGrayboxLandmark::None, ECanyonRouteLayer::Ramp);
    Link(Plan.SpawnNode, EntryFlat, ECanyonRouteLayer::Ramp);
    Link(EntryFlat, EntryMiddle, ECanyonRouteLayer::Ramp);
    Link(EntryMiddle, U0, ECanyonRouteLayer::Ramp);
    Link(U0, U1, ECanyonRouteLayer::Upper);
    Link(U1, U2, ECanyonRouteLayer::Upper);
    Link(U2, U3, ECanyonRouteLayer::Upper);
    Link(U3, U4, ECanyonRouteLayer::Upper);
    const float TreasureY = Plan.Nodes[Plan.TreasureNode].Position.Y;
    const float TreasureZ = Plan.Nodes[Plan.TreasureNode].Position.Z;
    const int32 ExitMiddle = Add(L + 3000.f, UpperY * 0.55f + TreasureY * 0.45f,
        (UpperZ + TreasureZ) * 0.5f,
        ECanyonGrayboxLandmark::None, ECanyonRouteLayer::Ramp);
    const int32 ExitFlat = Add(L + 2500.f, UpperY * 0.15f + TreasureY * 0.85f, TreasureZ,
        ECanyonGrayboxLandmark::None, ECanyonRouteLayer::Ramp);
    Link(U4, ExitMiddle, ECanyonRouteLayer::Ramp);
    Link(ExitMiddle, ExitFlat, ECanyonRouteLayer::Ramp);
    Link(ExitFlat, Plan.TreasureNode, ECanyonRouteLayer::Ramp);
    const int32 U5 = AddUpper(ForkX + (MergeX - ForkX) * 0.40f, UpperY + UpperSide * 2000.f);
    const int32 U6 = AddUpper(ForkX + (MergeX - ForkX) * 0.73f, UpperY + UpperSide * 2000.f);
    Link(U1, U5, ECanyonRouteLayer::Upper);
    Link(U5, U6, ECanyonRouteLayer::Upper);
    Link(U6, U3, ECanyonRouteLayer::Upper);

    // Three character capsules are 252 cm across; the narrow flat corridors are
    // 340 cm wide before their steep banks. Other edges stay noticeably wider.
    int32 LowerIndex = 0;
    for (FCanyonGrayboxEdge& Edge : Plan.Edges)
    {
        if (Edge.Layer == ECanyonRouteLayer::Lower)
        {
            Edge.HalfWidth = (LowerIndex == 1 || LowerIndex == 3 || R.FRand() < 0.27f)
                ? 170.f : R.FRandRange(380.f, 600.f);
            ++LowerIndex;
        }
        else Edge.HalfWidth = Edge.Layer == ECanyonRouteLayer::Upper ? 420.f : 460.f;
    }

    const float Angle = R.FRandRange(-PI, PI);
    const float Scale = R.FRandRange(0.88f, 1.12f);
    const float C = FMath::Cos(Angle), S = FMath::Sin(Angle);
    FVector2D Minimum(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
    FVector2D Maximum(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
    for (FCanyonGrayboxNode& Node : Plan.Nodes)
    {
        const float X = Node.Position.X * Scale, Y = Node.Position.Y * Scale;
        Node.Position.X = X * C - Y * S;
        Node.Position.Y = X * S + Y * C;
        Minimum.X = FMath::Min(Minimum.X, Node.Position.X);
        Minimum.Y = FMath::Min(Minimum.Y, Node.Position.Y);
        Maximum.X = FMath::Max(Maximum.X, Node.Position.X);
        Maximum.Y = FMath::Max(Maximum.Y, Node.Position.Y);
    }
    const FVector2D Center = (Minimum + Maximum) * 0.5f;
    for (FCanyonGrayboxNode& Node : Plan.Nodes)
    {
        Node.Position.X -= Center.X;
        Node.Position.Y -= Center.Y;
    }
    return Plan;
}

const TCHAR* FCanyonGrayboxLayout::ProblemName() const
{
    switch (Problem)
    {
    case ECanyonGrayboxProblem::ForkRejoin: return TEXT("ForkRejoin");
    case ECanyonGrayboxProblem::LoopShortcut: return TEXT("LoopShortcut");
    case ECanyonGrayboxProblem::HighLow: return TEXT("HighLow");
    case ECanyonGrayboxProblem::HubPocket: return TEXT("HubPocket");
    case ECanyonGrayboxProblem::ChainAlcoves: return TEXT("ChainAlcoves");
    default: return TEXT("BraidedGorge");
    }
}

void FCanyonGrayboxLayout::ScaleForMap(float Scale)
{
    LengthScale = FMath::Sqrt(FMath::Clamp(Scale, 0.5f, 5.f));
    for (FCanyonGrayboxNode& Node : Nodes) Node.Position *= LengthScale;
    for (FCanyonGrayboxEdge& Edge : Edges)
        Edge.HalfWidth = Edge.Layer == ECanyonRouteLayer::Lower
            ? FMath::Max(170.f, Edge.HalfWidth * LengthScale)
            : Edge.HalfWidth * LengthScale;
}

bool FCanyonGrayboxLayout::Validate() const
{
    if (!Nodes.IsValidIndex(SpawnNode) || !Nodes.IsValidIndex(TreasureNode) || Nodes.Num() < 6) return false;
    TArray<bool> Seen;
    Seen.Init(false, Nodes.Num());
    TArray<int32> Queue = { SpawnNode };
    Seen[SpawnNode] = true;
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
        for (const FCanyonGrayboxEdge& Edge : Edges)
        {
            if (!Nodes.IsValidIndex(Edge.A) || !Nodes.IsValidIndex(Edge.B)) return false;
            const FVector Delta = Nodes[Edge.B].Position - Nodes[Edge.A].Position;
            if (Delta.Size2D() < 900.f || FMath::Abs(Delta.Z) / Delta.Size2D() > 0.18f) return false;
            const int32 Next = Edge.A == Queue[Head] ? Edge.B : Edge.B == Queue[Head] ? Edge.A : INDEX_NONE;
            if (Next != INDEX_NONE && !Seen[Next]) { Seen[Next] = true; Queue.Add(Next); }
        }
    return Queue.Num() == Nodes.Num();
}

float FCanyonGrayboxLayout::HeightAt(float X, float Y, float* DistanceFromRoute,
    ECanyonRouteLayer* SurfaceLayer) const
{
    const FVector2D Point(X, Y);
    float LayerHeight[3] = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(),
        TNumericLimits<float>::Max() };
    float LayerDistance[3] = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(),
        TNumericLimits<float>::Max() };
    auto Consider = [&](float Distance, float HalfWidth, float FloorZ, float Rise, ECanyonRouteLayer Layer)
    {
        const int32 Index = static_cast<int32>(Layer);
        // Within one level the closest route owns the surface. Taking the lowest
        // height from every adjoining branch creates sudden drops at high/low forks.
        if (Distance >= LayerDistance[Index]) return;
        const float T = FMath::Clamp((Distance - HalfWidth) / (470.f * LengthScale), 0.f, 1.f);
        LayerHeight[Index] = FloorZ + Rise * T * T * (3.f - 2.f * T);
        LayerDistance[Index] = Distance;
    };
    for (const FCanyonGrayboxEdge& Edge : Edges)
    {
        const FVector& A = Nodes[Edge.A].Position;
        const FVector& B = Nodes[Edge.B].Position;
        const FVector2D Start(A.X, A.Y), Delta(B.X - A.X, B.Y - A.Y);
        const float T = FMath::Clamp(FVector2D::DotProduct(Point - Start, Delta) / Delta.SizeSquared(), 0.f, 1.f);
        const float Distance = FVector2D::Distance(Point, Start + Delta * T);
        const float FloorZ = FMath::Lerp(A.Z, B.Z, T);
        const float Rise = Edge.Layer == ECanyonRouteLayer::Upper ? 1200.f * LengthScale
            : Edge.Layer == ECanyonRouteLayer::Ramp ? 2200.f * LengthScale - FloorZ
            : 2200.f * LengthScale;
        Consider(Distance, Edge.HalfWidth, FloorZ, Rise, Edge.Layer);
    }
    for (const FCanyonGrayboxNode& Node : Nodes)
    {
        const float Distance = FVector2D::Distance(Point, FVector2D(Node.Position.X, Node.Position.Y));
        Consider(Distance * 0.72f,
            (Node.Layer == ECanyonRouteLayer::Upper ? 480.f : 440.f) * LengthScale,
            Node.Position.Z, Node.Layer == ECanyonRouteLayer::Upper ? 1200.f * LengthScale
                : Node.Layer == ECanyonRouteLayer::Ramp ? 2200.f * LengthScale - Node.Position.Z
                : 2200.f * LengthScale,
            Node.Layer);
    }
    int32 BestIndex = 0;
    for (int32 Index = 1; Index < 3; ++Index)
        if (LayerHeight[Index] < LayerHeight[BestIndex]) BestIndex = Index;
    const float BestDistance = LayerDistance[BestIndex];
    if (DistanceFromRoute) *DistanceFromRoute = BestDistance;
    if (SurfaceLayer) *SurfaceLayer = static_cast<ECanyonRouteLayer>(BestIndex);
    const float Detail = FMath::Sin(X * 0.0023f + Seed * 0.03f) * FMath::Cos(Y * 0.0027f - Seed * 0.04f)
        * (BestDistance < 550.f * LengthScale ? 6.f : 25.f);
    return 600.f + LayerHeight[BestIndex] + Detail;
}
