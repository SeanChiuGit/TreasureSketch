#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../CanyonGrayboxLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonGrayboxPlanTest, "TreasureSketch.Canyon.GrayboxRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonGrayboxPlanTest::RunTest(const FString& Parameters)
{
    TSet<ECanyonGrayboxProblem> Problems;
    TSet<ECanyonCavePattern> Caves;
    for (int32 Seed = 1000; Seed < 1100; ++Seed)
    {
        const FCanyonGrayboxLayout Layout = FCanyonGrayboxLayout::Generate(Seed);
        if (!TestTrue(FString::Printf(TEXT("Seed %d has a connected, walkable graph"), Seed), Layout.Validate()))
            continue;
        Problems.Add(Layout.Problem);
        Caves.Add(Layout.CavePattern);
        int32 CaveEdges = 0;
        for (const FCanyonGrayboxEdge& Edge : Layout.Edges) CaveEdges += Edge.bCave;
        TestTrue(TEXT("Cave connects to the walkable graph"), CaveEdges > 0 && Layout.CaveMouthNodes.Num() > 0);
        if (Layout.CavePattern == ECanyonCavePattern::ThreeMouthHall)
            TestTrue(TEXT("Three surface entrances lead to one chamber"),
                Layout.CaveMouthNodes.Num() == 3 && CaveEdges == 3 && Layout.CaveChambers.Num() == 1);
        if (Layout.CavePattern == ECanyonCavePattern::PillarChamber)
            TestTrue(TEXT("Pillar splits and rejoins the cave route"),
                CaveEdges == 4 && Layout.CaveChambers.Num() == 1 && Layout.CaveChambers[0].bPillar);
        if (Layout.CavePattern == ECanyonCavePattern::FissureHall)
            TestTrue(TEXT("Narrow entrance opens into a larger chamber"),
                CaveEdges == 2 && Layout.CaveChambers.Num() == 1 && Layout.CaveChambers[0].Radius > 170.f);
        if (Layout.CavePattern == ECanyonCavePattern::TreasureAlcove)
            TestEqual(TEXT("Treasure sits at the marked cave dead end"),
                Layout.Nodes[Layout.TreasureNode].Landmark, ECanyonGrayboxLandmark::CaveBeacon);
        for (const FCanyonGrayboxNode& Node : Layout.Nodes)
            TestTrue(FString::Printf(TEXT("Seed %d route stays inside terrain"), Seed),
                FMath::Abs(Node.Position.X) < 19000.f && FMath::Abs(Node.Position.Y) < 19000.f);
        TestTrue(TEXT("Graph adds several local route decisions"),
            Layout.Nodes.Num() >= 14 && Layout.Edges.Num() >= 19
            && Layout.Edges.Num() - Layout.Nodes.Num() + 1 >= 3);
        int32 UpperEdges = 0, RampEdges = 0, NarrowEdges = 0;
        for (const FCanyonGrayboxEdge& Edge : Layout.Edges)
        {
            UpperEdges += Edge.Layer == ECanyonRouteLayer::Upper;
            RampEdges += Edge.Layer == ECanyonRouteLayer::Ramp;
            NarrowEdges += Edge.Layer == ECanyonRouteLayer::Lower && Edge.HalfWidth <= 170.f;
        }
        TestTrue(TEXT("Upper route has a loop and two multi-stage access ramps"),
            UpperEdges >= 7 && RampEdges == 6);
        TestTrue(TEXT("At least two passages narrow to three-character width"), NarrowEdges >= 2);
        const FCanyonGrayboxLayout Again = FCanyonGrayboxLayout::Generate(Seed);
        TestEqual(TEXT("Seed reproduces problem"), Layout.Problem, Again.Problem);
        TestEqual(TEXT("Seed reproduces node count"), Layout.Nodes.Num(), Again.Nodes.Num());
        if (Seed < 1003)
            for (float Scale : { 0.5f, 1.f, 2.f, 5.f })
            {
                FCanyonGrayboxLayout Scaled = FCanyonGrayboxLayout::Generate(Seed);
                Scaled.ScaleForMap(Scale);
                TestTrue(TEXT("Scaled canyon graph stays traversable"), Scaled.Validate());
                const float TerrainHalf = 6270.f * FMath::Sqrt(Scale);
                for (const FCanyonGrayboxNode& Node : Scaled.Nodes)
                    TestTrue(TEXT("Scaled route leaves room for banks at terrain edge"),
                        FMath::Abs(Node.Position.X) <= TerrainHalf - 900.f * FMath::Sqrt(Scale)
                        && FMath::Abs(Node.Position.Y) <= TerrainHalf - 900.f * FMath::Sqrt(Scale));
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
            float LastHeight = Layout.HeightAt(A.X, A.Y);
            const float StepLength = FVector::Dist2D(A, B) / 24.f;
            for (int32 Step = 1; Step <= 24; ++Step)
            {
                const FVector Point = FMath::Lerp(A, B, Step / 24.f);
                const float Height = Layout.HeightAt(Point.X, Point.Y);
                TestTrue(FString::Printf(TEXT("Seed %d edge %d-%d layer %d step %d z %.1f->%.1f run %.1f remains walkable"),
                    Seed, Edge.A, Edge.B, static_cast<int32>(Edge.Layer), Step, LastHeight, Height, StepLength),
                    FMath::Abs(Height - LastHeight) / StepLength < 0.45f);
                LastHeight = Height;
            }
        }
    }
    TestEqual(TEXT("All six navigation problems occur in 100 seeds"), Problems.Num(), 6);
    TestEqual(TEXT("All five cave structures occur in 100 seeds"), Caves.Num(), 5);
    return true;
}

#endif
