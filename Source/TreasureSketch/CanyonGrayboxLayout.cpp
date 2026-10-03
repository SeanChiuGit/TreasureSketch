#include "CanyonGrayboxLayout.h"

namespace
{
TArray<TArray<FVector>> MakeLoopPaths(const FVector& MouthA, const FVector& MouthB,
    const FVector& Outward, int32 Seed)
{
    const FVector Direction = (MouthB - MouthA).GetSafeNormal2D();
    FRandomStream Random(Seed ^ 0x4C4F4F50);
    const float Separation = Random.FRandRange(2800.f, 3600.f);
    const float Reach = Random.FRandRange(6000.f, 8000.f);
    const FVector Start = MouthA - Direction * Reach - Outward * 7000.f + FVector(0.f, 0.f, -1700.f);
    const FVector End = MouthB + Direction * 3500.f - Outward * 7000.f + FVector(0.f, 0.f, -1500.f);
    TArray<TArray<FVector>> Paths;
    Paths.Add({ MouthA, MouthA - Direction * (Reach + 2500.f) - Outward * 3000.f
        + FVector(0.f, 0.f, -900.f), Start });
    for (const int32 Sign : { -1, 1 })
    {
        TArray<FVector> Arm = { Start };
        for (const float Fraction : { 0.2f, 0.5f, 0.8f })
            Arm.Add(FMath::Lerp(Start, End, Fraction)
                + Outward * (Sign * Separation * (Fraction == 0.5f ? 1.25f : 1.f))
                + FVector(0.f, 0.f, Sign < 0 ? -350.f : 150.f));
        Arm.Add(End);
        Paths.Add(MoveTemp(Arm));
    }
    Paths.Add({ End, MouthB + Direction * 6500.f - Outward * 3000.f
        + FVector(0.f, 0.f, -700.f), MouthB });
    return Paths;
}

struct FDeadEndShape
{
    ECanyonDeadEndKind Kind;
    TArray<TArray<FVector>> Paths;
    TArray<FVector> Ends;
};

struct FLongCaveShape
{
    TArray<FVector> Main;
    TArray<FDeadEndShape> DeadEnds;
};

FDeadEndShape MakeDeadEnd(const FVector& Junction, const FVector& Direction,
    const FVector& IntoRock, ECanyonDeadEndKind Kind, FRandomStream& Random)
{
    FDeadEndShape Shape;
    Shape.Kind = Kind;
    if (Kind == ECanyonDeadEndKind::ShortAlcove)
    {
        const FVector End = Junction + IntoRock * Random.FRandRange(1700.f, 2300.f)
            - Direction * 250.f + FVector(0.f, 0.f, -100.f);
        Shape.Paths.Add({ Junction, End });
        Shape.Ends.Add(End);
    }
    else if (Kind == ECanyonDeadEndKind::LongWinding)
    {
        const float Stretch = Random.FRandRange(0.9f, 1.12f);
        const FVector P1 = Junction + IntoRock * (2500.f * Stretch) - Direction * 1000.f + FVector(0.f, 0.f, -300.f);
        const FVector P2 = Junction + IntoRock * (6000.f * Stretch) - Direction * 1400.f + FVector(0.f, 0.f, -650.f);
        const FVector P3 = Junction + IntoRock * (8500.f * Stretch) + Direction * 1200.f + FVector(0.f, 0.f, -850.f);
        const FVector End = Junction + IntoRock * (11000.f * Stretch) + Direction * 800.f + FVector(0.f, 0.f, -850.f);
        Shape.Paths.Add({ Junction, P1, P2, P3, End });
        Shape.Ends.Add(End);
    }
    else
    {
        const FVector Fork = Junction + IntoRock * 2700.f + Direction * 350.f + FVector(0.f, 0.f, -350.f);
        Shape.Paths.Add({ Junction, Fork });
        for (int32 Sign : { -1, 1 })
        {
            const FVector Bend = Fork + IntoRock * 1200.f + Direction * (Sign * 1700.f) + FVector(0.f, 0.f, -150.f);
            const FVector End = Fork + IntoRock * Random.FRandRange(2400.f, 2900.f)
                + Direction * (Sign * 2500.f) + FVector(0.f, 0.f, -150.f);
            Shape.Paths.Add({ Fork, Bend, End });
            Shape.Ends.Add(End);
        }
    }
    return Shape;
}

FLongCaveShape MakeLongCave(const FVector& MouthA, const FVector& MouthB,
    const FVector& Outward, int32 Seed, bool bPreview)
{
    const FVector Direction = (MouthB - MouthA).GetSafeNormal2D();
    const float Drop = FMath::Min(2000.f, 500.f + 0.22f * (FVector::Dist2D(MouthA, MouthB) + 1500.f)
        - FMath::Abs(MouthA.Z - MouthB.Z));
    const FVector FirstPass = MouthA - Direction * 6500.f - Outward * 7000.f + FVector(0.f, 0.f, -2200.f);
    const FVector LastPass = MouthB + Direction * 2500.f - Outward * 7000.f + FVector(0.f, 0.f, -Drop - 900.f);
    FRandomStream Random(Seed ^ 0x4252414E);
    const int32 Count = bPreview ? 3 : (Seed != 1001 && Random.FRand() < 0.6f ? Random.RandRange(1, 3) : 0);
    TArray<ECanyonDeadEndKind> Kinds = { ECanyonDeadEndKind::ShortAlcove, ECanyonDeadEndKind::LongWinding, ECanyonDeadEndKind::Forked };
    if (!bPreview)
        for (int32 I = 2; I > 0; --I) Kinds.Swap(I, Random.RandRange(0, I));
    FLongCaveShape Shape;
    TArray<FVector> Main = { MouthA, FirstPass };
    for (int32 I = 0; I < Count; ++I)
    {
        const float PreviewFractions[] = { 0.14f, 0.48f, 0.84f };
        const FVector Junction = FMath::Lerp(FirstPass, LastPass,
            bPreview ? PreviewFractions[I] : (I + 1.f) / (Count + 1.f));
        Main.Add(Junction);
        FDeadEndShape Addition = MakeDeadEnd(Junction, Direction, -Outward, Kinds[I], Random);
        bool bSeparate = true;
        for (const FDeadEndShape& Existing : Shape.DeadEnds)
            for (const auto& Path : Addition.Paths)
                for (int32 Segment = 1; Segment < Path.Num(); ++Segment)
                    for (int32 Sample = 0; Sample <= 8; ++Sample)
                    {
                        const FVector Probe = FMath::Lerp(Path[Segment - 1], Path[Segment], Sample / 8.f);
                        for (const auto& OtherPath : Existing.Paths)
                            for (int32 Edge = 1; Edge < OtherPath.Num(); ++Edge)
                            {
                                const FVector P = OtherPath[Edge - 1], Q = OtherPath[Edge];
                                const FVector2D Delta(Q.X - P.X, Q.Y - P.Y);
                                const float T = FMath::Clamp(FVector2D::DotProduct(
                                    FVector2D(Probe.X - P.X, Probe.Y - P.Y), Delta) / Delta.SizeSquared(), 0.f, 1.f);
                                if (FVector::Dist2D(Probe, FMath::Lerp(P, Q, T)) < 1900.f) bSeparate = false;
                            }
                    }
        if (bSeparate) Shape.DeadEnds.Add(MoveTemp(Addition));
    }
    Main.Append({ LastPass,
        MouthB + Direction * 2500.f - Outward * 5000.f + FVector(0.f, 0.f, -Drop - 900.f),
        MouthA - Direction * 1500.f - Outward * 5000.f + FVector(0.f, 0.f, -Drop + 300.f),
        MouthA - Direction * 1500.f - Outward * 3000.f + FVector(0.f, 0.f, -Drop),
        MouthB - Outward * 3000.f + FVector(0.f, 0.f, -700.f), MouthB });
    Shape.Main = MoveTemp(Main);
    return Shape;
}
}

