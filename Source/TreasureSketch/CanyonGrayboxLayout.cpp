#include "CanyonGrayboxLayout.h"

FCanyonGrayboxLayout FCanyonGrayboxLayout::Generate(int32 InSeed, float MapScale)
{
    FCanyonGrayboxLayout Plan;
    Plan.Seed = InSeed;
    // Keep the second cave out of random rounds until its fixed preview is reviewed.
    Plan.CavePattern = InSeed == 1001 && FMath::IsNearlyEqual(MapScale, 1.f)
        ? ECanyonCavePattern::LongWindingThrough
        : ECanyonCavePattern::ThroughShortcut;
    const float Area = FMath::IsFinite(MapScale) ? FMath::Clamp(MapScale, 0.5f, 5.f) : 1.f;
    FRandomStream R(InSeed ^ 0x4C7A21);
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
    if (DetourRoutes.Num() > 0)
    {
        int32 ChosenEdge = CandidateEdges[0];
        float ChosenSign = 1.f, ChosenDistance = 5400.f;
        float BestScore = -TNumericLimits<float>::Max();
        for (int32 Candidate : CandidateEdges)
        {
            const FCanyonGrayboxEdge& Route = Plan.Edges[Candidate];
            const FVector From = Plan.Nodes[Route.A].Position;
            const FVector To = Plan.Nodes[Route.B].Position;
            const FVector AlongCandidate = (To - From).GetSafeNormal2D();
            const FVector NormalCandidate(-AlongCandidate.Y, AlongCandidate.X, 0.f);
            for (float Distance : { 5400.f, 7000.f, 8500.f })
                for (int32 Sign : { -1, 1 })
                {
                    if (Plan.CavePattern == ECanyonCavePattern::LongWindingThrough
                        && Distance < 8500.f) continue;
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
        const bool bLongCave = Plan.CavePattern == ECanyonCavePattern::LongWindingThrough;
        const FVector SurfaceBend = (MouthA + MouthB) * 0.5f
            + Outward * (bLongCave ? 6700.f : 4500.f);
        const int32 SurfaceBendIndex = Add(SurfaceBend.X, SurfaceBend.Y, SurfaceBend.Z);
        Link(MouthAIndex, SurfaceBendIndex);
        Link(SurfaceBendIndex, MouthBIndex);
        Plan.Edges.RemoveAt(ChosenEdge);
        Plan.CavePathNodes = { MouthAIndex };
        if (bLongCave)
        {
            // Three separated passes give this tunnel real travel length. The
            // connecting turns sit beyond the ends of the parallel passages,
            // so the entrances do not cut directly into the final pass.
            const FVector Direction = (B - A).GetSafeNormal2D();
            const FVector Points[] = {
                MouthA - Direction * 6500.f - Outward * 7000.f + FVector(0.f, 0.f, -2200.f),
                MouthB + Direction * 2500.f - Outward * 7000.f + FVector(0.f, 0.f, -2900.f),
                MouthB + Direction * 2500.f - Outward * 5000.f + FVector(0.f, 0.f, -2900.f),
                MouthA - Direction * 1500.f - Outward * 5000.f + FVector(0.f, 0.f, -1700.f),
                MouthA - Direction * 1500.f - Outward * 3000.f + FVector(0.f, 0.f, -2000.f),
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

const TCHAR* FCanyonGrayboxLayout::CaveName() const
{
    switch (CavePattern)
    {
    case ECanyonCavePattern::ThroughShortcut: return TEXT("ThroughShortcut");
    case ECanyonCavePattern::LongWindingThrough: return TEXT("LongWindingThrough");
    case ECanyonCavePattern::ThreeMouthHall: return TEXT("ThreeMouthHall");
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
}

bool FCanyonGrayboxLayout::Validate() const
{
    if (!Nodes.IsValidIndex(SpawnNode) || !Nodes.IsValidIndex(TreasureNode) || Nodes.Num() < 6) return false;
    if (CaveMouthNodes.Num() != 2 || CavePathNodes.Num() < 4
        || CavePathNodes[0] != CaveMouthNodes[0] || CavePathNodes.Last() != CaveMouthNodes[1]) return false;
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

bool FCanyonGrayboxLayout::ProjectCave(float X, float Y, float& T, float& Lateral, float& FloorZ) const
{
    if (CavePathNodes.Num() < 2) return false;
    const FVector2D Point(X, Y);
    float Total = 0.f;
    for (int32 I = 1; I < CavePathNodes.Num(); ++I)
        Total += FVector::Dist2D(Nodes[CavePathNodes[I - 1]].Position, Nodes[CavePathNodes[I]].Position);
    float Along = 0.f, BestDistance = TNumericLimits<float>::Max();
    for (int32 I = 1; I < CavePathNodes.Num(); ++I)
    {
        const FVector A = Nodes[CavePathNodes[I - 1]].Position;
        const FVector B = Nodes[CavePathNodes[I]].Position;
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
        }
        Along += Length;
    }
    return true;
}

float FCanyonGrayboxLayout::CaveHalfWidth(float T) const
{
    if (CavePattern == ECanyonCavePattern::LongWindingThrough)
    {
        const float WiderSections = FMath::Square(FMath::Sin(2.f * PI * T));
        return 205.f + 70.f * WiderSections
            + 12.f * FMath::Square(FMath::Sin(7.f * PI * T + Seed * 0.13f));
    }
    const float Bulge = FMath::Square(FMath::Sin(PI * T));
    return 235.f + 105.f * Bulge + 18.f * FMath::Square(FMath::Sin(5.f * PI * T + Seed * 0.13f));
}

float FCanyonGrayboxLayout::CaveClearance(float T) const
{
    if (CavePattern == ECanyonCavePattern::LongWindingThrough)
        return 385.f + 90.f * FMath::Square(FMath::Sin(2.f * PI * T))
            + 20.f * FMath::Square(FMath::Sin(6.f * PI * T + Seed * 0.07f));
    return 370.f + 75.f * FMath::Square(FMath::Sin(PI * T))
        + 22.f * FMath::Square(FMath::Sin(4.f * PI * T + Seed * 0.07f));
}

bool FCanyonGrayboxLayout::IsCaveVoid(float X, float Y) const
{
    float T = 0.f, Lateral = 0.f, Floor = 0.f;
    return ProjectCave(X, Y, T, Lateral, Floor)
        && T > 0.f && T < 1.f
        && FMath::Abs(Lateral) < CaveHalfWidth(T) + 80.f;
}

float FCanyonGrayboxLayout::SurfaceHeightAt(float X, float Y, float* DistanceFromRoute,
    ECanyonRouteLayer* SurfaceLayer) const
{
    const FVector2D Point(X, Y);
    float LayerHeight[3] = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(),
        TNumericLimits<float>::Max() };
    float LayerDistance[3] = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(),
        TNumericLimits<float>::Max() };
    float CaveT = 0.f, CaveLateral = 0.f, CaveFloor = 0.f;
    const bool bNearCave = ProjectCave(X, Y, CaveT, CaveLateral, CaveFloor);
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
        if (Edge.bCave && (CavePattern == ECanyonCavePattern::LongWindingThrough
            || !bNearCave || (CaveT > 0.05f && CaveT < 0.95f))) continue;
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
        if (Node.bCaveInterior) continue;
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
    float Height = 600.f + LayerHeight[BestIndex] + Detail;
    if (bNearCave && CaveT > 0.02f && CaveT < 0.98f
        && FMath::Abs(CaveLateral) < CaveHalfWidth(CaveT) + 650.f)
    {
        const float Longitudinal = FMath::Clamp((FMath::Min(CaveT, 1.f - CaveT) - 0.02f) / 0.16f, 0.f, 1.f);
        const float EndFade = Longitudinal * Longitudinal * (3.f - 2.f * Longitudinal);
        const float OuterWidth = CaveHalfWidth(CaveT) + 650.f;
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
        Height = FMath::Max(Height,
            600.f + CaveFloor + 1120.f * EndFade * RadialFade * Avoidance);
    }
    return Height;
}

float FCanyonGrayboxLayout::HeightAt(float X, float Y, float* DistanceFromRoute,
    ECanyonRouteLayer* SurfaceLayer) const
{
    float T = 0.f, Lateral = 0.f, Floor = 0.f;
    if (ProjectCave(X, Y, T, Lateral, Floor) && T > 0.001f && T < 0.999f
        && FMath::Abs(Lateral) <= CaveHalfWidth(T))
    {
        if (DistanceFromRoute) *DistanceFromRoute = FMath::Abs(Lateral);
        if (SurfaceLayer) *SurfaceLayer = ECanyonRouteLayer::Lower;
        return 600.f + Floor + FMath::Sin(X * 0.0023f + Seed * 0.03f)
            * FMath::Cos(Y * 0.0027f - Seed * 0.04f) * 6.f;
    }
    return SurfaceHeightAt(X, Y, DistanceFromRoute, SurfaceLayer);
}
