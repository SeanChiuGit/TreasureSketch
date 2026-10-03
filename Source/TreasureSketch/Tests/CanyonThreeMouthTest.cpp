#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "../ProceduralIsland.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonThreeMouthTest,
    "TreasureSketch.Canyon.ThreeMouthHall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonThreeMouthTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.World() && (Context.WorldType == EWorldType::Editor
            || Context.WorldType == EWorldType::Game)) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("A renderable world is available"), World)) return false;

    for (int32 PreviewSeed : { 1002, 1003 })
    {
    AProceduralIsland* Island = World->SpawnActor<AProceduralIsland>();
    if (!TestNotNull(TEXT("Three-mouth preview actor exists"), Island)) return false;
    Island->Seed = PreviewSeed;
    Island->Theme = EIslandTheme::CanyonGraybox;
    Island->MapScale = 1.f;
    Island->OnConstruction(Island->GetActorTransform());
    const FCanyonGrayboxLayout& Cave = Island->GetCanyonLayout();
    for (int32 Index = 0; Index < Cave.Edges.Num(); ++Index)
    {
        const FCanyonGrayboxEdge& Edge = Cave.Edges[Index];
        const FVector Delta = Cave.Nodes[Edge.B].Position - Cave.Nodes[Edge.A].Position;
        if (Delta.Size2D() < 900.f * Cave.LengthScale
            || FMath::Abs(Delta.Z) / Delta.Size2D() > (Edge.bCave ? 0.25f : 0.18f))
            UE_LOG(LogTemp, Warning, TEXT("Invalid cave preview edge %d (%d -> %d): %s -> %s length %.0f slope %.3f"),
                Index, Edge.A, Edge.B,
                *Cave.Nodes[Edge.A].Position.ToString(), *Cave.Nodes[Edge.B].Position.ToString(),
                Delta.Size2D(), FMath::Abs(Delta.Z) / Delta.Size2D());
    }
    if (!TestTrue(TEXT("One hall joins three branches with the expected exterior mouths"),
        Cave.Validate() && Cave.CavePattern == ECanyonCavePattern::ThreeMouthHall
        && Cave.CaveBranches.Num() == 3 && Cave.CaveMouthNodes.Num() == (Cave.CaveBranchOpen[2] ? 3 : 2)
        && Cave.CaveHalls.Num() == 1))
    {
        World->DestroyActor(Island);
        return false;
    }

    const FVector Origin = Island->GetActorLocation();
    const FCanyonCaveHall& Hall = Cave.CaveHalls[0];
    const FVector HallPoint = Cave.Nodes[Hall.Node].Position;
    const float Roof = Cave.SurfaceHeightAt(HallPoint.X, HallPoint.Y)
        - Cave.HeightAt(HallPoint.X, HallPoint.Y) - Hall.Clearance;
    TestTrue(TEXT("The hall has a rock roof"), Roof > 80.f);

    for (int32 Branch = 0; Branch < 3; ++Branch)
    {
        const int32 Endpoint = Cave.CaveBranches[Branch][0];
        const FVector End = Cave.Nodes[Endpoint].Position;
        const FVector Center = Origin + End + FVector(0.f, 0.f, 730.f);
        if (Cave.CaveBranchOpen[Branch])
        {
            bool bApproach = false;
            for (const FCanyonGrayboxEdge& Edge : Cave.Edges)
            {
                if (Edge.bCave || (Edge.A != Endpoint && Edge.B != Endpoint)) continue;
                const int32 Neighbor = Edge.A == Endpoint ? Edge.B : Edge.A;
                const FVector Delta = Cave.Nodes[Neighbor].Position - End;
                const FVector Outside = Center + Delta * FMath::Min(0.75f, 320.f / Delta.Size2D());
                FHitResult Blocker, Ground, RoofHit;
                const bool bClear = !World->SweepSingleByChannel(Blocker, Center, Outside,
                    FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 92.f));
                const bool bGround = World->LineTraceSingleByChannel(Ground,
                    Outside + FVector(0.f, 0.f, 180.f), Outside - FVector(0.f, 0.f, 240.f),
                    ECC_Visibility) && Ground.GetActor() == Island;
                const bool bSky = !World->LineTraceSingleByChannel(RoofHit,
                    Outside + FVector(0.f, 0.f, 180.f), Outside + FVector(0.f, 0.f, 2500.f),
                    ECC_Visibility);
                bApproach |= bClear && bGround && bSky;
            }
            TestTrue(FString::Printf(TEXT("Seed %d mouth %d reaches walkable exterior ground"),
                PreviewSeed, Branch), bApproach);
        }
        else
        {
            FVector Point, Tangent;
            Cave.SampleCaveBranch(Branch, 0.f, Point, Tangent);
            FHitResult Cap, Cover;
            TestTrue(TEXT("Enclosed endpoint has a rock cap"), World->SweepSingleByChannel(Cap,
                Center, Center - Tangent * 600.f, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(42.f, 92.f)) && Cap.GetActor() == Island);
            TestTrue(TEXT("Enclosed endpoint stays underneath rock"), World->LineTraceSingleByChannel(Cover,
                Center, Center + FVector(0.f, 0.f, 2000.f), ECC_Visibility)
                && Cover.GetActor() == Island);
        }
    }

    for (int32 Branch = 0; Branch < 3; ++Branch)
    {
        FVector Previous = FVector::ZeroVector;
        bool bFloor = true, bClear = true;
        float RouteLength = 0.f;
        for (int32 I = 1; I < Cave.CaveBranches[Branch].Num(); ++I)
            RouteLength += FVector::Dist2D(
                Cave.Nodes[Cave.CaveBranches[Branch][I - 1]].Position,
                Cave.Nodes[Cave.CaveBranches[Branch][I]].Position);
        UE_LOG(LogTemp, Warning, TEXT("Three-mouth branch %d length %.0f cm"),
            Branch, RouteLength);
        TArray<FVector> Samples;
        for (int32 Segment = 1; Segment < Cave.CaveBranches[Branch].Num(); ++Segment)
        {
            const FVector A = Cave.Nodes[Cave.CaveBranches[Branch][Segment - 1]].Position;
            const FVector B = Cave.Nodes[Cave.CaveBranches[Branch][Segment]].Position;
            const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(A, B) / 75.f));
            for (int32 Step = 0; Step <= Steps; ++Step)
                Samples.Add(FMath::Lerp(A, B, Step / static_cast<float>(Steps)));
        }
        for (int32 Step = 0; Step < Samples.Num(); ++Step)
        {
            const FVector Point = Samples[Step];
            FVector Center = Origin + FVector(Point.X, Point.Y,
                600.f + Point.Z + 130.f);
            FHitResult Ground;
            const bool bGroundHit = World->LineTraceSingleByChannel(Ground,
                Center + FVector(0.f, 0.f, 70.f),
                Center - FVector(0.f, 0.f, 430.f), ECC_Visibility)
                && Ground.GetActor() == Island && Ground.ImpactNormal.Z > 0.5f;
            if (bGroundHit) Center.Z = Ground.ImpactPoint.Z + 130.f;
            if (!bGroundHit)
            {
                FHitResult DeepGround;
                const bool bDeep = World->LineTraceSingleByChannel(DeepGround,
                    Center + FVector(0.f, 0.f, 180.f),
                    Center - FVector(0.f, 0.f, 1500.f), ECC_Visibility);
                AddError(FString::Printf(TEXT("Branch %d floor missing at step %d point %s center %s deep %s %s"),
                    Branch, Step, *Point.ToString(), *Center.ToString(),
                    bDeep ? *DeepGround.ImpactPoint.ToString() : TEXT("none"),
                    bDeep && DeepGround.GetComponent() ? *DeepGround.GetComponent()->GetName() : TEXT("none")));
            }
            bFloor &= bGroundHit;
            if (Step > 0)
            {
                FHitResult Blocker;
                if (World->SweepSingleByChannel(Blocker, Previous, Center,
                    FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeCapsule(42.f, 92.f)))
                {
                    AddError(FString::Printf(TEXT("Branch %d blocked at step %d point %s floor %.0f center %s previous %s hit %s at %s"),
                        Branch, Step, *Point.ToString(),
                        600.f + Point.Z, *Center.ToString(), *Previous.ToString(),
                        Blocker.GetComponent() ? *Blocker.GetComponent()->GetName() : TEXT("unknown"),
                        *Blocker.ImpactPoint.ToString()));
                    bClear = false;
                    break;
                }
            }
            Previous = Center;
        }
        TestTrue(FString::Printf(TEXT("Branch %d has continuous floor"), Branch), bFloor);
        TestTrue(FString::Printf(TEXT("Branch %d reaches the hall"), Branch), bClear);
    }

    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(Island);
    Target->InitCustomFormat(960, 540, PF_B8G8R8A8, false);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Island);
    Capture->TextureTarget = Target;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->FOVAngle = 78.f;
    Capture->RegisterComponentWithWorld(World);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CanyonThreeMouthReview"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    auto TakeView = [&](const FString& Name, const FVector& Camera, const FVector& LookAt)
    {
        Capture->SetWorldLocationAndRotation(Camera, (LookAt - Camera).Rotation());
        Capture->CaptureScene();
        TArray<FColor> Pixels;
        if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)
            || Pixels.Num() != 960 * 540) return false;
        const TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
        if (!Png.IsValid() || !Png->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor),
            960, 540, ERGBFormat::BGRA, 8)) return false;
        const TArray64<uint8> Compressed = Png->GetCompressed(90);
        TArray<uint8> Bytes;
        Bytes.Append(Compressed.GetData(), static_cast<int32>(Compressed.Num()));
        return FFileHelper::SaveArrayToFile(Bytes,
            *FPaths::Combine(Directory, Name + TEXT(".png")));
    };
    for (int32 Branch = 0; Branch < 3; ++Branch)
    {
        FVector Mouth, Tangent, Inside, InsideTangent;
        Cave.SampleCaveBranch(Branch, 0.f, Mouth, Tangent);
        UE_LOG(LogTemp, Warning, TEXT("Mouth %d floor %.0f surface %.0f point %s"),
            Branch, 600.f + Mouth.Z, Cave.SurfaceHeightAt(Mouth.X, Mouth.Y), *Mouth.ToString());
        Cave.SampleCaveBranch(Branch, Branch == 2 ? 0.08f : 0.45f,
            Inside, InsideTangent);
        FVector Camera = Origin + FVector(Mouth.X, Mouth.Y,
            Cave.SurfaceHeightAt(Mouth.X, Mouth.Y) + 180.f) - Tangent * 550.f;
        if (Branch == 2 && Cave.CaveBranchOpen[2])
            for (const FCanyonGrayboxEdge& Edge : Cave.Edges)
                if (!Edge.bCave && (Edge.A == Cave.CaveBranches[2][0]
                    || Edge.B == Cave.CaveBranches[2][0]))
                {
                    const int32 Neighbor = Edge.A == Cave.CaveBranches[2][0] ? Edge.B : Edge.A;
                    const FVector Approach = (Cave.Nodes[Neighbor].Position - Mouth).GetSafeNormal2D();
                    Camera = Origin + FVector(Mouth.X, Mouth.Y,
                        Cave.SurfaceHeightAt(Mouth.X, Mouth.Y) + 180.f) + Approach * 320.f;
                    break;
                }
        if (!Cave.CaveBranchOpen[Branch])
            Camera = Origin + Mouth + FVector(0.f, 0.f, 780.f);
        const FVector LookAt = Origin + FVector(Inside.X, Inside.Y,
            600.f + Inside.Z + 200.f);
        TestTrue(FString::Printf(TEXT("Mouth %d view saved"), Branch),
            TakeView(FString::Printf(TEXT("seed%d_endpoint%d"), PreviewSeed, Branch), Camera, LookAt));
    }
    FVector BranchPoint, BranchTangent;
    Cave.SampleCaveBranch(2, 0.55f, BranchPoint, BranchTangent);
    TestTrue(TEXT("Hall view saved"), TakeView(FString::Printf(TEXT("seed%d_hall"), PreviewSeed),
        Origin + FVector(HallPoint.X, HallPoint.Y,
            600.f + HallPoint.Z + 170.f),
        Origin + FVector(BranchPoint.X, BranchPoint.Y,
            600.f + BranchPoint.Z + 200.f)));
    Capture->DestroyComponent();
    World->DestroyActor(Island);
    }
    return true;
}

#endif