FCanyonGrayboxLayout FCanyonGrayboxLayout::Generate(int32 InSeed, float MapScale, bool bHallPreview, bool bLoopPreview, bool bBranchPreview)
{
    FCanyonGrayboxLayout Plan;
    Plan.Seed = InSeed;
    FRandomStream CaveTypeRandom(InSeed ^ 0x54595045);
    Plan.CavePattern = static_cast<ECanyonCavePattern>(CaveTypeRandom.RandRange(0, 3));
    if (bHallPreview || InSeed == 1002 || InSeed == 1003)
        Plan.CavePattern = ECanyonCavePattern::ThreeMouthHall;
    else if (InSeed == 1001) Plan.CavePattern = ECanyonCavePattern::LongWindingThrough;
    else if (InSeed == 1000) Plan.CavePattern = ECanyonCavePattern::ThroughShortcut;
    if (bLoopPreview || InSeed == 1010) Plan.CavePattern = ECanyonCavePattern::LongLoop;
    if (bBranchPreview || InSeed == 1020) Plan.CavePattern = ECanyonCavePattern::LongWindingThrough;
    if (InSeed == 1030) Plan.CavePattern = ECanyonCavePattern::ThreeMouthHall;
    if (InSeed == 1040) Plan.CavePattern = ECanyonCavePattern::LongLoop;
    if (InSeed == 1050 || InSeed == 1051) Plan.CavePattern = ECanyonCavePattern::LongWindingThrough;
    const float Area = FMath::IsFinite(MapScale) ? FMath::Clamp(MapScale, 0.5f, 5.f) : 1.f;
    // The comparison seeds share geometry; ordinary hall seeds vary the whole map.
    const int32 LayoutSeed = InSeed == 1003 || InSeed == 1030 ? 1002
        : InSeed == 1020 || InSeed == 1050 || InSeed == 1051 ? 1001 : InSeed == 1040 ? 1010 : InSeed;
    FRandomStream R(LayoutSeed ^ 0x4C7A21);
    const int32 UpperRoll = R.RandRange(0, 4);
    Plan.UpperPattern = UpperRoll < 2 ? ECanyonUpperPattern::Lookout
        : UpperRoll < 4 ? ECanyonUpperPattern::Traverse : ECanyonUpperPattern::SplitTraverse;
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

    const float UpperSide = Plan.UpperPattern == ECanyonUpperPattern::Lookout
        ? (R.RandRange(0, 1) == 0 ? -1.f : 1.f)
        : (Plan.Nodes[Plan.TreasureNode].Position.Y >= 0.f ? -1.f : 1.f);

    // Scatter a few local alternatives across the graph instead of repeating the
    // same small loop next to the first junction on every seed.
    const int32 CoreEdgeCount = Plan.Edges.Num();
    struct FDetour { int32 CoreEdge; int32 Bend; };
    TArray<FDetour> DetourRoutes;
    TArray<int32> CandidateEdges;
    for (int32 Index = 0; Index < CoreEdgeCount; ++Index)
        if (Plan.Edges[Index].Layer == ECanyonRouteLayer::Lower
            && FVector::Dist2D(Plan.Nodes[Plan.Edges[Index].A].Position,
                Plan.Nodes[Plan.Edges[Index].B].Position) >= 3200.f)
            CandidateEdges.Add(Index);
    for (int32 Index = CandidateEdges.Num() - 1; Index > 0; --Index)
        CandidateEdges.Swap(Index, R.RandRange(0, Index));
    if (CandidateEdges.Num() > 0)
    {
        int32 Longest = 0;
        float LongestLength = 0.f;
        for (int32 Choice = 0; Choice < CandidateEdges.Num(); ++Choice)
        {
            const FCanyonGrayboxEdge& Edge = Plan.Edges[CandidateEdges[Choice]];
            const float Length = FVector::Dist2D(Plan.Nodes[Edge.A].Position, Plan.Nodes[Edge.B].Position);
            if (Length > LongestLength) { LongestLength = Length; Longest = Choice; }
        }
        CandidateEdges.Swap(0, Longest);
    }
    const int32 DetourCount = FMath::Min(CandidateEdges.Num(), 2 + R.RandRange(0, 1));
    for (int32 Choice = 0; Choice < DetourCount; ++Choice)
    {
        const int32 Index = CandidateEdges[Choice];
        if (Choice == 0)
        {
            // This edge becomes a complete mountain crossing after the other
            // regions exist, so its rock mass can be placed in open space.
            DetourRoutes.Add({ Index, INDEX_NONE });
            continue;
        }
        const FCanyonGrayboxEdge Core = Plan.Edges[Index];
        const FVector A = Plan.Nodes[Core.A].Position, B = Plan.Nodes[Core.B].Position;
        const FVector2D Delta(B.X - A.X, B.Y - A.Y);
        const FVector2D Side(-Delta.Y, Delta.X);
        const FVector2D Offset = Side.GetSafeNormal() * R.FRandRange(1800.f, 3000.f)
            * (R.RandRange(0, 1) == 0 ? -1.f : 1.f);
        const float T = R.FRandRange(0.38f, 0.62f);
        const FVector Mid = FMath::Lerp(A, B, T) + FVector(Offset.X, Offset.Y, 0.f);
        const int32 Bend = Add(Mid.X, Mid.Y, Mid.Z);
        Link(Core.A, Bend); Link(Bend, Core.B);
        DetourRoutes.Add({ Index, Bend });
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

    // A larger play area gains additional destinations and crossings. The base
    // problem stays recognizable for a seed, but its graph is no longer a zoom.
    const int32 AdditionalRegions = FMath::Clamp(FMath::FloorToInt((Area - 1.f) * 1.05f + 0.1f), 0, 4);
    const int32 RegionCount = (Area >= 1.f ? 1 : 0) + AdditionalRegions;
    for (int32 Region = 0; Region < RegionCount && CandidateEdges.Num() > 0; ++Region)
    {
        const int32 RegionEdge = CandidateEdges.Num() > 1
            ? CandidateEdges[1 + (DetourCount - 1 + Region) % (CandidateEdges.Num() - 1)]
            : CandidateEdges[0];
        const FCanyonGrayboxEdge Core = Plan.Edges[RegionEdge];
        const FVector A = Plan.Nodes[Core.A].Position, B = Plan.Nodes[Core.B].Position;
        const FVector Direction = (B - A).GetSafeNormal2D();
        const FVector Normal(-Direction.Y, Direction.X, 0.f);
        const FVector Mid = (A + B) * 0.5f;
        auto OpenSpace = [&](float Sign)
        {
            const FVector Probe = Mid + Normal * (Sign * 6500.f);
            float Distance = TNumericLimits<float>::Max();
            for (const FCanyonGrayboxNode& Node : Plan.Nodes)
                Distance = FMath::Min(Distance, FVector::Dist2D(Probe, Node.Position));
            // Keep new lower regions away from the upper shelf: the heightfield
            // cannot represent a bridge and a floor at the same XY coordinate.
            return Distance - FMath::Max(0.f, Probe.Y * UpperSide) * 10.f;
        };
        const float Sign = OpenSpace(1.f) > OpenSpace(-1.f) ? 1.f : -1.f;
        const float Offset = R.FRandRange(3200.f, 4700.f);
        if (Region % 2 == 0)
        {
            const int32 Anchor = R.RandRange(0, 1) == 0 ? Core.A : Core.B;
            const FVector Start = Plan.Nodes[Anchor].Position;
            const FVector Bend = Start + Normal * (Sign * Offset * 0.62f)
                + Direction * R.FRandRange(500.f, 1100.f);
            const FVector End = Start + Normal * (Sign * Offset * 1.5f)
                + Direction * R.FRandRange(1000.f, 1800.f);
            const FVector OtherEnd = Start + Normal * (Sign * Offset * 1.2f)
                - Direction * R.FRandRange(1400.f, 2200.f);
            const int32 BranchNode = Add(Bend.X, Bend.Y, Start.Z);
            const int32 EndNode = Add(End.X, End.Y, Start.Z,
                Region == 0 ? ECanyonGrayboxLandmark::StoneRing : ECanyonGrayboxLandmark::Needle);
            const int32 OtherNode = Add(OtherEnd.X, OtherEnd.Y, Start.Z);
            Link(Anchor, BranchNode); Link(BranchNode, EndNode); Link(BranchNode, OtherNode);
        }
        else
        {
            const FVector P = FMath::Lerp(A, B, 0.30f) + Normal * (Sign * Offset);
            const FVector Q = FMath::Lerp(A, B, 0.70f) + Normal * (Sign * Offset * 1.12f);
            const int32 FirstOuter = Add(P.X, P.Y, P.Z, ECanyonGrayboxLandmark::SplitPeak);
            const int32 SecondOuter = Add(Q.X, Q.Y, Q.Z);
            Link(Core.A, FirstOuter); Link(FirstOuter, SecondOuter); Link(SecondOuter, Core.B);
        }
    }

    // The upper layer may be a branching lookout, a crossing, or a crossing
    // with its own inner choice. Only the latter produces the familiar ring.
    const float UpperY = UpperSide * 10000.f;
    const float UpperZ = 1000.f;
    auto AddUpper = [&](float X, float Y)
    {
        return Add(X, Y, UpperZ, ECanyonGrayboxLandmark::None, ECanyonRouteLayer::Upper);
    };
    const int32 U0 = AddUpper(-2000.f, UpperY);
    const int32 U1 = AddUpper(ForkX + 1700.f, UpperY + UpperSide * 350.f);
    const int32 U2 = AddUpper((ForkX + MergeX) * 0.5f, UpperY - UpperSide * 450.f);
    const int32 EntryFlat = Add(-2500.f, UpperY * 0.10f, 0.f,
        ECanyonGrayboxLandmark::None, ECanyonRouteLayer::Ramp);
    const int32 EntryMiddle = Add(-3000.f, UpperY * 0.55f, 500.f,
        ECanyonGrayboxLandmark::None, ECanyonRouteLayer::Ramp);
    Link(Plan.SpawnNode, EntryFlat, ECanyonRouteLayer::Ramp);
    Link(EntryFlat, EntryMiddle, ECanyonRouteLayer::Ramp);
    Link(EntryMiddle, U0, ECanyonRouteLayer::Ramp);
    Link(U0, U1, ECanyonRouteLayer::Upper);
    Link(U1, U2, ECanyonRouteLayer::Upper);
    if (Plan.UpperPattern == ECanyonUpperPattern::Lookout)
    {
        const int32 End = AddUpper(MergeX + 700.f, UpperY + UpperSide * 1600.f);
        const int32 SideEnd = AddUpper(ForkX + (MergeX - ForkX) * 0.58f,
            UpperY + UpperSide * 2600.f);
        Plan.Nodes[End].Landmark = ECanyonGrayboxLandmark::Needle;
        Link(U2, End, ECanyonRouteLayer::Upper);
        Link(U1, SideEnd, ECanyonRouteLayer::Upper);
    }
    else
    {
        const int32 U3 = AddUpper(MergeX - 1700.f, UpperY + UpperSide * 300.f);
        const int32 U4 = AddUpper(L + 2000.f, UpperY);
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
        if (Plan.UpperPattern == ECanyonUpperPattern::SplitTraverse)
        {
            const int32 U5 = AddUpper(ForkX + (MergeX - ForkX) * 0.40f, UpperY + UpperSide * 2000.f);
            const int32 U6 = AddUpper(ForkX + (MergeX - ForkX) * 0.73f, UpperY + UpperSide * 2000.f);
            Link(U1, U5, ECanyonRouteLayer::Upper);
            Link(U5, U6, ECanyonRouteLayer::Upper);
            Link(U6, U3, ECanyonRouteLayer::Upper);
        }
    }

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

    // Move the crossing into an open side of the canyon. The exterior approach
    // meets two rock faces; the longer surface road goes around the same mass.
    TArray<FCanyonGrayboxEdge> CaveCandidates;
    for (const int32 Index : CandidateEdges) CaveCandidates.Add(Plan.Edges[Index]);
    TArray<ECanyonCavePattern> CaveTypes = { Plan.CavePattern };
    if (!bHallPreview && !bLoopPreview && !bBranchPreview && InSeed != 1010 && InSeed != 1020
        && InSeed != 1030 && InSeed != 1040 && InSeed != 1050 && InSeed != 1051
        && (InSeed < 1000 || InSeed > 1003))
    {
        const ECanyonCavePattern Second = Plan.CavePattern == ECanyonCavePattern::ThroughShortcut
            ? static_cast<ECanyonCavePattern>(CaveTypeRandom.RandRange(1, 3))
            : ECanyonCavePattern::ThroughShortcut;
        CaveTypes.Add(Second);
        if (Area >= 2.f)
            for (int32 Type = 0; Type < 4 && CaveTypes.Num() < (Area >= 4.f ? 4 : 3); ++Type)
                if (!CaveTypes.Contains(static_cast<ECanyonCavePattern>(Type)))
                    CaveTypes.Add(static_cast<ECanyonCavePattern>(Type));
    }
    for (int32 CaveIndex = 0; CaveIndex < CaveTypes.Num() && DetourRoutes.Num() > 0; ++CaveIndex)
    {
        const FCanyonGrayboxLayout BeforeCave = Plan;
        Plan.CavePattern = CaveTypes[CaveIndex];
        Plan.CaveMouthNodes.Reset();
        Plan.CavePathNodes.Reset();
        Plan.CaveBranches.Reset();
        Plan.CaveBranchOpen.Reset();
        Plan.CaveHalls.Reset();
        Plan.CaveDeadEnds.Reset();
        CandidateEdges.Reset();
        for (const FCanyonGrayboxEdge& Candidate : CaveCandidates)
            for (int32 Index = 0; Index < Plan.Edges.Num(); ++Index)
                if (Plan.Edges[Index].A == Candidate.A && Plan.Edges[Index].B == Candidate.B
                    && !Plan.Edges[Index].bCave) { CandidateEdges.Add(Index); break; }
        if (CandidateEdges.IsEmpty()) break;
        int32 ChosenEdge = CandidateEdges[0];
        float ChosenSign = 1.f, ChosenDistance = 5400.f;
        float BestScore = -TNumericLimits<float>::Max();
        for (int32 Candidate : CandidateEdges)
        {
            const FCanyonGrayboxEdge& Route = Plan.Edges[Candidate];
            const FVector From = Plan.Nodes[Route.A].Position;
            const FVector To = Plan.Nodes[Route.B].Position;
            if (Plan.CavePattern == ECanyonCavePattern::ThreeMouthHall
                && FVector::Dist2D(From, To) < 2000.f) continue;
            const FVector AlongCandidate = (To - From).GetSafeNormal2D();
            const FVector NormalCandidate(-AlongCandidate.Y, AlongCandidate.X, 0.f);
            for (float Distance : { 5400.f, 7000.f, 8500.f })
                for (int32 Sign : { -1, 1 })
                {
                    if (Plan.CavePattern != ECanyonCavePattern::ThroughShortcut
                        && Distance < 8500.f) continue;
                    if (CaveIndex > 0)
                    {
                        const FVector Offset = NormalCandidate * (Sign * Distance);
                        const FVector FirstMouth = From + Offset, SecondMouth = To + Offset;
                        const FVector Bend = (FirstMouth + SecondMouth) * 0.5f
                            + NormalCandidate * (Sign * (Plan.CavePattern == ECanyonCavePattern::ThroughShortcut ? 4500.f : 6700.f));
                        const FVector Proposed[4][2] = { { From, FirstMouth }, { To, SecondMouth },
                            { FirstMouth, Bend }, { Bend, SecondMouth } };
                        bool bHeightConflict = false;
                        for (const auto& Road : Proposed)
                            for (const FCanyonGrayboxEdge& Existing : Plan.Edges)
                            {
                                if (Existing.bCave) continue;
                                const FVector P = Road[0], Q = Road[1];
                                const FVector U = Plan.Nodes[Existing.A].Position;
                                const FVector V = Plan.Nodes[Existing.B].Position;
                                const FVector2D D(Q.X - P.X, Q.Y - P.Y), E(V.X - U.X, V.Y - U.Y);
                                // Nearby roads with incompatible elevations also create floor seams.
                                for (float ProbeT : { 0.f, 0.05f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.95f, 1.f })
                                {
                                    const FVector Probe = FMath::Lerp(P, Q, ProbeT);
                                    const float Along = FMath::Clamp(FVector2D::DotProduct(
                                        FVector2D(Probe.X - U.X, Probe.Y - U.Y), E)
                                        / FMath::Max(1.f, E.SizeSquared()), 0.f, 1.f);
                                    const FVector Nearest = FMath::Lerp(U, V, Along);
                                    const float Margin = Existing.Layer == ECanyonRouteLayer::Lower ? 1500.f : 1800.f;
                                    if (FVector::Dist2D(Probe, Nearest) < Margin
                                        && FMath::Abs(Probe.Z - Nearest.Z) > 80.f)
                                        bHeightConflict = true;
                                }
                                const float Denominator = FVector2D::CrossProduct(D, E);
                                if (FMath::Abs(Denominator) < 1.f) continue;
                                const FVector2D Between(U.X - P.X, U.Y - P.Y);
                                const float T = FVector2D::CrossProduct(Between, E) / Denominator;
                                const float S = FVector2D::CrossProduct(Between, D) / Denominator;
                                if (T > 0.01f && T < 0.99f && S > 0.01f && S < 0.99f
                                    && FMath::Abs(FMath::Lerp(P.Z, Q.Z, T) - FMath::Lerp(U.Z, V.Z, S)) > 80.f)
                                    bHeightConflict = true;
                            }
                        if (bHeightConflict) continue;
                        const bool bExistingLoop = Plan.CaveNetworks.ContainsByPredicate(
                            [](const FCanyonCaveNetwork& Network) { return Network.Pattern == ECanyonCavePattern::LongLoop
                                || !Network.DeadEnds.IsEmpty(); });
                        if (Plan.CavePattern == ECanyonCavePattern::LongLoop
                            || Plan.CavePattern == ECanyonCavePattern::LongWindingThrough
                            || (bExistingLoop && Plan.CavePattern == ECanyonCavePattern::ThroughShortcut))
                        {
                            // Separate independent cave networks; overlapping voids can
                            // consume a previously valid floor at different elevations.
                            TArray<TArray<FVector>> ProposedCaves;
                            if (Plan.CavePattern == ECanyonCavePattern::LongLoop)
                                ProposedCaves = MakeLoopPaths(FirstMouth, SecondMouth, NormalCandidate * Sign, InSeed);
                            else if (Plan.CavePattern == ECanyonCavePattern::LongWindingThrough)
                            {
                                const auto Shape = MakeLongCave(FirstMouth, SecondMouth, NormalCandidate * Sign,
                                    InSeed, bBranchPreview || InSeed == 1020);
                                ProposedCaves.Add(Shape.Main);
                                for (const auto& DeadEnd : Shape.DeadEnds) ProposedCaves.Append(DeadEnd.Paths);
                            }
                            else ProposedCaves.Add({ FirstMouth,
                                FMath::Lerp(FirstMouth, SecondMouth, 0.32f) - NormalCandidate * (Sign * 1250.f),
                                FMath::Lerp(FirstMouth, SecondMouth, 0.68f) - NormalCandidate * (Sign * 1450.f), SecondMouth });
                            bool bCaveConflict = false;
                            for (const TArray<FVector>& Path : ProposedCaves)
                                for (int32 Segment = 1; Segment < Path.Num(); ++Segment)
                                    for (int32 Sample = 0; Sample <= 12; ++Sample)
                                    {
                                        const FVector Probe = FMath::Lerp(Path[Segment - 1], Path[Segment], Sample / 12.f);
                                        for (const FCanyonGrayboxEdge& Existing : Plan.Edges)
                                        {
                                            if (!Existing.bCave) continue;
                                            const FVector U = Plan.Nodes[Existing.A].Position;
                                            const FVector V = Plan.Nodes[Existing.B].Position;
                                            const FVector2D Delta(V.X - U.X, V.Y - U.Y);
                                            const float Along = FMath::Clamp(FVector2D::DotProduct(
                                                FVector2D(Probe.X - U.X, Probe.Y - U.Y), Delta)
                                                / FMath::Max(1.f, Delta.SizeSquared()), 0.f, 1.f);
                                            const FVector Nearest = FMath::Lerp(U, V, Along);
                                            if (FVector::Dist2D(Probe, Nearest) < 2200.f
                                                && FMath::Abs(Probe.Z - Nearest.Z) < 2200.f)
                                                bCaveConflict = true;
                                        }
                                    }
                            if (bCaveConflict) continue;
                        }
                    }
                    float Clearance = TNumericLimits<float>::Max();
                    for (float T : { 0.22f, 0.38f, 0.5f, 0.62f, 0.78f })
                    {
                        const FVector Probe = FMath::Lerp(From, To, T)
                            + NormalCandidate * (Sign * (Distance - 1200.f));
                        for (int32 Other = 0; Other < Plan.Edges.Num(); ++Other)
                        {
                            if (Other == Candidate) continue;
                            const FCanyonGrayboxEdge& Neighbor = Plan.Edges[Other];
                            const FVector P = Plan.Nodes[Neighbor.A].Position;
                            const FVector Q = Plan.Nodes[Neighbor.B].Position;
                            const FVector2D Delta(Q.X - P.X, Q.Y - P.Y);
                            const float U = FMath::Clamp(FVector2D::DotProduct(
                                FVector2D(Probe.X - P.X, Probe.Y - P.Y), Delta)
                                / Delta.SizeSquared(), 0.f, 1.f);
                            Clearance = FMath::Min(Clearance,
                                FVector2D::Distance(FVector2D(Probe.X, Probe.Y),
                                    FVector2D(P.X, P.Y) + Delta * U) - Neighbor.HalfWidth);
                        }
                    }
                    const FVector Outer = (From + To) * 0.5f
                        + NormalCandidate * (Sign * (Distance + 4500.f));
                    float UpperClearance = TNumericLimits<float>::Max();
                    for (const FCanyonGrayboxEdge& Neighbor : Plan.Edges)
                    {
                        if (Neighbor.Layer == ECanyonRouteLayer::Lower) continue;
                        const FVector P = Plan.Nodes[Neighbor.A].Position;
                        const FVector Q = Plan.Nodes[Neighbor.B].Position;
                        const FVector2D Delta(Q.X - P.X, Q.Y - P.Y);
                        const float U = FMath::Clamp(FVector2D::DotProduct(
                            FVector2D(Outer.X - P.X, Outer.Y - P.Y), Delta)
                            / Delta.SizeSquared(), 0.f, 1.f);
                        UpperClearance = FMath::Min(UpperClearance,
                            FVector2D::Distance(FVector2D(Outer.X, Outer.Y),
                                FVector2D(P.X, P.Y) + Delta * U));
                    }
                    const float Score = FMath::Min(Clearance, UpperClearance * 0.8f)
                        - Distance * 0.06f + FVector::Dist2D(From, To) * 0.05f
                        - FMath::Max(0.f, Outer.Y * UpperSide) * 8.f
                        - (Candidate == CandidateEdges[0] ? 0.f : 1200.f);
                    if (Score > BestScore)
                    {
                        BestScore = Score;
                        ChosenEdge = Candidate;
                        ChosenSign = static_cast<float>(Sign);
                        ChosenDistance = Distance;
                    }
                }
        }
        if (BestScore == -TNumericLimits<float>::Max()) break;
        const FCanyonGrayboxEdge Core = Plan.Edges[ChosenEdge];
        const FVector A = Plan.Nodes[Core.A].Position;
        const FVector B = Plan.Nodes[Core.B].Position;
        const FVector2D Along(B.X - A.X, B.Y - A.Y);
        const FVector2D Side(-Along.Y, Along.X);
        const FVector2D Normal = Side.GetSafeNormal();
        const FVector Outward(Normal.X * ChosenSign, Normal.Y * ChosenSign, 0.f);
        const FVector MouthA = A + Outward * ChosenDistance;
        const FVector MouthB = B + Outward * ChosenDistance;
        const int32 MouthAIndex = Add(MouthA.X, MouthA.Y, MouthA.Z);
        const int32 MouthBIndex = Add(MouthB.X, MouthB.Y, MouthB.Z);
        Link(Core.A, MouthAIndex);
        Link(Core.B, MouthBIndex);
        const bool bLongCave = Plan.CavePattern != ECanyonCavePattern::ThroughShortcut;
        const FVector SurfaceBend = (MouthA + MouthB) * 0.5f
            + Outward * (bLongCave ? 6700.f : 4500.f);
        const int32 SurfaceBendIndex = Add(SurfaceBend.X, SurfaceBend.Y, SurfaceBend.Z);
        Link(MouthAIndex, SurfaceBendIndex);
        Link(SurfaceBendIndex, MouthBIndex);
        Plan.Edges.RemoveAt(ChosenEdge);
        const float PassDrop = FMath::Min(2000.f,
            500.f + 0.22f * (FVector::Dist2D(A, B) + 1500.f) - FMath::Abs(A.Z - B.Z));
        if (Plan.CavePattern == ECanyonCavePattern::LongWindingThrough)
        {
            const auto Shape = MakeLongCave(MouthA, MouthB, Outward,
                InSeed == 1050 || InSeed == 1051 ? 1001 : InSeed, bBranchPreview || InSeed == 1020);
            TMap<FVector, int32> Points;
            Points.Add(MouthA, MouthAIndex);
            Points.Add(MouthB, MouthBIndex);
            auto PointIndex = [&](const FVector& Point)
            {
                if (const int32* Existing = Points.Find(Point)) return *Existing;
                const int32 Index = Add(Point.X, Point.Y, Point.Z);
                Plan.Nodes[Index].bCaveInterior = true;
                Points.Add(Point, Index);
                return Index;
            };
            auto AddPath = [&](const TArray<FVector>& RawPath)
            {
                TArray<int32> Path;
                for (const FVector& Point : RawPath) Path.Add(PointIndex(Point));
                for (int32 I = 1; I < Path.Num(); ++I)
                {
                    Link(Path[I - 1], Path[I]);
                    Plan.Edges.Last().bCave = true;
                    Plan.Edges.Last().HalfWidth = 480.f;
                }
                return Path;
            };
            Plan.CavePathNodes = AddPath(Shape.Main);
            Plan.CaveMouthNodes = { MouthAIndex, MouthBIndex };
            for (const FDeadEndShape& Addition : Shape.DeadEnds)
            {
                FCanyonDeadEnd DeadEnd;
                DeadEnd.Kind = Addition.Kind;
                for (const auto& Path : Addition.Paths) DeadEnd.Paths.Add(AddPath(Path));
                for (const FVector& End : Addition.Ends) DeadEnd.EndNodes.Add(PointIndex(End));
                if (Addition.Kind != ECanyonDeadEndKind::LongWinding)
                {
                    for (int32 I = 0; I < DeadEnd.EndNodes.Num(); ++I)
                        DeadEnd.Rooms.Add({ DeadEnd.EndNodes[I], Addition.Kind == ECanyonDeadEndKind::ShortAlcove
                            ? 280.f : 350.f + I * 70.f, 440.f });
                }
                Plan.CaveDeadEnds.Add(MoveTemp(DeadEnd));
            }
        }
        else if (Plan.CavePattern == ECanyonCavePattern::LongLoop)
        {
            const auto LoopPaths = MakeLoopPaths(MouthA, MouthB, Outward, InSeed);
            auto Interior = [&](const FVector& Point)
            {
                const int32 Index = Add(Point.X, Point.Y, Point.Z);
                Plan.Nodes[Index].bCaveInterior = true;
                return Index;
            };
            const int32 LoopFork = Interior(LoopPaths[0].Last()), LoopMerge = Interior(LoopPaths[3][0]);
            // Approach both junctions from beyond the loop ends. Stems must
            // not double back across an arm and carve away its floor.
            const int32 Entry = Interior(LoopPaths[0][1]);
            const int32 Exit = Interior(LoopPaths[3][1]);
            Plan.CaveBranches.Add({ MouthAIndex, Entry, LoopFork });
            for (int32 ArmIndex = 1; ArmIndex <= 2; ++ArmIndex)
            {
                TArray<int32> Arm = { LoopFork };
                for (int32 I = 1; I + 1 < LoopPaths[ArmIndex].Num(); ++I)
                    Arm.Add(Interior(LoopPaths[ArmIndex][I]));
                Arm.Add(LoopMerge);
                Plan.CaveBranches.Add(MoveTemp(Arm));
            }
            Plan.CaveBranches.Add({ LoopMerge, Exit, MouthBIndex });
            Plan.CavePathNodes = Plan.CaveBranches[0];
            for (int32 I = 1; I < Plan.CaveBranches[1].Num(); ++I)
                Plan.CavePathNodes.Add(Plan.CaveBranches[1][I]);
            Plan.CavePathNodes.Add(Exit);
            Plan.CavePathNodes.Add(MouthBIndex);
            Plan.CaveMouthNodes = { MouthAIndex, MouthBIndex };
            for (const TArray<int32>& Path : Plan.CaveBranches)
                for (int32 I = 1; I < Path.Num(); ++I)
                {
                    Link(Path[I - 1], Path[I]);
                    Plan.Edges.Last().bCave = true;
                    Plan.Edges.Last().HalfWidth = 480.f;
                }
        }
        else if (Plan.CavePattern == ECanyonCavePattern::ThreeMouthHall)
        {
            FRandomStream EndpointRandom(InSeed ^ 0x4341560A);
            const bool bThirdMouthOpen = EndpointRandom.FRand() < 0.5f;
            Plan.CaveBranchOpen = { true, true, bThirdMouthOpen };
            const FVector Direction = (B - A).GetSafeNormal2D();
            const FVector CoreCenter = (A + B) * 0.5f;
            const int32 CoreCenterIndex = Add(CoreCenter.X, CoreCenter.Y, CoreCenter.Z);
            Link(Core.A, CoreCenterIndex, Core.Layer);
            Link(CoreCenterIndex, Core.B, Core.Layer);
            // The third opening is on the opposite rock face. Its short surface
            // approach meets the restored inner route across the mountain.
            const FVector MouthC = CoreCenter - Outward * 1100.f
                + FVector(0.f, 0.f, bThirdMouthOpen ? 0.f : -2000.f);
            const int32 MouthCIndex = Add(MouthC.X, MouthC.Y, MouthC.Z);
            if (bThirdMouthOpen) Link(CoreCenterIndex, MouthCIndex);
            else Plan.Nodes[MouthCIndex].bCaveInterior = true;
            FVector Passes[] = {
                MouthA - Direction * 6500.f - Outward * 7000.f + FVector(0.f, 0.f, -2200.f),
                MouthB + Direction * 2500.f - Outward * 7000.f + FVector(0.f, 0.f, -PassDrop - 900.f),
                MouthB + Direction * 2500.f - Outward * 5000.f + FVector(0.f, 0.f, -PassDrop - 900.f),
                MouthA - Direction * 1500.f - Outward * 5000.f + FVector(0.f, 0.f, -PassDrop - 300.f),
                MouthA - Direction * 1500.f - Outward * 3000.f + FVector(0.f, 0.f, -PassDrop),
                MouthB - Outward * 3000.f + FVector(0.f, 0.f, -700.f)
            };
            Passes[1].Z = CoreCenter.Z - PassDrop - 900.f;
            Passes[2].Z = Passes[3].Z = CoreCenter.Z - PassDrop - 600.f;
            Passes[4].Z = CoreCenter.Z - PassDrop - 200.f;
            const FVector Hall = (Passes[2] + Passes[3]) * 0.5f;
            const int32 HallIndex = Add(Hall.X, Hall.Y, Hall.Z);
            Plan.Nodes[HallIndex].bCaveInterior = true;
            Plan.CaveHalls.Add({ HallIndex, 550.f, 550.f });
            auto AddInterior = [&](const FVector& Point)
            {
                const int32 Index = Add(Point.X, Point.Y, Point.Z);
                Plan.Nodes[Index].bCaveInterior = true;
                return Index;
            };
            int32 PassNodes[6];
            for (int32 I = 0; I < 6; ++I) PassNodes[I] = AddInterior(Passes[I]);
            const int32 C1 = AddInterior(CoreCenter + Direction * 2500.f
                - Outward * 2500.f + FVector(0.f, 0.f, bThirdMouthOpen ? -650.f : -2000.f));
            // Descend before crossing the long passes, then climb toward the
            // hall. Rock between crossing tunnels keeps their floors intact.
            const int32 C2 = AddInterior(CoreCenter - Direction * 2500.f
                - Outward * 12000.f + FVector(0.f, 0.f, -2800.f));
            const int32 C3 = AddInterior(CoreCenter - Direction * 5500.f
                - Outward * 3500.f + FVector(0.f, 0.f, -4300.f));
            const int32 C4 = AddInterior(CoreCenter - Direction * 3000.f
                + Outward * 1800.f + FVector(0.f, 0.f, Hall.Z - CoreCenter.Z - 2600.f));
            const int32 C5 = AddInterior(CoreCenter - Direction * 3000.f
                + Outward * 5500.f + FVector(0.f, 0.f, Hall.Z - CoreCenter.Z - 1900.f));
            const int32 C6 = AddInterior(Hall + Direction * 5000.f + Outward * 2000.f);
            Plan.CaveMouthNodes = { MouthAIndex, MouthBIndex };
            if (bThirdMouthOpen) Plan.CaveMouthNodes.Add(MouthCIndex);
            Plan.CaveBranches = {
                { MouthAIndex, PassNodes[0], PassNodes[1], PassNodes[2], HallIndex },
                { MouthBIndex, PassNodes[5], PassNodes[4], PassNodes[3], HallIndex },
                { MouthCIndex, C1, C2, C3, C4, C5, C6, HallIndex }
            };
            Plan.CavePathNodes = { MouthAIndex, PassNodes[0], PassNodes[1], PassNodes[2],
                HallIndex, PassNodes[3], PassNodes[4], PassNodes[5], MouthBIndex };
            for (const TArray<int32>& Leg : Plan.CaveBranches)
                for (int32 Segment = 0; Segment + 1 < Leg.Num(); ++Segment)
                {
                    Link(Leg[Segment], Leg[Segment + 1]);
                    Plan.Edges.Last().bCave = true;
                    Plan.Edges.Last().HalfWidth = 480.f;
                }
        }
        else
        {
        Plan.CavePathNodes = { MouthAIndex };
        if (bLongCave)
        {
            // Three separated passes give this tunnel real travel length. The
            // connecting turns sit beyond the ends of the parallel passages,
            // so the entrances do not cut directly into the final pass.
            const FVector Direction = (B - A).GetSafeNormal2D();
            const FVector Points[] = {
                MouthA - Direction * 6500.f - Outward * 7000.f + FVector(0.f, 0.f, -2200.f),
                MouthB + Direction * 2500.f - Outward * 7000.f + FVector(0.f, 0.f, -PassDrop - 900.f),
                MouthB + Direction * 2500.f - Outward * 5000.f + FVector(0.f, 0.f, -PassDrop - 900.f),
                MouthA - Direction * 1500.f - Outward * 5000.f + FVector(0.f, 0.f, -PassDrop + 300.f),
                MouthA - Direction * 1500.f - Outward * 3000.f + FVector(0.f, 0.f, -PassDrop),
                MouthB - Outward * 3000.f + FVector(0.f, 0.f, -700.f)
            };
            for (const FVector& Point : Points)
            {
                const int32 Index = Add(Point.X, Point.Y, Point.Z);
                Plan.Nodes[Index].bCaveInterior = true;
                Plan.CavePathNodes.Add(Index);
            }
        }
        else
        {
            const FVector Curve = -Outward * R.FRandRange(1000.f, 1450.f);
            for (float Fraction : { 0.32f, 0.68f })
            {
                const FVector Point = FMath::Lerp(MouthA, MouthB, Fraction)
                    + Curve * (Fraction < 0.5f ? 1.f : 1.15f);
                const int32 Index = Add(Point.X, Point.Y, Point.Z);
                Plan.Nodes[Index].bCaveInterior = true;
                Plan.CavePathNodes.Add(Index);
            }
        }
        Plan.CavePathNodes.Add(MouthBIndex);
        Plan.CaveMouthNodes = { MouthAIndex, MouthBIndex };
        for (int32 Segment = 0; Segment + 1 < Plan.CavePathNodes.Num(); ++Segment)
        {
            Link(Plan.CavePathNodes[Segment], Plan.CavePathNodes[Segment + 1]);
            Plan.Edges.Last().bCave = true;
            Plan.Edges.Last().HalfWidth = 480.f;
        }
        }
        // Decorate the completed base graph. The same additions work on hall
        // legs and loop arms without changing their mouth/hall/cycle structure.
        if ((Plan.CavePattern == ECanyonCavePattern::ThreeMouthHall
                || Plan.CavePattern == ECanyonCavePattern::LongLoop)
            && InSeed != 1002 && InSeed != 1003 && InSeed != 1010)
        {
            FRandomStream Additions(InSeed ^ (0x4445434F + CaveIndex));
            const bool bComparison = InSeed == 1030 || InSeed == 1040;
            const int32 Wanted = bComparison ? 3 : Additions.FRand() < 0.6f ? Additions.RandRange(1, 3) : 0;
            TArray<FCanyonGrayboxEdge> Sites;
            for (const auto& Edge : Plan.Edges)
                if (Edge.bCave && Plan.Nodes[Edge.A].bCaveInterior && Plan.Nodes[Edge.B].bCaveInterior
                    && (Plan.CavePathNodes.Contains(Edge.A) || Plan.CaveBranches.ContainsByPredicate(
                        [&](const TArray<int32>& Path) { return Path.Contains(Edge.A); }))
                    && FVector::Dist2D(Plan.Nodes[Edge.A].Position, Plan.Nodes[Edge.B].Position) > 4000.f)
                    Sites.Add(Edge);
            for (int32 I = Sites.Num() - 1; I > 0; --I) Sites.Swap(I, Additions.RandRange(0, I));
            TArray<ECanyonDeadEndKind> Kinds = { ECanyonDeadEndKind::ShortAlcove, ECanyonDeadEndKind::LongWinding, ECanyonDeadEndKind::Forked };
            if (!bComparison)
                for (int32 I = 2; I > 0; --I) Kinds.Swap(I, Additions.RandRange(0, I));
            for (int32 AdditionIndex = 0; AdditionIndex < Wanted; ++AdditionIndex)
            {
                bool bPlaced = false;
                for (const auto& Site : Sites)
                {
                    int32 EdgeIndex = Plan.Edges.IndexOfByPredicate([&](const FCanyonGrayboxEdge& Edge)
                        { return Edge.A == Site.A && Edge.B == Site.B && Edge.bCave; });
                    if (EdgeIndex == INDEX_NONE) continue;
                    const FVector P = Plan.Nodes[Site.A].Position, Q = Plan.Nodes[Site.B].Position;
                    const FVector AdditionAlong = (Q - P).GetSafeNormal2D();
                    const FVector AdditionSide(-AdditionAlong.Y, AdditionAlong.X, 0.f);
                    for (float Fraction : { 0.5f, 0.3f, 0.7f })
                    {
                        const FVector Junction = FMath::Lerp(P, Q, Fraction);
                        for (int32 Sign : { -1, 1 })
                        {
                            FDeadEndShape Shape = MakeDeadEnd(Junction, AdditionAlong, AdditionSide * Sign, Kinds[AdditionIndex], Additions);
                            bool bClear = true;
                            for (const auto& Path : Shape.Paths)
                                for (int32 Segment = 1; Segment < Path.Num(); ++Segment)
                                    for (int32 Sample = 0; Sample <= 12; ++Sample)
                                    {
                                        const FVector Probe = FMath::Lerp(Path[Segment - 1], Path[Segment], Sample / 12.f);
                                        for (int32 Other = 0; Other < Plan.Edges.Num(); ++Other)
                                        {
                                            const auto& Edge = Plan.Edges[Other];
                                            if (!Edge.bCave || Other == EdgeIndex) continue;
                                            const FVector U = Plan.Nodes[Edge.A].Position, V = Plan.Nodes[Edge.B].Position;
                                            const FVector2D Delta(V.X - U.X, V.Y - U.Y);
                                            const float T = FMath::Clamp(FVector2D::DotProduct(
                                                FVector2D(Probe.X - U.X, Probe.Y - U.Y), Delta) / Delta.SizeSquared(), 0.f, 1.f);
                                            const FVector Nearest = FMath::Lerp(U, V, T);
                                            const float Margin = FVector::Dist2D(Probe, Junction) < 1000.f ? 1400.f : 2300.f;
                                            if (FVector::Dist2D(Probe, Nearest) < Margin
                                                && FMath::Abs(Probe.Z - Nearest.Z) < 2200.f) bClear = false;
                                        }
                                    }
                            if (!bClear) continue;
                            const int32 Root = Add(Junction.X, Junction.Y, Junction.Z);
                            Plan.Nodes[Root].bCaveInterior = true;
                            Plan.Edges.RemoveAt(EdgeIndex);
                            Link(Site.A, Root); Plan.Edges.Last().bCave = true; Plan.Edges.Last().HalfWidth = Site.HalfWidth;
                            Link(Root, Site.B); Plan.Edges.Last().bCave = true; Plan.Edges.Last().HalfWidth = Site.HalfWidth;
                            auto SplitPath = [&](TArray<int32>& Path)
                            {
                                for (int32 I = 1; I < Path.Num(); ++I)
                                    if ((Path[I - 1] == Site.A && Path[I] == Site.B)
                                        || (Path[I - 1] == Site.B && Path[I] == Site.A))
                                    { Path.Insert(Root, I); break; }
                            };
                            SplitPath(Plan.CavePathNodes);
                            for (auto& Path : Plan.CaveBranches) SplitPath(Path);
                            TMap<FVector, int32> Points;
                            Points.Add(Junction, Root);
                            auto PointIndex = [&](const FVector& Point)
                            {
                                if (const int32* Existing = Points.Find(Point)) return *Existing;
                                const int32 Index = Add(Point.X, Point.Y, Point.Z);
                                Plan.Nodes[Index].bCaveInterior = true; Points.Add(Point, Index); return Index;
                            };
                            FCanyonDeadEnd DeadEnd;
                            DeadEnd.Kind = Shape.Kind;
                            for (const auto& RawPath : Shape.Paths)
                            {
                                TArray<int32> Path;
                                for (const FVector& Point : RawPath) Path.Add(PointIndex(Point));
                                for (int32 I = 1; I < Path.Num(); ++I)
                                { Link(Path[I - 1], Path[I]); Plan.Edges.Last().bCave = true; Plan.Edges.Last().HalfWidth = 480.f; }
                                DeadEnd.Paths.Add(MoveTemp(Path));
                            }
                            for (const FVector& Point : Shape.Ends) DeadEnd.EndNodes.Add(PointIndex(Point));
                            if (Shape.Kind != ECanyonDeadEndKind::LongWinding)
                                for (int32 I = 0; I < DeadEnd.EndNodes.Num(); ++I)
                                    DeadEnd.Rooms.Add({ DeadEnd.EndNodes[I], Shape.Kind == ECanyonDeadEndKind::ShortAlcove ? 280.f : 350.f + I * 70.f, 440.f });
                            Plan.CaveDeadEnds.Add(MoveTemp(DeadEnd));
                            bPlaced = true;
                            break;
                        }
                        if (bPlaced) break;
                    }
                    if (bPlaced) break;
                }
            }
        }
        // Elevation is independent of topology. Keep comparison maps unchanged,
        // and reuse the same tunnels/rooms for level and uphill variants.
        const FCanyonGrayboxLayout BeforeElevation = Plan;
        FRandomStream ElevationRandom(InSeed ^ (0x454C4556 + CaveIndex * 7919));
        const float ElevationRoll = ElevationRandom.FRand();
        ECanyonCaveElevation Elevation = ECanyonCaveElevation::Descending;
        if (InSeed == 1050) Elevation = ECanyonCaveElevation::Ascending;
        else if (InSeed == 1051) Elevation = ECanyonCaveElevation::Level;
        else if (CaveIndex > 0 && (InSeed < 1000 || InSeed > 1040)
            && !bHallPreview && !bLoopPreview && !bBranchPreview)
            Elevation = ElevationRoll < 0.3f ? ECanyonCaveElevation::Ascending
                : ElevationRoll < 0.45f ? ECanyonCaveElevation::Level : ECanyonCaveElevation::Descending;
        // The hall's third leg crosses other passages at a different elevation.
        // Retain that separation until an uphill hall has its own crossing plan.
        if (Plan.CavePattern == ECanyonCavePattern::ThreeMouthHall)
            Elevation = ECanyonCaveElevation::Descending;
        if (Elevation != ECanyonCaveElevation::Descending)
        {
            TMap<int32, float> OriginalHeights;
            for (int32 Node = BeforeCave.Nodes.Num(); Node < Plan.Nodes.Num(); ++Node)
                OriginalHeights.Add(Node, Plan.Nodes[Node].Position.Z);
            TSet<int32> CaveNodes;
            TArray<FCanyonGrayboxEdge> ElevationEdges;
            for (const auto& Edge : Plan.Edges)
                if (Edge.bCave && (Edge.A >= BeforeCave.Nodes.Num() || Edge.B >= BeforeCave.Nodes.Num()))
                {
                    CaveNodes.Add(Edge.A);
                    CaveNodes.Add(Edge.B);
                    ElevationEdges.Add(Edge);
                }
            const float EntryZ = Plan.Nodes[MouthAIndex].Position.Z;
            // The uphill exit connects back to the existing surface road by a
            // walkable ramp; its height is capped by that approach's length.
            const float Rise = Elevation == ECanyonCaveElevation::Ascending
                ? FMath::Clamp(B.Z - EntryZ + FVector::Dist2D(B, MouthB) * 0.16f, 0.f, 1200.f) : 0.f;
            TMap<int32, float> FixedHeights;
            FixedHeights.Add(MouthAIndex, EntryZ);
            FixedHeights.Add(MouthBIndex, EntryZ + Rise);
            for (int32 Mouth : Plan.CaveMouthNodes)
                if (!FixedHeights.Contains(Mouth)) FixedHeights.Add(Mouth, EntryZ + Rise * 0.6f);
            for (int32 Node : CaveNodes) Plan.Nodes[Node].Position.Z = EntryZ + Rise * 0.5f;
            // Downhill passages can run under existing roads. At ground level,
            // mirror that excursion into the outer mountain instead.
            for (int32 Node : CaveNodes)
                if (Plan.Nodes[Node].bCaveInterior)
                {
                    FVector& Point = Plan.Nodes[Node].Position;
                    Point -= Outward * (2.f * FVector::DotProduct(Point - MouthA, Outward));
                }
            for (const auto& Fixed : FixedHeights) Plan.Nodes[Fixed.Key].Position.Z = Fixed.Value;
            // Weighted relaxation shares one height at every junction, including
            // both loop arms and halls. Dead ends settle at their root height.
            for (int32 Iteration = 0; Iteration < 512; ++Iteration)
            {
                float MaxChange = 0.f;
                for (int32 Node : CaveNodes)
                {
                    if (FixedHeights.Contains(Node)) continue;
                    double WeightedZ = 0., WeightSum = 0.;
                    for (const auto& Edge : ElevationEdges)
                    {
                        const int32 Other = Edge.A == Node ? Edge.B : Edge.B == Node ? Edge.A : INDEX_NONE;
                        if (Other == INDEX_NONE) continue;
                        const double Weight = 1. / FMath::Max(1., FVector::Dist2D(
                            Plan.Nodes[Node].Position, Plan.Nodes[Other].Position));
                        WeightedZ += Plan.Nodes[Other].Position.Z * Weight;
                        WeightSum += Weight;
                    }
                    if (WeightSum <= 0.) continue;
                    const float Height = WeightedZ / WeightSum;
                    MaxChange = FMath::Max(MaxChange, FMath::Abs(Height - Plan.Nodes[Node].Position.Z));
                    Plan.Nodes[Node].Position.Z = Height;
                }
                if (MaxChange < 0.01f) break;
            }
            // Preserve each optional addition's local height differences relative
            // to its junction, including separation from nearby main passages.
            for (const auto& DeadEnd : Plan.CaveDeadEnds)
            {
                const int32 Root = DeadEnd.Paths[0][0];
                const float Offset = Plan.Nodes[Root].Position.Z - OriginalHeights[Root];
                TSet<int32> Shifted;
                for (const auto& Path : DeadEnd.Paths)
                    for (int32 Node : Path)
                        if (Node != Root && !Shifted.Contains(Node))
                        {
                            Plan.Nodes[Node].Position.Z = OriginalHeights[Node] + Offset;
                            Shifted.Add(Node);
                        }
            }
            const float BypassRise = Plan.Nodes[MouthBIndex].Position.Z - EntryZ;
            Plan.Nodes[SurfaceBendIndex].Position.Z = EntryZ + BypassRise * 0.5f;
            // Wrap around the outside of the uphill mountain. The old triangular
            // bypass crossed the long winding passage in plan, safe only below it.
            const FVector AlongCave = (MouthB - MouthA).GetSafeNormal2D();
            float LeftExtent = 0.f, RightExtent = FVector::Dist2D(MouthA, MouthB), DepthExtent = 0.f;
            for (int32 Node : CaveNodes)
            {
                const FVector Offset = Plan.Nodes[Node].Position - MouthA;
                LeftExtent = FMath::Min(LeftExtent, static_cast<float>(FVector::DotProduct(Offset, AlongCave)));
                RightExtent = FMath::Max(RightExtent, static_cast<float>(FVector::DotProduct(Offset, AlongCave)));
                DepthExtent = FMath::Max(DepthExtent, static_cast<float>(FVector::DotProduct(Offset, Outward)));
            }
            const FVector OuterLeft = MouthA + AlongCave * (LeftExtent - 6000.f);
            const FVector OuterRight = MouthA + AlongCave * (RightExtent + 6000.f);
            const float OuterDepth = DepthExtent + 6000.f;
            FVector& OuterCenter = Plan.Nodes[SurfaceBendIndex].Position;
            OuterCenter += Outward * (OuterDepth - FVector::DotProduct(OuterCenter - MouthA, Outward));
            const int32 LeftFoot = Add(OuterLeft.X, OuterLeft.Y, EntryZ);
            const int32 LeftTop = Add(OuterLeft.X + Outward.X * OuterDepth,
                OuterLeft.Y + Outward.Y * OuterDepth, EntryZ + BypassRise * 0.25f);
            const int32 RightTop = Add(OuterRight.X + Outward.X * OuterDepth,
                OuterRight.Y + Outward.Y * OuterDepth, EntryZ + BypassRise * 0.75f);
            const int32 RightFoot = Add(OuterRight.X, OuterRight.Y, EntryZ + BypassRise);
            Plan.Edges.RemoveAll([&](const FCanyonGrayboxEdge& Edge)
                { return !Edge.bCave && (Edge.A == SurfaceBendIndex || Edge.B == SurfaceBendIndex); });
            const TArray<int32> Bypass = { MouthAIndex, LeftFoot, LeftTop,
                SurfaceBendIndex, RightTop, RightFoot, MouthBIndex };
            for (int32 I = 1; I < Bypass.Num(); ++I) Link(Bypass[I - 1], Bypass[I]);
            if (FMath::Abs(BypassRise) > 0.01f)
            {
                for (int32 Node : Bypass)
                    if (Node != MouthAIndex) Plan.Nodes[Node].Layer = ECanyonRouteLayer::CaveAccess;
                for (auto& Edge : Plan.Edges)
                    if (!Edge.bCave && (Edge.A == MouthBIndex || Edge.B == MouthBIndex
                        || Bypass.Contains(Edge.A) && Bypass.Contains(Edge.B)))
                    {
                        Edge.Layer = ECanyonRouteLayer::CaveAccess;
                    }
            }
        }
        auto RegisterNetwork = [&]()
        {
            FCanyonCaveNetwork Network;
            Network.Pattern = Plan.CavePattern;
            Network.Elevation = Elevation;
            Network.MouthNodes = Plan.CaveMouthNodes;
            Network.PathNodes = Plan.CavePathNodes;
            Network.Branches = Plan.CaveBranches;
            Network.BranchOpen = Plan.CaveBranchOpen;
            Network.Halls = Plan.CaveHalls;
            Network.DeadEnds = Plan.CaveDeadEnds;
            Plan.AllCaveMouthNodes.Append(Network.MouthNodes);
            Plan.AllCaveHalls.Append(Network.Halls);
            for (const auto& DeadEnd : Network.DeadEnds) Plan.AllCaveHalls.Append(DeadEnd.Rooms);
            Plan.CaveNetworks.Add(MoveTemp(Network));
            Plan.RebuildCaveSegments();
        };
        RegisterNetwork();
        if (CaveIndex > 0 || Elevation != ECanyonCaveElevation::Descending)
        {
            // Placement must preserve existing surface travel after terrain
            // ownership and room cover are combined, not just avoid crossings.
            auto SurfaceSafe = [&]()
            {
                if (Elevation != ECanyonCaveElevation::Descending)
                    for (const auto& Added : Plan.Edges)
                    {
                        if (Added.A < BeforeCave.Nodes.Num() && Added.B < BeforeCave.Nodes.Num()) continue;
                        const FVector A0 = Plan.Nodes[Added.A].Position, B0 = Plan.Nodes[Added.B].Position;
                        const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(A0, B0) / 350.f));
                        for (const auto& Existing : BeforeCave.Edges)
                        {
                            if (!Existing.bCave) continue;
                            const FVector P = BeforeCave.Nodes[Existing.A].Position;
                            const FVector Q = BeforeCave.Nodes[Existing.B].Position;
                            const FVector2D Start(P.X, P.Y), Delta(Q.X - P.X, Q.Y - P.Y);
                            for (int32 Step = 0; Step <= Steps; ++Step)
                            {
                                const FVector Probe = FMath::Lerp(A0, B0, Step / static_cast<float>(Steps));
                                const float T = FMath::Clamp(FVector2D::DotProduct(
                                    FVector2D(Probe.X, Probe.Y) - Start, Delta) / Delta.SizeSquared(), 0.f, 1.f);
                                const FVector Nearest = FMath::Lerp(P, Q, T);
                                if (FVector::Dist2D(Probe, Nearest) < 2200.f
                                    && FMath::Abs(Probe.Z - Nearest.Z) < 2200.f) return false;
                            }
                        }
                    }
                for (const FCanyonGrayboxEdge& Road : Plan.Edges)
                {
                    if (Road.bCave) continue;
                    const FVector P = Plan.Nodes[Road.A].Position, Q = Plan.Nodes[Road.B].Position;
                    const float Run = FVector::Dist2D(P, Q) / 24.f;
                    float Previous = Plan.SurfaceHeightAt(P.X, P.Y);
                    for (int32 Step = 1; Step <= 24; ++Step)
                    {
                        const FVector Point = FMath::Lerp(P, Q, Step / 24.f);
                        const float Height = Plan.SurfaceHeightAt(Point.X, Point.Y);
                        if (FMath::Abs(Height - Previous) > Run * 0.4f) return false;
                        Previous = Height;
                    }
                }
                return true;
            };
            bool bSurfaceSafe = SurfaceSafe();
            if (!bSurfaceSafe && Elevation != ECanyonCaveElevation::Descending
                && InSeed != 1050 && InSeed != 1051)
            {
                // An unsuitable uphill site keeps its original descending cave,
                // rather than losing a cave type from the map.
                Plan = BeforeElevation;
                Elevation = ECanyonCaveElevation::Descending;
                RegisterNetwork();
                bSurfaceSafe = SurfaceSafe();
            }
            if (!bSurfaceSafe)
            {
                Plan = BeforeCave;
                break;
            }
        }
    }
    if (!Plan.CaveNetworks.IsEmpty())
    {
        const FCanyonCaveNetwork& Primary = Plan.CaveNetworks[0];
        Plan.CavePattern = Primary.Pattern;
        Plan.CaveMouthNodes = Primary.MouthNodes;
        Plan.CavePathNodes = Primary.PathNodes;
        Plan.CaveBranches = Primary.Branches;
        Plan.CaveBranchOpen = Primary.BranchOpen;
        Plan.CaveHalls = Primary.Halls;
        Plan.CaveDeadEnds = Primary.DeadEnds;
    }
    int32 NarrowCount = 0;
    for (const FCanyonGrayboxEdge& Edge : Plan.Edges)
        NarrowCount += Edge.Layer == ECanyonRouteLayer::Lower && !Edge.bCave && Edge.HalfWidth <= 170.f;
    for (FCanyonGrayboxEdge& Edge : Plan.Edges)
        if (NarrowCount < 2 && Edge.Layer == ECanyonRouteLayer::Lower && !Edge.bCave
            && Edge.HalfWidth > 170.f)
        {
            Edge.HalfWidth = 170.f;
            ++NarrowCount;
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
        Node.Position.Z *= Scale;
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
    Plan.RebuildCaveSegments();
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

const TCHAR* FCanyonGrayboxLayout::CaveName(int32 NetworkIndex) const
{
    switch (CaveNetworks.IsValidIndex(NetworkIndex) ? CaveNetworks[NetworkIndex].Pattern : CavePattern)
    {
    case ECanyonCavePattern::ThroughShortcut: return TEXT("ThroughShortcut");
    case ECanyonCavePattern::LongWindingThrough: return TEXT("LongWindingThrough");
    case ECanyonCavePattern::ThreeMouthHall: return TEXT("ThreeMouthHall");
    case ECanyonCavePattern::LongLoop: return TEXT("LongLoop");
    case ECanyonCavePattern::BranchedThrough: return TEXT("BranchedThrough");
    case ECanyonCavePattern::PillarChamber: return TEXT("PillarChamber");
    case ECanyonCavePattern::FissureHall: return TEXT("FissureHall");
    default: return TEXT("TreasureAlcove");
    }
}

void FCanyonGrayboxLayout::ScaleForMap(float Scale)
{
    // Fill the same 125.4 m square used by Beach while reserving space for
    // corridor banks and landmark silhouettes, regardless of graph rotation.
    float MaxCoordinate = 1.f;
    for (const FCanyonGrayboxNode& Node : Nodes)
        MaxCoordinate = FMath::Max(MaxCoordinate,
            FMath::Max(FMath::Abs(Node.Position.X), FMath::Abs(Node.Position.Y)));
    LengthScale = (6270.f - 1000.f) / MaxCoordinate
        * FMath::Sqrt(FMath::Clamp(Scale, 0.5f, 5.f));
    for (FCanyonGrayboxNode& Node : Nodes) Node.Position *= LengthScale;
    for (FCanyonGrayboxEdge& Edge : Edges)
        Edge.HalfWidth = Edge.Layer == ECanyonRouteLayer::Lower
            ? Edge.HalfWidth <= 170.f ? 170.f : FMath::Max(260.f, Edge.HalfWidth * LengthScale)
            : FMath::Max(300.f, Edge.HalfWidth * LengthScale);
    RebuildCaveSegments();
}

void FCanyonGrayboxLayout::RebuildCaveSegments()
{
    CaveSegments.Reset();
    CaveSegmentGroups = 0;
    for (int32 NetworkIndex = 0; NetworkIndex < CaveNetworks.Num(); ++NetworkIndex)
    {
        const auto& Network = CaveNetworks[NetworkIndex];
        TArray<TArray<int32>> Paths = Network.Branches;
        if (Paths.IsEmpty()) Paths.Add(Network.PathNodes);
        for (const auto& DeadEnd : Network.DeadEnds) Paths.Append(DeadEnd.Paths);
        for (const auto& Path : Paths)
        {
            float Total = 0.f, Along = 0.f;
            for (int32 I = 1; I < Path.Num(); ++I)
                Total += FVector::Dist2D(Nodes[Path[I - 1]].Position, Nodes[Path[I]].Position);
            for (int32 I = 1; I < Path.Num(); ++I)
            {
                const FVector A = Nodes[Path[I - 1]].Position, B = Nodes[Path[I]].Position;
                const float Length = FVector::Dist2D(A, B), T = (Along + Length * 0.5f) / Total;
                const bool bHall = Network.Pattern == ECanyonCavePattern::ThreeMouthHall;
                CaveSegments.Add({ A, B, bHall ? 275.f : CaveHalfWidth(T, NetworkIndex),
                    bHall ? 430.f : CaveClearance(T, NetworkIndex), CaveSegmentGroups, false,
                    Network.MouthNodes.Contains(Path[I - 1]), Network.MouthNodes.Contains(Path[I]),
                    Network.Elevation != ECanyonCaveElevation::Descending });
                Along += Length;
            }
            ++CaveSegmentGroups;
        }
        for (const auto& Edge : Edges)
            if (!Edge.bCave && (Network.MouthNodes.Contains(Edge.A) || Network.MouthNodes.Contains(Edge.B)))
                CaveSegments.Add({ Nodes[Edge.A].Position, Nodes[Edge.B].Position,
                    CaveHalfWidth(0.f, NetworkIndex), CaveClearance(0.f, NetworkIndex),
                    CaveSegmentGroups++, true, false, false });
    }
    RebuildSurfaceGuides();
}

void FCanyonGrayboxLayout::RebuildSurfaceGuides()
{
    SurfaceGuides.Reset();
    SurfaceNodeHeights.Reset();
    bBuildingSurfaceGuides = true;
    SurfaceNodeHeights.SetNum(Nodes.Num());
    auto WithoutDetail = [&](float X, float Y)
    {
        return 10.f + SurfaceHeightAt(X, Y) - FMath::Sin(X * 0.0023f + Seed * 0.03f)
            * FMath::Cos(Y * 0.0027f - Seed * 0.04f) * 6.f;
    };
    for (int32 Node = 0; Node < Nodes.Num(); ++Node)
        SurfaceNodeHeights[Node] = WithoutDetail(Nodes[Node].Position.X, Nodes[Node].Position.Y);
    SurfaceGuides.SetNum(Edges.Num());
    for (int32 Index = 0; Index < Edges.Num(); ++Index)
    {
        const auto& Edge = Edges[Index];
        if (Edge.bCave) continue;
        const FVector A = Nodes[Edge.A].Position, B = Nodes[Edge.B].Position;
        const int32 Steps = FMath::Max(2, FMath::CeilToInt(FVector::Dist2D(A, B) / 50.f));
        auto& Heights = SurfaceGuides[Index].Heights;
        Heights.SetNum(Steps + 1);
        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            const FVector Point = FMath::Lerp(A, B, Step / static_cast<float>(Steps));
            Heights[Step] = WithoutDetail(Point.X, Point.Y);
        }
    }
    // A mountain may lift a road above a cave. Spread that rise into walkable
    // approaches, sharing the same elevation at every exterior junction.
    constexpr float Grade = 0.30f;
    for (int32 Iteration = 0; Iteration < 64; ++Iteration)
    {
        float Change = 0.f;
        for (int32 Index = 0; Index < Edges.Num(); ++Index)
        {
            const auto& Edge = Edges[Index];
            auto& Heights = SurfaceGuides[Index].Heights;
            if (Heights.IsEmpty()) continue;
            const float RisePerStep = Grade * FVector::Dist2D(Nodes[Edge.A].Position, Nodes[Edge.B].Position)
                / (Heights.Num() - 1);
            auto Raise = [&](float& Height, float Target)
            {
                Change = FMath::Max(Change, Target - Height);
                Height = FMath::Max(Height, Target);
            };
            Raise(Heights[0], SurfaceNodeHeights[Edge.A]);
            Raise(Heights.Last(), SurfaceNodeHeights[Edge.B]);
            for (int32 Step = 1; Step < Heights.Num(); ++Step) Raise(Heights[Step], Heights[Step - 1] - RisePerStep);
            for (int32 Step = Heights.Num() - 2; Step >= 0; --Step) Raise(Heights[Step], Heights[Step + 1] - RisePerStep);
            Raise(SurfaceNodeHeights[Edge.A], Heights[0]);
            Raise(SurfaceNodeHeights[Edge.B], Heights.Last());
        }
        if (Change < 0.01f) break;
    }
    bBuildingSurfaceGuides = false;
}

