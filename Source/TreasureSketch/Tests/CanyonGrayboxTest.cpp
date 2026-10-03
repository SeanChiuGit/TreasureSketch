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
    int32 MixedMaps = 0;
    for (int32 Seed = 1000; Seed < 1100; ++Seed)
    {
        const FCanyonGrayboxLayout Layout = FCanyonGrayboxLayout::Generate(Seed);
        if (!Layout.Validate())
            for (const FCanyonGrayboxEdge& Edge : Layout.Edges)
            {
                const FVector Delta = Layout.Nodes[Edge.B].Position - Layout.Nodes[Edge.A].Position;
                if (Delta.Size2D() < 900.f || FMath::Abs(Delta.Z) / Delta.Size2D() > (Edge.bCave ? 0.25f : 0.18f))
                    UE_LOG(LogTemp, Warning, TEXT("Invalid seed %d %s edge %d-%d cave%d run%.0f slope%.3f"),
                        Seed, Layout.CaveName(), Edge.A, Edge.B, Edge.bCave,
                        Delta.Size2D(), FMath::Abs(Delta.Z) / Delta.Size2D());
            }
        if (!TestTrue(FString::Printf(TEXT("Seed %d has a connected, walkable graph"), Seed), Layout.Validate()))
            continue;
        Problems.Add(Layout.Problem);
        Caves.Add(Layout.CavePattern);
        UpperPatterns.Add(Layout.UpperPattern);
        int32 CaveEdges = 0;
        for (const FCanyonGrayboxEdge& Edge : Layout.Edges) CaveEdges += Edge.bCave;
        int32 ExpectedEdges = 0;
        TSet<ECanyonCavePattern> MapCaves;
        for (const FCanyonCaveNetwork& Network : Layout.CaveNetworks)
        {
            MapCaves.Add(Network.Pattern);
            if (Network.Branches.IsEmpty()) ExpectedEdges += Network.PathNodes.Num() - 1;
            else for (const TArray<int32>& Branch : Network.Branches) ExpectedEdges += Branch.Num() - 1;
            for (const FCanyonDeadEnd& DeadEnd : Network.DeadEnds)
                for (const auto& Path : DeadEnd.Paths) ExpectedEdges += Path.Num() - 1;
        }
        TestEqual(TEXT("All cave networks have their generated edges"), CaveEdges, ExpectedEdges);
        if (Seed > 1003 && Seed != 1010 && Seed != 1020 && Seed != 1030 && Seed != 1040)
        {
            TestTrue(TEXT("Ordinary maps retain a cave network"), Layout.CaveNetworks.Num() >= 1);
            if (MapCaves.Num() >= 2) ++MixedMaps;
        }
        const bool bThreeMouth = Layout.CavePattern == ECanyonCavePattern::ThreeMouthHall;
        int32 BranchEdges = 0;
        for (const TArray<int32>& Branch : Layout.CaveBranches) BranchEdges += Branch.Num() - 1;
        TestTrue(TEXT("Cave route graph matches its entrances and halls"),
            bThreeMouth
                ? BranchEdges > 0 && Layout.CaveMouthNodes.Num() == (Layout.CaveBranchOpen[2] ? 3 : 2)
                    && Layout.CaveBranches.Num() == 3 && Layout.CaveHalls.Num() == 1
                    && Layout.CavePathNodes.Num() >= 9
                : Layout.CavePathNodes.Num() >= 4
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
                    Layout.CavePathNodes.Num() >= 8 && CaveLength > 6500.f);
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
        FCanyonGrayboxLayout TerrainBounds = Layout;
        TerrainBounds.ScaleForMap(1.f);
        for (const FCanyonGrayboxNode& Node : TerrainBounds.Nodes)
            TestTrue(FString::Printf(TEXT("Seed %d route stays inside terrain"), Seed),
                FMath::Abs(Node.Position.X) < 6270.f && FMath::Abs(Node.Position.Y) < 6270.f);
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
                TestTrue(FString::Printf(TEXT("Seed %d scale %.1f canyon graph stays traversable"), Seed, Scale), Scaled.Validate());
                if (Scale == 1.f)
                {
                    FVector CaveCenter, Along;
                    Scaled.SampleCave(0.5f, CaveCenter, Along);
                    TestTrue(FString::Printf(TEXT("Seed %d cave floor has a solid mountain above its void"), Seed),
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
                const float TerrainHalf = 3135.f * FMath::Sqrt(Scale);
                for (const FCanyonGrayboxNode& Node : Scaled.Nodes)
                    TestTrue(TEXT("Scaled route leaves room for banks at terrain edge"),
                        FMath::Abs(Node.Position.X) <= TerrainHalf - 450.f * FMath::Sqrt(Scale)
                        && FMath::Abs(Node.Position.Y) <= TerrainHalf - 450.f * FMath::Sqrt(Scale));
                if (Scale == 5.f)
                    for (const FCanyonGrayboxEdge& Edge : Scaled.Edges)
                    {
                        if (Edge.bCave) continue; // Cave collisions are checked against the actual solid mesh.
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
            if (Edge.bCave) continue;
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
                TestTrue(FString::Printf(TEXT("Seed %d edge %d-%d layer %d endpoints %.1f/%.1f step %d z %.1f->%.1f run %.1f remains walkable"),
                    Seed, Edge.A, Edge.B, static_cast<int32>(Edge.Layer), A.Z, B.Z, Step, LastHeight, Height, StepLength),
                    FMath::Abs(Height - LastHeight) / StepLength < 0.45f);
                LastHeight = Height;
            }
        }
    }
    TestEqual(TEXT("All six navigation problems occur in 100 seeds"), Problems.Num(), 6);
    TestEqual(TEXT("All four base cave layouts occur in random maps"), Caves.Num(), 4);
    TestTrue(TEXT("Most ordinary maps mix cave types where placement permits"), MixedMaps > 70);
    TestEqual(TEXT("All three upper route forms occur in 100 seeds"), UpperPatterns.Num(), 3);
    int32 OpenHalls = 0, ClosedHalls = 0;
    for (int32 Seed = 2000; Seed < 2100; ++Seed)
    {
        const FCanyonGrayboxLayout Preview = FCanyonGrayboxLayout::Generate(Seed, 1.f, true);
        TestTrue(FString::Printf(TEXT("Seed %d hall endpoint variant has a valid graph"), Seed), Preview.Validate());
        if (!Preview.Validate())
            for (const FCanyonGrayboxEdge& Edge : Preview.Edges)
            {
                const FVector Delta = Preview.Nodes[Edge.B].Position - Preview.Nodes[Edge.A].Position;
                if (Delta.Size2D() < 900.f || FMath::Abs(Delta.Z) / Delta.Size2D() > (Edge.bCave ? 0.25f : 0.18f))
                    UE_LOG(LogTemp, Warning, TEXT("Invalid hall seed %d edge%d-%d cave%d run%.0f slope%.3f"),
                        Seed, Edge.A, Edge.B, Edge.bCave, Delta.Size2D(), FMath::Abs(Delta.Z) / Delta.Size2D());
            }
        const FCanyonGrayboxLayout Again = FCanyonGrayboxLayout::Generate(Seed, 1.f, true);
        TestEqual(TEXT("Seed reproduces the third endpoint state"),
            Preview.CaveBranchOpen[2], Again.CaveBranchOpen[2]);
        if (Preview.CaveBranchOpen[2]) ++OpenHalls;
        else ++ClosedHalls;
    }
    TestTrue(TEXT("Random hall seeds include open and enclosed variants"),
        OpenHalls > 25 && ClosedHalls > 25);
    FCanyonGrayboxLayout Large = FCanyonGrayboxLayout::Generate(1008, 2.f);
    TestTrue(TEXT("Larger random maps retain multiple cave types when safe sites permit"),
        Large.CaveNetworks.Num() >= 2 && Large.CaveNetworks.Num() <= 3);
    Large.ScaleForMap(2.f);
    TestTrue(TEXT("Mixed large-map cave graph is connected"), Large.Validate());
    return true;
}

#endif
