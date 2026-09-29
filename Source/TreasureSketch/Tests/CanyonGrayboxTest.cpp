#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../CanyonGrayboxLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonGrayboxPlanTest, "TreasureSketch.Canyon.GrayboxRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonGrayboxPlanTest::RunTest(const FString& Parameters)
{
    TSet<ECanyonGrayboxProblem> Problems;
    for (int32 Seed = 1000; Seed < 1100; ++Seed)
    {
        const FCanyonGrayboxLayout Layout = FCanyonGrayboxLayout::Generate(Seed);
        if (!TestTrue(FString::Printf(TEXT("Seed %d has a connected, walkable graph"), Seed), Layout.Validate()))
            continue;
        Problems.Add(Layout.Problem);
        const FCanyonGrayboxLayout Again = FCanyonGrayboxLayout::Generate(Seed);
        TestEqual(TEXT("Seed reproduces problem"), Layout.Problem, Again.Problem);
        TestEqual(TEXT("Seed reproduces node count"), Layout.Nodes.Num(), Again.Nodes.Num());
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
                TestTrue(FString::Printf(TEXT("Seed %d edge %d-%d remains walkable"), Seed, Edge.A, Edge.B),
                    FMath::Abs(Height - LastHeight) / StepLength < 0.45f);
                LastHeight = Height;
            }
        }
    }
    TestEqual(TEXT("All six navigation problems occur in 100 seeds"), Problems.Num(), 6);
    return true;
}

#endif