void FCanyonGrayboxLayout::SampleCaveTunnels(float X, float Y, float Margin, float SurfaceZ,
    TArray<FCanyonSolidTunnel, TInlineAllocator<4>>& Tunnels) const
{
    struct FNearest { float Distance = TNumericLimits<float>::Max(); FCanyonSolidTunnel Tunnel; };
    TArray<FNearest, TInlineAllocator<12>> Nearest;
    Nearest.SetNum(CaveSegmentGroups);
    const FVector2D Point(X, Y);
    const bool bProtected = CavePortalWeight(X, Y) >= 0.9f;
    for (const auto& Segment : CaveSegments)
    {
        if (Segment.bExterior && bProtected) continue;
        const FVector2D Start(Segment.A.X, Segment.A.Y), Delta(Segment.B.X - Segment.A.X, Segment.B.Y - Segment.A.Y);
        const float T = FMath::Clamp(FVector2D::DotProduct(Point - Start, Delta) / Delta.SizeSquared(), 0.f, 1.f);
        const float Distance = FVector2D::Distance(Point, Start + Delta * T);
        if (Distance > Segment.Width + Margin || Distance >= Nearest[Segment.Group].Distance) continue;
        Nearest[Segment.Group].Distance = Distance;
        Nearest[Segment.Group].Tunnel = { static_cast<float>(600.f + FMath::Lerp(Segment.A.Z, Segment.B.Z, T)),
            Distance, Segment.Width, Segment.Clearance };
    }
    Tunnels.Reset();
    for (const auto& Group : Nearest)
        if (Group.Distance < TNumericLimits<float>::Max()) Tunnels.Add(Group.Tunnel);
    for (const auto& Hall : AllCaveHalls)
    {
        const FVector Center = Nodes[Hall.Node].Position;
        const float Distance = FVector2D::Distance(Point, FVector2D(Center.X, Center.Y));
        if (Distance <= Hall.Radius + Margin)
            Tunnels.Add({ static_cast<float>(600.f + Center.Z), Distance, Hall.Radius, Hall.Clearance });
    }
    if (bProtected)
        for (auto& Tunnel : Tunnels)
            Tunnel.Clearance = FMath::Max(0.f, FMath::Min(Tunnel.Clearance,
                SurfaceZ - Tunnel.FloorZ - FMath::Max(200.f, 90.f * LengthScale + 10.f)));
}

