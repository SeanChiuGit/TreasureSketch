#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../CanyonGrayboxLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonGrayboxPlanTest, "TreasureSketch.Canyon.GrayboxRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonGrayboxPlanTest::RunTest(const FString& Parameters)
{
    TSet<ECanyonGrayboxProblem> Problems;
    TSet<ECanyonCavePattern> Caves;
    TSet<ECanyonUpperPattern> UpperPatterns;
    for (int32 Seed = 1000; Seed < 1100; ++Seed)
    {
        const FCanyonGrayboxLayout Layout = FCanyonGrayboxLayout::Generate(Seed);
        if (!TestTrue(FString::Printf(TEXT("Seed %d has a connected, walkable graph"), Seed), Layout.Validate()))
            continue;
        Problems.Add(Layout.Problem);
        Caves.Add(Layout.CavePattern);
        UpperPatterns.Add(Layout.UpperPattern);
        int32 CaveEdges = 0;
        for (const FCanyonGrayboxEdge& Edge : Layout.Edges) CaveEdges += Edge.bCave;
        const bool bThreeMouth = Layout.CavePattern == ECanyonCavePattern::ThreeMouthHall;
        TestTrue(TEXT("Cave route graph matches its entrances and halls"),
            bThreeMouth
                ? CaveEdges == 12 && Layout.CaveMouthNodes.Num() == 3
                    && Layout.CaveBranches.Num() == 3 && Layout.CaveHalls.Num() == 1
                    && Layout.CavePathNodes.Num() == 9
                : CaveEdges == Layout.CavePathNodes.Num() - 1
                    && Layout.CaveMouthNodes.Num() == 2
                    && Layout.CavePathNodes.Num() >= 4);
        if (Seed < 1005 && Layout.CavePathNodes.Num() >= 4)
        {
            const int32 Entrance = Layout.CaveMouthNodes[0], Exit = Layout.CaveMouthNodes[1];
            float CaveLength = 0.f, SurfaceLength = TNumericLimits<float>::Max();
            for (int32 I = 1; I < Layout.CavePathNodes.Num(); ++I)
                CaveLength += FVector::Dist2D(Layout.Nodes[Layout.CavePathNodes[I - 1]].Position,
                    Layout.Nodes[Layout.CavePathNodes[I]].Position);
            for (const FCanyonGrayboxEdge& First : Layout.Edges)
            {
                if (First.bCave || (First.A != Entrance && First.B != Entrance)) continue;
                const int32 Bend = First.A == Entrance ? First.B : First.A;
                for (const FCanyonGrayboxEdge& Second : Layout.Edges)
                    if (!Second.bCave && ((Second.A == Bend && Second.B == Exit)
                        || (Second.B == Bend && Second.A == Exit)))
                        SurfaceLength = FMath::Min(SurfaceLength,
                            FVector::Dist2D(Layout.Nodes[Entrance].Position, Layout.Nodes[Bend].Position)
                            + FVector::Dist2D(Layout.Nodes[Bend].Position, Layout.Nodes[Exit].Position));
            }
            TestTrue(TEXT("A surface road connects the cave mouths"),
                SurfaceLength < TNumericLimits<float>::Max());
            if (Layout.CavePattern == ECanyonCavePattern::ThroughShortcut)
                TestTrue(TEXT("Short cave is a shortcut around the mountain"),
                    SurfaceLength > CaveLength * 1.15f);
            if (Layout.CavePattern == ECanyonCavePattern::LongWindingThrough)
                TestTrue(TEXT("Long cave has six bends and substantial route length"),
                    Layout.CavePathNodes.Num() == 8 && CaveLength > 6500.f);
            if (bThreeMouth)
                for (int32 Branch = 0; Branch < 3; ++Branch)
                {
                    FVector HallPoint, Along;
                    TestTrue(TEXT("Each cave mouth reaches the same hall"),
                        Layout.SampleCaveBranch(Branch, 1.f, HallPoint, Along)
                        && FVector::Dist2D(HallPoint,
                            Layout.Nodes[Layout.CaveHalls[0].Node].Position) < 1.f);
                }
        }
        for (const FCanyonGrayboxNode& Node : Layout.Nodes)
            TestTrue(FString::Printf(TEXT("Seed %d route stays inside terrain"), Seed),
                FMath::Abs(Node.Position.X) < 19000.f && FMath::Abs(Node.Position.Y) < 19000.f);
        TestTrue(TEXT("Graph adds several local route decisions"),
            Layout.Nodes.Num() >= 14 && Layout.Edges.Num() >= 19
            && Layout.Edges.Num() - Layout.Nodes.Num() + 1 >= 2);
        int32 UpperEdges = 0, RampEdges = 0, NarrowEdges = 0;
        for (const FCanyonGrayboxEdge& Edge : Layout.Edges)
        {
            UpperEdges += Edge.Layer == ECanyonRouteLayer::Upper;
            RampEdges += Edge.Layer == ECanyonRouteLayer::Ramp;
            NarrowEdges += Edge.Layer == ECanyonRouteLayer::Lower && Edge.HalfWidth <= 170.f;
        }
        TestTrue(TEXT("Upper route has a playable lookout or traverse"),
            UpperEdges >= 4 && RampEdges ==
                (Layout.UpperPattern == ECanyonUpperPattern::Lookout ? 3 : 6));
        TestTrue(TEXT("At least two passages narrow to three-character width"), NarrowEdges >= 2);
        const FCanyonGrayboxLayout Again = FCanyonGrayboxLayout::Generate(Seed);
        TestEqual(TEXT("Seed reproduces problem"), Layout.Problem, Again.Problem);
        TestEqual(TEXT("Seed reproduces node count"), Layout.Nodes.Num(), Again.Nodes.Num());
        if (Seed < 1003)
            for (float Scale : { 0.5f, 1.f, 2.f, 5.f })
            {
                FCanyonGrayboxLayout Scaled = FCanyonGrayboxLayout::Generate(Seed, Scale);
                Scaled.ScaleForMap(Scale);
                TestTrue(TEXT("Scaled canyon graph stays traversable"), Scaled.Validate());
                if (Scale == 1.f)
                {
                    FVector CaveCenter, Along;
                    Scaled.SampleCave(0.5f, CaveCenter, Along);
                    TestTrue(TEXT("Cave floor has a solid mountain above its void"),
                        Scaled.IsCaveVoid(CaveCenter.X, CaveCenter.Y)
                        && Scaled.SurfaceHeightAt(CaveCenter.X, CaveCenter.Y)
                            - Scaled.HeightAt(CaveCenter.X, CaveCenter.Y) > 380.f);
                    if (Scaled.CavePattern == ECanyonCavePattern::ThreeMouthHall)
                        TestTrue(TEXT("Central hall opens wider and higher than its branches"),
                            Scaled.CaveHalls[0].Radius > Scaled.CaveHalfWidth(0.5f) * 2.f
                            && Scaled.CaveHalls[0].Clearance > Scaled.CaveClearance(0.5f) + 100.f);
                    else
                    {
                        const float WideT = Scaled.CavePattern == ECanyonCavePattern::LongWindingThrough
                            ? 0.25f : 0.5f;
                        TestTrue(TEXT("Tunnel varies in width and height"),
                            Scaled.CaveHalfWidth(WideT) > Scaled.CaveHalfWidth(0.05f) + 45.f
                            && Scaled.CaveClearance(WideT) > Scaled.CaveClearance(0.05f) + 50.f);
                    }
                }
                if (Scale >= 2.f && Layout.CavePattern == Scaled.CavePattern)
                {
                    auto CoreCounts = [](const FCanyonGrayboxLayout& Plan)
                    {
                        int32 Interior = 0, CaveEdges = 0;
                        for (const FCanyonGrayboxNode& Node : Plan.Nodes)
                            Interior += Node.bCaveInterior;
                        for (const FCanyonGrayboxEdge& Edge : Plan.Edges)
                            CaveEdges += Edge.bCave;
                        return FIntPoint(Plan.Nodes.Num() - Interior,
                            Plan.Edges.Num() - CaveEdges);
                    };
                    const FIntPoint Large = CoreCounts(Scaled), Base = CoreCounts(Layout);
                    TestTrue(TEXT("Larger maps add route nodes and choices"),
                        Large.X > Base.X && Large.Y > Base.Y);
                }
                const float TerrainHalf = 6270.f * FMath::Sqrt(Scale);
                for (const FCanyonGrayboxNode& Node : Scaled.Nodes)
                    TestTrue(TEXT("Scaled route leaves room for banks at terrain edge"),
                        FMath::Abs(Node.Position.X) <= TerrainHalf - 900.f * FMath::Sqrt(Scale)
                        && FMath::Abs(Node.Position.Y) <= TerrainHalf - 900.f * FMath::Sqrt(Scale));
                if (Scale == 5.f)
                    for (const FCanyonGrayboxEdge& Edge : Scaled.Edges)
                    {
                        const FVector A = Scaled.Nodes[Edge.A].Position;
                        const FVector B = Scaled.Nodes[Edge.B].Position;
                        auto EdgeHeight = [&](float X, float Y)
                        { return Edge.bCave ? Scaled.HeightAt(X, Y)
                            : Scaled.SurfaceHeightAt(X, Y); };
                        float Previous = EdgeHeight(A.X, A.Y);
                        for (int32 Step = 1; Step <= 12; ++Step)
                        {
                            const FVector Point = FMath::Lerp(A, B, Step / 12.f);
                            const float Height = EdgeHeight(Point.X, Point.Y);
                            TestTrue(FString::Printf(TEXT("Seed %d large-map edge %d-%d layer %d step %d z %.1f->%.1f remains walkable"),
                                Seed, Edge.A, Edge.B, static_cast<int32>(Edge.Layer), Step, Previous, Height),
                                FMath::Abs(Height - Previous) / (FVector::Dist2D(A, B) / 12.f) < 0.45f);
                            Previous = Height;
                        }
                    }
            }
        TestTrue(TEXT("Spawn and treasure are separated"),
            FVector::Dist2D(Layout.Nodes[Layout.SpawnNode].Position,
                Layout.Nodes[Layout.TreasureNode].Position) > 2800.f);
        int32 LandmarkCount = 0;
        for (const FCanyonGrayboxNode& Node : Layout.Nodes)
            LandmarkCount += Node.Landmark != ECanyonGrayboxLandmark::None;
        TestTrue(TEXT("Three or more landmark placeholders"), LandmarkCount >= 3);
        for (const FCanyonGrayboxEdge& Edge : Layout.Edges)
        {
            const FVector& A = Layout.Nodes[Edge.A].Position;
            const FVector& B = Layout.Nodes[Edge.B].Position;
            auto EdgeHeight = [&](float X, float Y)
            { return Edge.bCave ? Layout.HeightAt(X, Y)
                : Layout.SurfaceHeightAt(X, Y); };
            float LastHeight = EdgeHeight(A.X, A.Y);
            const float StepLength = FVector::Dist2D(A, B) / 24.f;
            for (int32 Step = 1; Step <= 24; ++Step)
            {
                const FVector Point = FMath::Lerp(A, B, Step / 24.f);
                const float Height = EdgeHeight(Point.X, Point.Y);
                TestTrue(FString::Printf(TEXT("Seed %d edge %d-%d layer %d step %d z %.1f->%.1f run %.1f remains walkable"),
                    Seed, Edge.A, Edge.B, static_cast<int32>(Edge.Layer), Step, LastHeight, Height, StepLength),
                    FMath::Abs(Height - LastHeight) / StepLength < 0.45f);
                LastHeight = Height;
            }
        }
    }
    TestEqual(TEXT("All six navigation problems occur in 100 seeds"), Problems.Num(), 6);
    TestEqual(TEXT("Two through caves and the fixed three-mouth preview are generated"), Caves.Num(), 3);
    TestEqual(TEXT("All three upper route forms occur in 100 seeds"), UpperPatterns.Num(), 3);
    return true;
}

#endif
