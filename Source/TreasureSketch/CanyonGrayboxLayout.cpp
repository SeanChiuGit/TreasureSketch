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
                   ECanyonGrayboxLandmark Landmark = ECanyonGrayboxLandmark::None)
    {
        FCanyonGrayboxNode Node;
        Node.Position = FVector(X, Y, Z);
        Node.Landmark = Landmark;
        return Plan.Nodes.Add(Node);
    };
    auto Link = [&](int32 A, int32 B) { Plan.Edges.Add({ A, B }); };
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

float FCanyonGrayboxLayout::HeightAt(float X, float Y, float* DistanceFromRoute) const
{
    const FVector2D Point(X, Y);
    float Best = TNumericLimits<float>::Max();
    float FloorZ = 0.f;
    for (const FCanyonGrayboxEdge& Edge : Edges)
    {
        const FVector& A = Nodes[Edge.A].Position;
        const FVector& B = Nodes[Edge.B].Position;
        const FVector2D Start(A.X, A.Y), Delta(B.X - A.X, B.Y - A.Y);
        const float T = FMath::Clamp(FVector2D::DotProduct(Point - Start, Delta) / Delta.SizeSquared(), 0.f, 1.f);
        const float Distance = FVector2D::Distance(Point, Start + Delta * T);
        if (Distance < Best) { Best = Distance; FloorZ = FMath::Lerp(A.Z, B.Z, T); }
    }
    for (const FCanyonGrayboxNode& Node : Nodes)
    {
        const float Distance = FVector2D::Distance(Point, FVector2D(Node.Position.X, Node.Position.Y)) * 0.72f;
        if (Distance < Best) { Best = Distance; FloorZ = Node.Position.Z; }
    }
    if (DistanceFromRoute) *DistanceFromRoute = Best;
    const float T = FMath::Clamp((Best - 500.f) / 550.f, 0.f, 1.f);
    const float Wall = T * T * (3.f - 2.f * T) * 1400.f;
    const float Detail = FMath::Sin(X * 0.0023f + Seed * 0.03f) * FMath::Cos(Y * 0.0027f - Seed * 0.04f)
        * FMath::Lerp(6.f, 42.f, T);
    return 600.f + FloorZ + Wall + Detail;
}