float FCanyonGrayboxLayout::RequiredCaveCover(float X, float Y, float SurfaceZ) const
{
    // Rock thickness exceeds three vertical mesh cells, even at the largest map
    // scale. Protect the tunnel's full width, with side thickness and a smooth bank.
    const float Thickness = FMath::Max(200.f, 90.f * LengthScale + 10.f);
    constexpr float Feather = 450.f;
    const FVector2D Point(X, Y);
    const float Opening = FMath::Min(1.f, CavePortalWeight(X, Y) / 0.9f);
    for (const auto& Segment : CaveSegments)
    {
        if (Segment.bExterior) continue;
        const FVector2D Start(Segment.A.X, Segment.A.Y), Delta(Segment.B.X - Segment.A.X, Segment.B.Y - Segment.A.Y);
        const float RawT = FVector2D::DotProduct(Point - Start, Delta) / Delta.SizeSquared();
        if (Segment.bMountainCover && (Segment.bPortalA && RawT < 0.f || Segment.bPortalB && RawT > 1.f)) continue;
        const float T = FMath::Clamp(RawT, 0.f, 1.f);
        const float Distance = FVector2D::Distance(Point, Start + Delta * T);
        const float BankFeather = Segment.bMountainCover ? Feather : 1200.f;
        const float Bank = FMath::Clamp((Segment.Width + Thickness + BankFeather - Distance) / BankFeather, 0.f, 1.f);
        if (Bank <= 0.f) continue;
        const float Weight = Bank * Bank * (3.f - 2.f * Bank) * Opening;
        const float Headroom = Segment.bMountainCover ? Segment.Clearance : FMath::Min(Segment.Clearance, 260.f);
        const float Roof = 600.f + FMath::Lerp(Segment.A.Z, Segment.B.Z, T) + Headroom + Thickness;
        const float Base = 600.f + FMath::Min(0.f, FMath::Min(Segment.A.Z, Segment.B.Z));
        SurfaceZ = FMath::Max(SurfaceZ, FMath::Lerp(Base, Roof, Weight));
    }
    for (const auto& Hall : AllCaveHalls)
    {
        const FVector Center = Nodes[Hall.Node].Position;
        const float Distance = FVector2D::Distance(Point, FVector2D(Center.X, Center.Y));
        const float Weight = FMath::Clamp((Hall.Radius + Thickness + Feather - Distance) / Feather, 0.f, 1.f);
        const float Roof = 600.f + Center.Z + Hall.Clearance + Thickness;
        const float Base = 600.f + FMath::Min(0.f, Center.Z);
        SurfaceZ = FMath::Max(SurfaceZ, FMath::Lerp(Base, Roof,
            Weight * Weight * (3.f - 2.f * Weight) * Opening));
    }
    return SurfaceZ;
}

