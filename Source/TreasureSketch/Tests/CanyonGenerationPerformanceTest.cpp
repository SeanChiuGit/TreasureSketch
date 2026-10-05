#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "../ProceduralIsland.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonGenerationPerformanceTest,
    "TreasureSketch.Canyon.GenerationPerformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonGenerationPerformanceTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts())
        if (Context.World() && Context.WorldType == EWorldType::Editor)
        { World = Context.World(); break; }
    if (!TestNotNull(TEXT("An editor collision world exists"), World)) return false;
    for (int32 Seed : { 1050, 18232 })
    {
        auto* Island = World->SpawnActorDeferred<AProceduralIsland>(
            AProceduralIsland::StaticClass(), FTransform::Identity);
        Island->Theme = EIslandTheme::CanyonGraybox;
        Island->Seed = Seed;
        Island->MapScale = 1.f;
        const double Start = FPlatformTime::Seconds();
        Island->FinishSpawning(FTransform::Identity);
        const double Seconds = FPlatformTime::Seconds() - Start;
        uint32 Hash = 0;
        int32 Vertices = 0, Triangles = 0;
        TArray<UProceduralMeshComponent*> Meshes;
        Island->GetComponents(Meshes);
        for (auto* Mesh : Meshes)
            for (int32 Index = 0; Index < Mesh->GetNumSections(); ++Index)
                if (const auto* Section = Mesh->GetProcMeshSection(Index))
                {
                    if (!Section->ProcIndexBuffer.IsEmpty())
                        TestTrue(TEXT("Every generated mesh section retains collision"), Section->bEnableCollision);
                    Vertices += Section->ProcVertexBuffer.Num();
                    Triangles += Section->ProcIndexBuffer.Num() / 3;
                    for (const auto& Vertex : Section->ProcVertexBuffer)
                    {
                        Hash = FCrc::MemCrc32(&Vertex.Position, sizeof(Vertex.Position), Hash);
                        Hash = FCrc::MemCrc32(&Vertex.Normal, sizeof(Vertex.Normal), Hash);
                        Hash = FCrc::MemCrc32(&Vertex.UV0, sizeof(Vertex.UV0), Hash);
                        Hash = FCrc::MemCrc32(&Vertex.Color, sizeof(Vertex.Color), Hash);
                        Hash = FCrc::MemCrc32(&Vertex.Tangent.TangentX, sizeof(Vertex.Tangent.TangentX), Hash);
                        Hash = FCrc::MemCrc32(&Vertex.Tangent.bFlipTangentY, sizeof(bool), Hash);
                    }
                    Hash = FCrc::MemCrc32(Section->ProcIndexBuffer.GetData(),
                        Section->ProcIndexBuffer.Num() * sizeof(uint32), Hash);
                }
        TestTrue(TEXT("The generated canyon graph is valid"), Island->GetCanyonLayout().Validate());
        TestTrue(TEXT("The canyon has geometry"), Vertices > 0 && Triangles > 0);
        TestTrue(TEXT("Simplified canyon keeps a bounded triangle budget"), Triangles < 100000);
        TestTrue(TEXT("Canyon generation finishes within five seconds"), Seconds < 5.0);
        TArray<UPointLightComponent*> Lights;
        Island->GetComponents(Lights);
        TestTrue(TEXT("Canyon uses at most eight dynamic fill lights"), Lights.Num() <= 8);
        UE_LOG(LogTemp, Display, TEXT("CANYON_PERF Seed=%d Seconds=%.6f Vertices=%d Triangles=%d Hash=%u"),
            Seed, Seconds, Vertices, Triangles, Hash);
        Island->Destroy();
    }
    // Compare the new batched terrain collision with the original repeated
    // collision cooking on a seed that blocks the existing traversal test.
    auto* CollisionIsland = World->SpawnActorDeferred<AProceduralIsland>(
        AProceduralIsland::StaticClass(), FTransform::Identity);
    CollisionIsland->Theme = EIslandTheme::CanyonGraybox;
    CollisionIsland->Seed = 784343;
    CollisionIsland->MapScale = 1.f;
    CollisionIsland->FinishSpawning(FTransform::Identity);
    auto SampleCollision = [&]()
    {
        TArray<bool> Results;
        FVector Previous = FVector::ZeroVector;
        const auto& Cave = CollisionIsland->GetCanyonLayout();
        for (int32 Step = 0; Step <= 32; ++Step)
        {
            FVector Point, Along;
            Cave.SampleCave(Step / 32.f, Point, Along);
            const FVector Center(Point.X, Point.Y, Cave.HeightAt(Point.X, Point.Y) + 130.f);
            FCollisionQueryParams Query;
            Query.bTraceComplex = true;
            FHitResult Hit;
            Results.Add(World->LineTraceSingleByChannel(Hit, Center + FVector(0, 0, 180),
                Center - FVector(0, 0, 160), ECC_Visibility, Query));
            if (Step > 0)
                Results.Add(World->SweepSingleByChannel(Hit, Previous, Center, FQuat::Identity,
                    ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 92.f)));
            Previous = Center;
        }
        return Results;
    };
    const auto BatchedCollision = SampleCollision();
    TArray<UProceduralMeshComponent*> CollisionMeshes;
    CollisionIsland->GetComponents(CollisionMeshes);
    for (auto* Mesh : CollisionMeshes)
        if (Mesh->GetNumSections() == 4)
            for (int32 SectionIndex = 0; SectionIndex < 4; ++SectionIndex)
            {
                const FProcMeshSection Section = *Mesh->GetProcMeshSection(SectionIndex);
                Mesh->SetProcMeshSection(SectionIndex, Section);
            }
    const auto RepeatedCollision = SampleCollision();
    TestTrue(TEXT("Batched and repeated terrain cooking give identical cave collision queries"),
        BatchedCollision == RepeatedCollision);
    CollisionIsland->Destroy();
    return true;
}
#endif
