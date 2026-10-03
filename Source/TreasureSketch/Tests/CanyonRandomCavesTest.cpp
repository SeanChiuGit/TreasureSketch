#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../ProceduralIsland.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonRandomCavesTest, "TreasureSketch.Canyon.RandomCavePool",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonRandomCavesTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.World() && (Context.WorldType == EWorldType::Editor
            || Context.WorldType == EWorldType::Game)) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("A collision world exists"), World)) return false;
    TSet<ECanyonCavePattern> Patterns;
    for (const int32 Seed : { 1004, 1008, 1009 })
    {
        AProceduralIsland* Island = World->SpawnActor<AProceduralIsland>();
        if (!TestNotNull(TEXT("Random canyon actor exists"), Island)) return false;
        Island->Seed = Seed;
        Island->Theme = EIslandTheme::CanyonGraybox;
        Island->MapScale = 1.f;
        Island->OnConstruction(Island->GetActorTransform());
        const FCanyonGrayboxLayout& Layout = Island->GetCanyonLayout();
        TestTrue(FString::Printf(TEXT("Random seed %d has a valid graph"), Seed), Layout.Validate());
        TestTrue(TEXT("Random map contains multiple cave networks"), Layout.CaveNetworks.Num() >= 2);
        TArray<TArray<int32>> Paths;
        for (const FCanyonCaveNetwork& Network : Layout.CaveNetworks)
        {
            Patterns.Add(Network.Pattern);
            if (Network.Branches.IsEmpty()) Paths.Add(Network.PathNodes);
            else Paths.Append(Network.Branches);
        }
        for (int32 Branch = 0; Branch < Paths.Num(); ++Branch)
        {
            FVector Previous = FVector::ZeroVector;
            bool bPrevious = false, bFloor = true, bClear = true;
            for (int32 Segment = 1; Segment < Paths[Branch].Num(); ++Segment)
            {
                const FVector A = Layout.Nodes[Paths[Branch][Segment - 1]].Position;
                const FVector B = Layout.Nodes[Paths[Branch][Segment]].Position;
                const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(A, B) / 75.f));
                for (int32 Step = 0; Step <= Steps; ++Step)
                {
                    const FVector Point = FMath::Lerp(A, B, Step / static_cast<float>(Steps));
                    FVector Center = Island->GetActorLocation() + Point + FVector(0.f, 0.f, 730.f);
                    FHitResult Ground, Blocker;
                    const bool bGround = World->LineTraceSingleByChannel(Ground,
                        Center + FVector(0.f, 0.f, 70.f), Center - FVector(0.f, 0.f, 430.f),
                        ECC_Visibility) && Ground.GetActor() == Island && Ground.ImpactNormal.Z > 0.5f;
                    if (bGround) Center.Z = Ground.ImpactPoint.Z + 130.f;
                    const bool bBlocked = bPrevious && World->SweepSingleByChannel(Blocker,
                        Previous, Center, FQuat::Identity, ECC_Pawn,
                        FCollisionShape::MakeCapsule(42.f, 92.f));
                    if (!bGround || bBlocked)
                    {
                        AddError(FString::Printf(TEXT("Seed %d %s branch%d segment%d step%d floor%d blocked%d"),
                            Seed, Layout.CaveName(), Branch, Segment, Step, bGround, bBlocked));
                        bFloor &= bGround;
                        bClear &= !bBlocked;
                        break;
                    }
                    Previous = Center;
                    bPrevious = true;
                }
                if (!bFloor || !bClear) break;
            }
            TestTrue(FString::Printf(TEXT("Seed %d branch%d has floor and capsule clearance"), Seed, Branch),
                bFloor && bClear);
        }
        World->DestroyActor(Island);
    }
    TestEqual(TEXT("Ordinary seeds exercise all three cave types"), Patterns.Num(), 3);
    return true;
}

#endif