float FCanyonGrayboxLayout::CavePortalWeight(float X, float Y) const
{
    const FVector2D Point(X, Y);
    float Weight = 1.f;
    for (int32 Mouth : AllCaveMouthNodes)
    {
        const FVector Center = Nodes[Mouth].Position;
        const float Distance = FVector2D::Distance(Point, FVector2D(Center.X, Center.Y));
        float Radius = 700.f;
        for (const auto& Segment : CaveSegments)
            if ((Segment.bPortalA && FVector::DistSquared(Segment.A, Center) < 1.f)
                || (Segment.bPortalB && FVector::DistSquared(Segment.B, Center) < 1.f))
                Radius = FMath::Max(Radius, Segment.Width + Segment.Clearance
                    + (Segment.bMountainCover ? 1800.f : 250.f));
        const float T = FMath::Clamp(Distance / Radius, 0.f, 1.f);
        Weight = FMath::Min(Weight, T);
    }
    return Weight;
}

bool FCanyonGrayboxLayout::Validate() const
{
    if (!Nodes.IsValidIndex(SpawnNode) || !Nodes.IsValidIndex(TreasureNode) || Nodes.Num() < 6) return false;
    if (CaveNetworks.IsEmpty()) return false;
    for (const FCanyonCaveNetwork& Network : CaveNetworks)
    {
        if (Network.MouthNodes.Num() < 2 || Network.PathNodes.Num() < 4
            || Network.PathNodes[0] != Network.MouthNodes[0]
            || Network.PathNodes.Last() != Network.MouthNodes[1]) return false;
        for (const int32 Mouth : Network.MouthNodes)
            if (!Nodes.IsValidIndex(Mouth) || Nodes[Mouth].bCaveInterior) return false;
        for (const FCanyonDeadEnd& DeadEnd : Network.DeadEnds)
        {
            if (DeadEnd.Paths.IsEmpty() || DeadEnd.EndNodes.IsEmpty()
                || (!Network.PathNodes.Contains(DeadEnd.Paths[0][0])
                    && !Network.Branches.ContainsByPredicate([&](const TArray<int32>& Path)
                        { return Path.Contains(DeadEnd.Paths[0][0]); }))) return false;
            for (const auto& Path : DeadEnd.Paths)
            {
                if (Path.Num() < 2) return false;
                for (int32 Node : Path)
                    if (!Nodes.IsValidIndex(Node) || !Nodes[Node].bCaveInterior) return false;
            }
            for (int32 End : DeadEnd.EndNodes)
            {
                int32 Connections = 0;
                for (const auto& Edge : Edges) Connections += Edge.A == End || Edge.B == End;
                if (Connections != 1) return false;
            }
        }
        if (Network.Pattern == ECanyonCavePattern::BranchedThrough)
        {
            if (Network.Branches.Num() < 3 || Network.Branches.Num() > 5
                || Network.Halls.Num() != Network.Branches.Num() - 1
                || Network.Branches[0] != Network.PathNodes) return false;
            for (int32 I = 1; I < Network.Branches.Num(); ++I)
            {
                const TArray<int32>& Leg = Network.Branches[I];
                if (Leg.Num() != 3 || !Network.PathNodes.Contains(Leg[0])
                    || !Nodes.IsValidIndex(Leg.Last()) || !Nodes[Leg.Last()].bCaveInterior
                    || Leg.Last() != Network.Halls[I - 1].Node) return false;
                int32 Connections = 0;
                for (const FCanyonGrayboxEdge& Edge : Edges)
                    Connections += Edge.A == Leg.Last() || Edge.B == Leg.Last();
                if (Connections != 1) return false;
            }
        }
        if (Network.Pattern == ECanyonCavePattern::LongLoop)
        {
            if (Network.Branches.Num() != 4) return false;
            for (const TArray<int32>& Path : Network.Branches)
                if (Path.Num() < 3) return false;
            if (Network.Branches[0].Last() != Network.Branches[1][0]
                || Network.Branches[1][0] != Network.Branches[2][0]
                || Network.Branches[1].Last() != Network.Branches[2].Last()
                || Network.Branches[1].Last() != Network.Branches[3][0]) return false;
        }
        if (Network.Pattern == ECanyonCavePattern::ThreeMouthHall)
        {
            if (Network.Branches.Num() != 3 || Network.BranchOpen.Num() != 3
                || Network.Halls.Num() != 1) return false;
            for (int32 Branch = 0; Branch < 3; ++Branch)
            {
                if (Network.Branches[Branch].Num() < 3
                    || Network.Branches[Branch].Last() != Network.Halls[0].Node) return false;
                const int32 End = Network.Branches[Branch][0];
                if (!Nodes.IsValidIndex(End)
                    || Network.MouthNodes.Contains(End) != Network.BranchOpen[Branch]) return false;
                bool bSurface = false;
                for (const FCanyonGrayboxEdge& Edge : Edges)
                    bSurface |= !Edge.bCave && (Edge.A == End || Edge.B == End);
                if (bSurface != Network.BranchOpen[Branch]) return false;
            }
        }
    }
    const bool bHall = CavePattern == ECanyonCavePattern::ThreeMouthHall;
    if ((!bHall && CaveMouthNodes.Num() != 2)
        || (bHall && CaveMouthNodes.Num() != 2 && CaveMouthNodes.Num() != 3)
        || CavePathNodes.Num() < 4
        || CavePathNodes[0] != CaveMouthNodes[0] || CavePathNodes.Last() != CaveMouthNodes[1]) return false;
    if (bHall)
    {
        if (CaveHalls.Num() != 1 || CaveBranches.Num() != 3 || CaveBranchOpen.Num() != 3
            || !Nodes.IsValidIndex(CaveHalls[0].Node)) return false;
        int32 OpenCount = 0;
        for (int32 I = 0; I < 3; ++I)
        {
            if (CaveBranches[I].Num() < 3
                || !Nodes.IsValidIndex(CaveBranches[I][0])
                || CaveBranches[I].Last() != CaveHalls[0].Node) return false;
            const int32 Endpoint = CaveBranches[I][0];
            if (CaveBranchOpen[I])
            {
                ++OpenCount;
                if (!CaveMouthNodes.Contains(Endpoint) || Nodes[Endpoint].bCaveInterior) return false;
            }
            else if (CaveMouthNodes.Contains(Endpoint) || !Nodes[Endpoint].bCaveInterior) return false;
            bool bSurfaceConnection = false;
            for (const FCanyonGrayboxEdge& Edge : Edges)
                bSurfaceConnection |= !Edge.bCave && (Edge.A == Endpoint || Edge.B == Endpoint);
            if (bSurfaceConnection != CaveBranchOpen[I]) return false;
        }
        if (OpenCount != CaveMouthNodes.Num() || !CaveBranchOpen[0] || !CaveBranchOpen[1]) return false;
    }
    else if (CavePattern == ECanyonCavePattern::LongLoop)
    {
        if (CaveBranches.Num() != 4 || !CaveHalls.IsEmpty() || !CaveBranchOpen.IsEmpty()) return false;
        if (CaveBranches[0].Last() != CaveBranches[1][0]
            || CaveBranches[1][0] != CaveBranches[2][0]
            || CaveBranches[1].Last() != CaveBranches[2].Last()
            || CaveBranches[1].Last() != CaveBranches[3][0]) return false;
    }
    else if (CavePattern == ECanyonCavePattern::BranchedThrough)
    {
        if (CaveBranches.Num() < 3 || CaveBranches.Num() > 5 || CaveHalls.Num() != CaveBranches.Num() - 1
            || !CaveBranchOpen.IsEmpty() || CaveBranches[0] != CavePathNodes) return false;
        for (int32 I = 1; I < CaveBranches.Num(); ++I)
            if (CaveBranches[I].Num() != 3 || !CavePathNodes.Contains(CaveBranches[I][0])
                || CaveBranches[I].Last() != CaveHalls[I - 1].Node) return false;
    }
    else if (!CaveBranches.IsEmpty() || (!CaveHalls.IsEmpty() && CaveDeadEnds.IsEmpty()) || !CaveBranchOpen.IsEmpty()) return false;
    for (int32 Mouth : CaveMouthNodes)
        if (!Nodes.IsValidIndex(Mouth)) return false;
    for (int32 Interior : CavePathNodes)
        if (!Nodes.IsValidIndex(Interior)) return false;
    TArray<bool> Seen;
    Seen.Init(false, Nodes.Num());
    TArray<int32> Queue = { SpawnNode };
    Seen[SpawnNode] = true;
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
        for (const FCanyonGrayboxEdge& Edge : Edges)
        {
            if (!Nodes.IsValidIndex(Edge.A) || !Nodes.IsValidIndex(Edge.B)) return false;
            const FVector Delta = Nodes[Edge.B].Position - Nodes[Edge.A].Position;
            if (Delta.Size2D() < 900.f * LengthScale
                || FMath::Abs(Delta.Z) / Delta.Size2D()
                    > (Edge.bCave ? 0.25f : 0.18f)) return false;
            const int32 Next = Edge.A == Queue[Head] ? Edge.B : Edge.B == Queue[Head] ? Edge.A : INDEX_NONE;
            if (Next != INDEX_NONE && !Seen[Next]) { Seen[Next] = true; Queue.Add(Next); }
        }
    return Queue.Num() == Nodes.Num();
}

