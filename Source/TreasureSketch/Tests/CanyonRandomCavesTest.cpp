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
    TArray<int32> Seeds = { 1001, 1008, 1006, 1010, 1020, 1030, 1040, 1050, 1051 };
    bool bFoundRandomUphill = false;
    for (int32 Seed = 1100; Seed < 1300; ++Seed)
        if (FCanyonGrayboxLayout::Generate(Seed).CaveNetworks.ContainsByPredicate(
            [](const FCanyonCaveNetwork& Cave) { return Cave.Elevation == ECanyonCaveElevation::Ascending; }))
        {
            Seeds.Add(Seed);
            bFoundRandomUphill = true;
            break;
        }
    TestTrue(TEXT("Ordinary map pool can retain an uphill cave"), bFoundRandomUphill);
    for (const int32 Seed : Seeds)
    {
        AProceduralIsland* Island = World->SpawnActor<AProceduralIsland>();
        if (!TestNotNull(TEXT("Random canyon actor exists"), Island)) return false;
        Island->Seed = Seed;
        Island->Theme = EIslandTheme::CanyonGraybox;
        Island->MapScale = 1.f;
        Island->OnConstruction(Island->GetActorTransform());
        const FCanyonGrayboxLayout& Layout = Island->GetCanyonLayout();
        for (const FCanyonGrayboxNode& Node : Layout.Nodes)
            TestTrue(TEXT("Legal canyon floors remain above fall recovery"),
                Island->GetActorTransform().TransformPosition(Node.Position + FVector(0.f, 0.f, 600.f)).Z
                    >= Island->GetFallRecoveryLimitZ() + 599.f);
        TestTrue(FString::Printf(TEXT("Random seed %d has a valid graph"), Seed), Layout.Validate());
        if (Seed == 1050 || Seed == 1051)
        {
            const FVector Entry = Layout.Nodes[Layout.CaveMouthNodes[0]].Position;
            const FVector Exit = Layout.Nodes[Layout.CaveMouthNodes[1]].Position;
            TestTrue(TEXT("Ground-level entrance meets the exterior surface"),
                FMath::Abs(Layout.SurfaceHeightAt(Entry.X, Entry.Y) - (600.f + Entry.Z)) < 15.f);
            TestTrue(TEXT("Uphill exit rises while level comparison stays level"),
                Seed == 1050 ? Exit.Z - Entry.Z > 250.f : FMath::Abs(Exit.Z - Entry.Z) < 1.f);
            for (int32 I = 1; I < Layout.CavePathNodes.Num(); ++I)
                TestTrue(TEXT("Main passage never descends from its ground-level entrance"),
                    Layout.Nodes[Layout.CavePathNodes[I]].Position.Z + 1.f
                        >= Layout.Nodes[Layout.CavePathNodes[I - 1]].Position.Z);
            for (float T : { 0.2f, 0.4f, 0.6f, 0.8f })
            {
                FVector Point, Tangent;
                Layout.SampleCave(T, Point, Tangent);
                TestTrue(TEXT("Uphill passage remains covered by mountain rock"),
                    Layout.SurfaceHeightAt(Point.X, Point.Y) > 600.f + Point.Z + Layout.CaveClearance(T));
            }
            UE_LOG(LogTemp, Display, TEXT("CANYON_ELEVATION Seed=%d RiseMeters=%.1f"), Seed, (Exit.Z - Entry.Z) / 100.f);
        }
        TestTrue(TEXT("Random map contains cave networks"),
            Layout.CaveNetworks.Num() >= (Seed == 1001 || Seed == 1010 || Seed == 1020
                || Seed == 1030 || Seed == 1040 || Seed == 1050 || Seed == 1051 ? 1 : 2));
        if (Seed == 1030 || Seed == 1040)
        {
            TestTrue(TEXT("Hall and loop comparisons contain optional dead ends"), !Layout.CaveDeadEnds.IsEmpty());
            UE_LOG(LogTemp, Display, TEXT("CANYON_DECORATED_CAVE Seed=%d Type=%s Additions=%d"),
                Seed, Layout.CaveName(), Layout.CaveDeadEnds.Num());
            TestEqual(TEXT("Decoration preserves the base cave branch count"), Layout.CaveBranches.Num(), Seed == 1030 ? 3 : 4);
            TestEqual(TEXT("Decoration preserves the central hall count"), Layout.CaveHalls.Num(), Seed == 1030 ? 1 : 0);
        }
        if (Seed == 1020)
        {
            TestTrue(TEXT("Dead ends decorate the existing long cave"), Layout.CavePattern == ECanyonCavePattern::LongWindingThrough);
            TestEqual(TEXT("Fixed preview has three different side passage structures"), Layout.CaveDeadEnds.Num(), 3);
            TSet<ECanyonDeadEndKind> Kinds;
            float ShortLength = 0.f, LongLength = 0.f;
            for (const FCanyonDeadEnd& DeadEnd : Layout.CaveDeadEnds)
            {
                Kinds.Add(DeadEnd.Kind);
                float SideLength = 0.f;
                for (const auto& Path : DeadEnd.Paths)
                    for (int32 I = 1; I < Path.Num(); ++I)
                        SideLength += FVector::Dist2D(Layout.Nodes[Path[I - 1]].Position, Layout.Nodes[Path[I]].Position);
                if (DeadEnd.Kind == ECanyonDeadEndKind::ShortAlcove) ShortLength = SideLength;
                if (DeadEnd.Kind == ECanyonDeadEndKind::LongWinding) LongLength = SideLength;
                if (DeadEnd.Kind == ECanyonDeadEndKind::Forked)
                    TestEqual(TEXT("Forked dead end has two distinct terminal rooms"), DeadEnd.EndNodes.Num(), 2);
                UE_LOG(LogTemp, Display, TEXT("CANYON_DEAD_END Kind=%d LengthMeters=%.1f Ends=%d"),
                    static_cast<int32>(DeadEnd.Kind), SideLength / 100.f, DeadEnd.EndNodes.Num());
            }
            TestEqual(TEXT("Preview offers short, long and forked side passages"), Kinds.Num(), 3);
            TestTrue(TEXT("Long side passage is substantially longer than the alcove"), LongLength > ShortLength * 3.f);
            float Length = 0.f;
            for (int32 I = 1; I < Layout.CavePathNodes.Num(); ++I)
                Length += FVector::Dist2D(Layout.Nodes[Layout.CavePathNodes[I - 1]].Position,
                    Layout.Nodes[Layout.CavePathNodes[I]].Position);
            UE_LOG(LogTemp, Display, TEXT("CANYON_BRANCH_PREVIEW Seed=%d MainMeters=%.1f SidePassages=%d"),
                Seed, Length / 100.f, Layout.CaveDeadEnds.Num());
            TestTrue(TEXT("Branched cave retains a long main passage"), Length > 9000.f);
        }
        if (Seed == 1010)
        {
            TestTrue(TEXT("Fixed preview contains a loop cave"), Layout.CavePattern == ECanyonCavePattern::LongLoop);
            for (int32 Arm = 1; Arm <= 2; ++Arm)
            {
                float Length = 0.f;
                const TArray<int32>& Path = Layout.CaveBranches[Arm];
                for (int32 I = 1; I < Path.Num(); ++I)
                    Length += FVector::Dist2D(Layout.Nodes[Path[I - 1]].Position, Layout.Nodes[Path[I]].Position);
                UE_LOG(LogTemp, Display, TEXT("CANYON_LOOP_ARM Seed=%d Arm=%d LengthMeters=%.1f"), Seed, Arm, Length / 100.f);
                TestTrue(TEXT("Each loop arm provides substantial travel"), Length >= 4000.f);
            }
        }
        TArray<TArray<int32>> Paths;
        for (const FCanyonCaveNetwork& Network : Layout.CaveNetworks)
        {
            Patterns.Add(Network.Pattern);
            if (Network.Branches.IsEmpty()) Paths.Add(Network.PathNodes);
            else Paths.Append(Network.Branches);
            for (const FCanyonDeadEnd& DeadEnd : Network.DeadEnds) Paths.Append(DeadEnd.Paths);
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
                        UE_LOG(LogTemp, Warning, TEXT("CANYON_BLOCK Point=%s Hit=%s Normal=%s Component=%s"),
                            *Center.ToString(), *Blocker.ImpactPoint.ToString(), *Blocker.ImpactNormal.ToString(),
                            *GetNameSafe(Blocker.GetComponent()));
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
        int32 UndergroundTreasures = 0;
        for (int32 Sample = 0; Sample < 40; ++Sample)
        {
            FRandomStream TreasureStream(Seed * 100 + Sample);
            FRandomStream ReplayStream(Seed * 100 + Sample);
            const FVector Treasure = Island->FindTreasurePoint(TreasureStream);
            TestTrue(TEXT("Treasure selection repeats for the same stream"),
                Treasure.Equals(Island->FindTreasurePoint(ReplayStream), 0.1f));
            const FVector Local = Island->GetActorTransform().InverseTransformPosition(Treasure);
            if (Local.Z < Layout.SurfaceHeightAt(Local.X, Local.Y) - 200.f)
            {
                ++UndergroundTreasures;
                FHitResult Floor;
                TestTrue(TEXT("Underground treasure rests on a walkable collision floor"),
                    World->LineTraceSingleByChannel(Floor, Treasure + FVector(0.f, 0.f, 20.f),
                        Treasure - FVector(0.f, 0.f, 20.f), ECC_Visibility)
                    && Floor.GetActor() == Island && Floor.ImpactNormal.Z >= 0.55f);
                TestTrue(TEXT("Underground treasure leaves room for a player"),
                    !World->OverlapBlockingTestByChannel(Treasure + FVector(0.f, 0.f, 130.f),
                        FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 96.f)));
            }
        }
        TestTrue(TEXT("Treasure pool includes both cave and surface locations"),
            UndergroundTreasures > 4 && UndergroundTreasures < 36);
        World->DestroyActor(Island);
    }
    TestEqual(TEXT("Representative seeds exercise all four base cave types"), Patterns.Num(), 4);
    return true;
}

#endif
