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

    AProceduralIsland* Island = World->SpawnActor<AProceduralIsland>();
    if (!TestNotNull(TEXT("Three-mouth preview actor exists"), Island)) return false;
    Island->Seed = 1002;
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
    if (!TestTrue(TEXT("One hall joins three surface mouths"),
        Cave.Validate() && Cave.CavePattern == ECanyonCavePattern::ThreeMouthHall
        && Cave.CaveBranches.Num() == 3 && Cave.CaveMouthNodes.Num() == 3
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
        FVector Previous = FVector::ZeroVector;
        bool bFloor = true, bClear = true;
        float RouteLength = 0.f;
        for (int32 I = 1; I < Cave.CaveBranches[Branch].Num(); ++I)
            RouteLength += FVector::Dist2D(
                Cave.Nodes[Cave.CaveBranches[Branch][I - 1]].Position,
                Cave.Nodes[Cave.CaveBranches[Branch][I]].Position);
        UE_LOG(LogTemp, Warning, TEXT("Three-mouth branch %d length %.0f cm"),
            Branch, RouteLength);
        for (int32 Step = 0; Step <= 32; ++Step)
        {
            FVector Point, Tangent;
            Cave.SampleCaveBranch(Branch, Step / 32.f, Point, Tangent);
            const FVector Center = Origin + FVector(Point.X, Point.Y,
                600.f + Point.Z + 130.f);
            FHitResult Ground;
            const bool bGroundHit = World->LineTraceSingleByChannel(Ground,
                Center + FVector(0.f, 0.f, 180.f),
                Center - FVector(0.f, 0.f, 240.f), ECC_Visibility)
                && Ground.GetActor() == Island;
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
        if (Branch == 2)
            for (const FCanyonGrayboxEdge& Edge : Cave.Edges)
                if (!Edge.bCave && (Edge.A == Cave.CaveMouthNodes[2]
                    || Edge.B == Cave.CaveMouthNodes[2]))
                {
                    const int32 Neighbor = Edge.A == Cave.CaveMouthNodes[2] ? Edge.B : Edge.A;
                    const FVector Approach = (Cave.Nodes[Neighbor].Position - Mouth).GetSafeNormal2D();
                    Camera = Origin + FVector(Mouth.X, Mouth.Y,
                        Cave.SurfaceHeightAt(Mouth.X, Mouth.Y) + 180.f) + Approach * 320.f;
                    break;
                }
        const FVector LookAt = Origin + FVector(Inside.X, Inside.Y,
            600.f + Inside.Z + 200.f);
        TestTrue(FString::Printf(TEXT("Mouth %d view saved"), Branch),
            TakeView(FString::Printf(TEXT("seed1002_mouth%d"), Branch), Camera, LookAt));
    }
    FVector BranchPoint, BranchTangent;
    Cave.SampleCaveBranch(2, 0.55f, BranchPoint, BranchTangent);
    TestTrue(TEXT("Hall view saved"), TakeView(TEXT("seed1002_hall"),
        Origin + FVector(HallPoint.X, HallPoint.Y,
            600.f + HallPoint.Z + 170.f),
        Origin + FVector(BranchPoint.X, BranchPoint.Y,
            600.f + BranchPoint.Z + 200.f)));
    Capture->DestroyComponent();
    World->DestroyActor(Island);
    return true;
}

#endif