bool FCanyonGrayboxLayout::SampleCave(float T, FVector& Position, FVector& Tangent) const
{
    if (CavePathNodes.Num() < 2) return false;
    float Total = 0.f;
    for (int32 I = 1; I < CavePathNodes.Num(); ++I)
        Total += FVector::Dist2D(Nodes[CavePathNodes[I - 1]].Position, Nodes[CavePathNodes[I]].Position);
    float Target = FMath::Clamp(T, 0.f, 1.f) * Total;
    for (int32 I = 1; I < CavePathNodes.Num(); ++I)
    {
        const FVector A = Nodes[CavePathNodes[I - 1]].Position;
        const FVector B = Nodes[CavePathNodes[I]].Position;
        const float Length = FVector::Dist2D(A, B);
        if (Target <= Length || I == CavePathNodes.Num() - 1)
        {
            Position = FMath::Lerp(A, B, FMath::Clamp(Target / Length, 0.f, 1.f));
            Tangent = (B - A).GetSafeNormal2D();
            return true;
        }
        Target -= Length;
    }
    return false;
}

bool FCanyonGrayboxLayout::SampleCaveBranch(int32 Branch, float T,
    FVector& Position, FVector& Tangent) const
{
    if (!CaveBranches.IsValidIndex(Branch)) return false;
    const TArray<int32>& Path = CaveBranches[Branch];
    float Total = 0.f;
    for (int32 I = 1; I < Path.Num(); ++I)
        Total += FVector::Dist2D(Nodes[Path[I - 1]].Position, Nodes[Path[I]].Position);
    float Target = FMath::Clamp(T, 0.f, 1.f) * Total;
    for (int32 I = 1; I < Path.Num(); ++I)
    {
        const FVector A = Nodes[Path[I - 1]].Position;
        const FVector B = Nodes[Path[I]].Position;
        const float Length = FVector::Dist2D(A, B);
        if (Target <= Length || I == Path.Num() - 1)
        {
            Position = FMath::Lerp(A, B, FMath::Clamp(Target / Length, 0.f, 1.f));
            Tangent = (B - A).GetSafeNormal2D();
            return true;
        }
        Target -= Length;
    }
    return false;
}

bool FCanyonGrayboxLayout::ProjectCave(float X, float Y, float& T, float& Lateral, float& FloorZ,
    int32* NetworkIndex) const
{
    const FVector2D Point(X, Y);
    float BestDistance = TNumericLimits<float>::Max();
    auto ScanPath = [&](const TArray<int32>& Path, int32 Network)
    {
        float Total = 0.f;
        for (int32 I = 1; I < Path.Num(); ++I)
            Total += FVector::Dist2D(Nodes[Path[I - 1]].Position, Nodes[Path[I]].Position);
        float Along = 0.f;
        for (int32 I = 1; I < Path.Num(); ++I)
        {
            const FVector A = Nodes[Path[I - 1]].Position, B = Nodes[Path[I]].Position;
            const FVector2D Start(A.X, A.Y), Delta(B.X - A.X, B.Y - A.Y);
            const float Length = Delta.Size();
            const float Fraction = FMath::Clamp(FVector2D::DotProduct(Point - Start, Delta)
                / Delta.SizeSquared(), 0.f, 1.f);
            const FVector2D Difference = Point - (Start + Delta * Fraction);
            if (Difference.SizeSquared() < BestDistance)
            {
                BestDistance = Difference.SizeSquared();
                T = (Along + Length * Fraction) / Total;
                const float Side = FVector2D::CrossProduct(Delta / Length, Difference);
                Lateral = (Side >= 0.f ? 1.f : -1.f) * FMath::Sqrt(BestDistance);
                FloorZ = FMath::Lerp(A.Z, B.Z, Fraction);
                if (NetworkIndex) *NetworkIndex = Network;
            }
            Along += Length;
        }
    };
    for (int32 Network = 0; Network < CaveNetworks.Num(); ++Network)
    {
        const FCanyonCaveNetwork& Cave = CaveNetworks[Network];
        if (Cave.Branches.IsEmpty()) ScanPath(Cave.PathNodes, Network);
        else for (const TArray<int32>& Branch : Cave.Branches) ScanPath(Branch, Network);
        for (const FCanyonDeadEnd& DeadEnd : Cave.DeadEnds)
            for (const auto& Path : DeadEnd.Paths) ScanPath(Path, Network);
    }
    return BestDistance < TNumericLimits<float>::Max();
}

float FCanyonGrayboxLayout::CaveHalfWidth(float T, int32 NetworkIndex) const
{
    const ECanyonCavePattern Pattern = CaveNetworks.IsValidIndex(NetworkIndex)
        ? CaveNetworks[NetworkIndex].Pattern : CavePattern;
    if (Pattern == ECanyonCavePattern::ThreeMouthHall)
        return 235.f + 20.f * FMath::Square(FMath::Sin(3.f * PI * T + Seed * 0.1f));
    if (Pattern == ECanyonCavePattern::LongWindingThrough || Pattern == ECanyonCavePattern::LongLoop
        || Pattern == ECanyonCavePattern::BranchedThrough)
    {
        const float WiderSections = FMath::Square(FMath::Sin(2.f * PI * T));
        return 205.f + 70.f * WiderSections
            + 12.f * FMath::Square(FMath::Sin(7.f * PI * T + Seed * 0.13f));
    }
    const float Bulge = FMath::Square(FMath::Sin(PI * T));
    return 235.f + 105.f * Bulge + 18.f * FMath::Square(FMath::Sin(5.f * PI * T + Seed * 0.13f));
}

float FCanyonGrayboxLayout::CaveClearance(float T, int32 NetworkIndex) const
{
    const ECanyonCavePattern Pattern = CaveNetworks.IsValidIndex(NetworkIndex)
        ? CaveNetworks[NetworkIndex].Pattern : CavePattern;
    if (Pattern == ECanyonCavePattern::ThreeMouthHall)
        return 390.f + 25.f * FMath::Square(FMath::Sin(3.f * PI * T + Seed * 0.07f));
    if (Pattern == ECanyonCavePattern::LongWindingThrough || Pattern == ECanyonCavePattern::LongLoop
        || Pattern == ECanyonCavePattern::BranchedThrough)
        return 385.f + 90.f * FMath::Square(FMath::Sin(2.f * PI * T))
            + 20.f * FMath::Square(FMath::Sin(6.f * PI * T + Seed * 0.07f));
    return 370.f + 75.f * FMath::Square(FMath::Sin(PI * T))
        + 22.f * FMath::Square(FMath::Sin(4.f * PI * T + Seed * 0.07f));
}

bool FCanyonGrayboxLayout::IsCaveVoid(float X, float Y) const
{
    for (const FCanyonCaveHall& Hall : AllCaveHalls)
        if (Nodes.IsValidIndex(Hall.Node)
            && FVector2D::Distance(FVector2D(X, Y),
                FVector2D(Nodes[Hall.Node].Position.X, Nodes[Hall.Node].Position.Y))
                < Hall.Radius) return true;
    float T = 0.f, Lateral = 0.f, Floor = 0.f;
    int32 Network = INDEX_NONE;
    return ProjectCave(X, Y, T, Lateral, Floor, &Network)
        && T > 0.001f && T < 0.999f
        && FMath::Abs(Lateral) < CaveHalfWidth(T, Network) + 80.f;
}

float FCanyonGrayboxLayout::SurfaceHeightAt(float X, float Y, float* DistanceFromRoute,
    ECanyonRouteLayer* SurfaceLayer) const
{
    const FVector2D Point(X, Y);
    float LayerHeight[4] = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(),
        TNumericLimits<float>::Max(), TNumericLimits<float>::Max() };
    float LayerDistance[4] = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(),
        TNumericLimits<float>::Max(), TNumericLimits<float>::Max() };
    float GuideHeight[4] = { -TNumericLimits<float>::Max(), -TNumericLimits<float>::Max(),
        -TNumericLimits<float>::Max(), -TNumericLimits<float>::Max() };
    float CaveT = 0.f, CaveLateral = 0.f, CaveFloor = 0.f;
    int32 CaveNetwork = INDEX_NONE;
    const bool bNearCave = ProjectCave(X, Y, CaveT, CaveLateral, CaveFloor, &CaveNetwork);
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
    for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); ++EdgeIndex)
    {
        const FCanyonGrayboxEdge& Edge = Edges[EdgeIndex];
        if (Edge.bCave) continue;
        const FVector& A = Nodes[Edge.A].Position;
        const FVector& B = Nodes[Edge.B].Position;
        const FVector2D Start(A.X, A.Y), Delta(B.X - A.X, B.Y - A.Y);
        const float T = FMath::Clamp(FVector2D::DotProduct(Point - Start, Delta) / Delta.SizeSquared(), 0.f, 1.f);
        const float Distance = FVector2D::Distance(Point, Start + Delta * T);
        float FloorZ = FMath::Lerp(A.Z, B.Z, T);
        if (!bBuildingSurfaceGuides && SurfaceGuides.IsValidIndex(EdgeIndex)
            && !SurfaceGuides[EdgeIndex].Heights.IsEmpty())
        {
            const auto& Heights = SurfaceGuides[EdgeIndex].Heights;
            const float Sample = T * (Heights.Num() - 1);
            const int32 Left = FMath::Min(FMath::FloorToInt(Sample), Heights.Num() - 2);
            FloorZ = FMath::Lerp(Heights[Left], Heights[Left + 1], Sample - Left) - 600.f;
            const int32 LayerIndex = static_cast<int32>(Edge.Layer);
            GuideHeight[LayerIndex] = FMath::Max(GuideHeight[LayerIndex], FloorZ - Distance * 0.30f);
        }
        const float Rise = Edge.Layer == ECanyonRouteLayer::Upper ? 1200.f * LengthScale
            : Edge.Layer == ECanyonRouteLayer::Ramp ? 2200.f * LengthScale - FloorZ
            : 2200.f * LengthScale;
        Consider(Distance, Edge.HalfWidth, FloorZ, Rise, Edge.Layer);
    }
    for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
    {
        const FCanyonGrayboxNode& Node = Nodes[NodeIndex];
        if (Node.bCaveInterior || Node.Layer == ECanyonRouteLayer::CaveAccess) continue;
        const float Distance = FVector2D::Distance(Point, FVector2D(Node.Position.X, Node.Position.Y));
        Consider(Distance * 0.72f,
            (Node.Layer == ECanyonRouteLayer::Upper ? 480.f : 440.f) * LengthScale,
            !bBuildingSurfaceGuides && SurfaceNodeHeights.IsValidIndex(NodeIndex)
                ? SurfaceNodeHeights[NodeIndex] - 600.f : Node.Position.Z,
            Node.Layer == ECanyonRouteLayer::Upper ? 1200.f * LengthScale
                : Node.Layer == ECanyonRouteLayer::Ramp ? 2200.f * LengthScale - Node.Position.Z
                : 2200.f * LengthScale,
            Node.Layer);
    }
    int32 BestIndex = 0;
    for (int32 Index = 1; Index < 4; ++Index)
        if (LayerHeight[Index] < LayerHeight[BestIndex]) BestIndex = Index;
    const float BestDistance = LayerDistance[BestIndex];
    // Adjacent roads can exchange ownership near a fork. Use one continuous
    // grade envelope in their walkable core instead of switching guide heights.
    if (!bBuildingSurfaceGuides && BestIndex == 0 && BestDistance < 150.f * LengthScale
        && GuideHeight[BestIndex] > -TNumericLimits<float>::Max())
        LayerHeight[BestIndex] = FMath::Max(LayerHeight[BestIndex], GuideHeight[BestIndex]);
    if (DistanceFromRoute) *DistanceFromRoute = BestDistance;
    if (SurfaceLayer) *SurfaceLayer = static_cast<ECanyonRouteLayer>(BestIndex);
    const float Detail = FMath::Sin(X * 0.0023f + Seed * 0.03f) * FMath::Cos(Y * 0.0027f - Seed * 0.04f)
        * (BestDistance < 550.f * LengthScale ? 6.f : 25.f);
    float Height = 600.f + LayerHeight[BestIndex] + Detail;
    if (bNearCave && CaveT > 0.02f && CaveT < 0.98f
        && CaveNetworks[CaveNetwork].Elevation == ECanyonCaveElevation::Descending
        && FMath::Abs(CaveLateral) < CaveHalfWidth(CaveT, CaveNetwork) + 650.f)
    {
        const float Longitudinal = FMath::Clamp((FMath::Min(CaveT, 1.f - CaveT) - 0.02f) / 0.16f, 0.f, 1.f);
        const float EndFade = Longitudinal * Longitudinal * (3.f - 2.f * Longitudinal);
        const float OuterWidth = CaveHalfWidth(CaveT, CaveNetwork) + 650.f;
        const float Radial = FMath::Clamp((OuterWidth - FMath::Abs(CaveLateral)) / 500.f, 0.f, 1.f);
        const float RadialFade = Radial * Radial * (3.f - 2.f * Radial);
        float OtherRouteMargin = TNumericLimits<float>::Max();
        for (const FCanyonGrayboxEdge& Edge : Edges)
        {
            if (Edge.bCave) continue;
            const FVector A = Nodes[Edge.A].Position, B = Nodes[Edge.B].Position;
            const FVector2D Start(A.X, A.Y), Delta(B.X - A.X, B.Y - A.Y);
            const float U = FMath::Clamp(FVector2D::DotProduct(Point - Start, Delta)
                / Delta.SizeSquared(), 0.f, 1.f);
            OtherRouteMargin = FMath::Min(OtherRouteMargin,
                FVector2D::Distance(Point, Start + Delta * U) - Edge.HalfWidth);
        }
        const float Avoidance = FMath::Clamp(OtherRouteMargin / 300.f, 0.f, 1.f);
        Height = FMath::Max(Height, 600.f + CaveFloor + 1120.f * EndFade * RadialFade * Avoidance);
    }
    for (const FCanyonCaveHall& Hall : AllCaveHalls)
    {
        const FVector& Center = Nodes[Hall.Node].Position;
        const float Distance = FVector2D::Distance(Point, FVector2D(Center.X, Center.Y));
        const float Fade = FMath::Clamp((Hall.Radius + 600.f - Distance) / 600.f, 0.f, 1.f);
        const float Smooth = Fade * Fade * (3.f - 2.f * Fade);
        Height = FMath::Max(Height, 600.f + Center.Z + (Hall.Clearance + 180.f) * Smooth);
    }
    return RequiredCaveCover(X, Y, Height);
}

float FCanyonGrayboxLayout::HeightAt(float X, float Y, float* DistanceFromRoute,
    ECanyonRouteLayer* SurfaceLayer) const
{
    for (const FCanyonCaveHall& Hall : AllCaveHalls)
    {
        const FVector& Center = Nodes[Hall.Node].Position;
        const float Distance = FVector2D::Distance(FVector2D(X, Y),
            FVector2D(Center.X, Center.Y));
        if (Distance < Hall.Radius)
        {
            if (DistanceFromRoute) *DistanceFromRoute = Distance;
            if (SurfaceLayer) *SurfaceLayer = ECanyonRouteLayer::Lower;
            return 600.f + Center.Z;
        }
    }
    float T = 0.f, Lateral = 0.f, Floor = 0.f;
    int32 Network = INDEX_NONE;
    if (ProjectCave(X, Y, T, Lateral, Floor, &Network)
        && T > 0.001f && T < 0.999f
        && FMath::Abs(Lateral) <= CaveHalfWidth(T, Network))
    {
        if (DistanceFromRoute) *DistanceFromRoute = FMath::Abs(Lateral);
        if (SurfaceLayer) *SurfaceLayer = ECanyonRouteLayer::Lower;
        return 600.f + Floor + FMath::Sin(X * 0.0023f + Seed * 0.03f)
            * FMath::Cos(Y * 0.0027f - Seed * 0.04f) * 6.f;
    }
    return SurfaceHeightAt(X, Y, DistanceFromRoute, SurfaceLayer);
}
