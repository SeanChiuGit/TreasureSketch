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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonCaveViewsTest, "TreasureSketch.Canyon.CaveViews",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonCaveViewsTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.World() && (Context.WorldType == EWorldType::Editor
            || Context.WorldType == EWorldType::Game)) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("A renderable world is available"), World)) return false;

    AProceduralIsland* Island = World->SpawnActor<AProceduralIsland>();
    if (!TestNotNull(TEXT("Canyon preview actor exists"), Island)) return false;
    Island->Seed = 1001;
    Island->Theme = EIslandTheme::CanyonGraybox;
    Island->MapScale = 1.f;
    Island->OnConstruction(Island->GetActorTransform());
    const FCanyonGrayboxLayout& Cave = Island->GetCanyonLayout();
    TestTrue(TEXT("Preview uses the new long winding cave"),
        Cave.CavePattern == ECanyonCavePattern::LongWindingThrough);
    float RouteLength = 0.f;
    for (int32 I = 1; I < Cave.CavePathNodes.Num(); ++I)
        RouteLength += FVector::Dist2D(Cave.Nodes[Cave.CavePathNodes[I - 1]].Position,
            Cave.Nodes[Cave.CavePathNodes[I]].Position);
    UE_LOG(LogTemp, Warning, TEXT("LONG_CAVE_PREVIEW seed=1001 length_cm=%.0f scale=%.3f mouths_cm=%.0f"),
        RouteLength, Cave.LengthScale, FVector::Dist2D(
            Cave.Nodes[Cave.CaveMouthNodes[0]].Position,
            Cave.Nodes[Cave.CaveMouthNodes[1]].Position));
    TestTrue(TEXT("Long cave is at least 100 metres on the 1x map"), RouteLength >= 10000.f);
    float MinimumRoof = TNumericLimits<float>::Max(), MinimumRoofT = 0.f;
    for (int32 I = 10; I <= 90; ++I)
    {
        const float T = I / 100.f;
        FVector Point, Along;
        Cave.SampleCave(T, Point, Along);
        const float Roof = Cave.SurfaceHeightAt(Point.X, Point.Y)
            - Cave.HeightAt(Point.X, Point.Y) - Cave.CaveClearance(T);
        if (Roof < MinimumRoof) { MinimumRoof = Roof; MinimumRoofT = T; }
    }
    FVector ThinPoint, ThinAlong;
    Cave.SampleCave(MinimumRoofT, ThinPoint, ThinAlong);
    float NearestRoute = TNumericLimits<float>::Max();
    int32 NearestEdge = INDEX_NONE;
    for (int32 EdgeIndex = 0; EdgeIndex < Cave.Edges.Num(); ++EdgeIndex)
    {
        const FCanyonGrayboxEdge& Edge = Cave.Edges[EdgeIndex];
        if (Edge.bCave) continue;
        const FVector A = Cave.Nodes[Edge.A].Position, B = Cave.Nodes[Edge.B].Position;
        const FVector2D Delta(B.X - A.X, B.Y - A.Y);
        const float U = FMath::Clamp(FVector2D::DotProduct(
            FVector2D(ThinPoint.X - A.X, ThinPoint.Y - A.Y), Delta)
            / Delta.SizeSquared(), 0.f, 1.f);
        const float Distance = FVector2D::Distance(FVector2D(ThinPoint.X, ThinPoint.Y),
            FVector2D(A.X, A.Y) + Delta * U);
        if (Distance < NearestRoute) { NearestRoute = Distance; NearestEdge = EdgeIndex; }
    }
    UE_LOG(LogTemp, Warning, TEXT("LONG_CAVE_PREVIEW minimum_roof_cm=%.0f at_t=%.2f xy=(%.0f,%.0f) nearest_route_cm=%.0f edge=%d"),
        MinimumRoof, MinimumRoofT, ThinPoint.X, ThinPoint.Y, NearestRoute, NearestEdge);
    TestTrue(TEXT("Long cave remains inside the mountain"), MinimumRoof > 70.f);
    FVector Entrance, Exit, Mid, Forward;
    Cave.SampleCave(0.f, Entrance, Forward);
    Cave.SampleCave(1.f, Exit, Forward);
    Cave.SampleCave(0.5f, Mid, Forward);
    const FVector Side(-Forward.Y, Forward.X, 0.f);
    const FVector Origin = Island->GetActorLocation();
    auto Ground = [&](const FVector& P) { return Origin + FVector(P.X, P.Y, Cave.HeightAt(P.X, P.Y)); };

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

    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CanyonCaveReview"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    auto TakeView = [&](const TCHAR* Name, const FVector& Camera, const FVector& LookAt)
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
            *FPaths::Combine(Directory, FString(Name) + TEXT(".png")));
    };

    FVector Interior, InteriorForward;
    Cave.SampleCave(0.43f, Interior, InteriorForward);
    FVector InteriorCameraPoint, InteriorCameraForward;
    Cave.SampleCave(0.30f, InteriorCameraPoint, InteriorCameraForward);
    const FVector FarCamera = Ground(Mid) + Side * 2400.f + FVector(0.f, 0.f, 1450.f);
    const FVector MouthCamera = Ground(Entrance) - (Cave.Nodes[Cave.CavePathNodes[1]].Position
        - Entrance).GetSafeNormal2D() * 650.f + Side * 180.f + FVector(0.f, 0.f, 180.f);
    const FVector InsideCamera = Ground(InteriorCameraPoint) + FVector(0.f, 0.f, 170.f);
    TestTrue(TEXT("Far mountain view captured"), TakeView(TEXT("seed1001_far"), FarCamera,
        Ground(Mid) + FVector(0.f, 0.f, 580.f)));
    TestTrue(TEXT("Entrance view captured"), TakeView(TEXT("seed1001_entrance"), MouthCamera,
        Ground(Cave.Nodes[Cave.CavePathNodes[1]].Position) + FVector(0.f, 0.f, 190.f)));
    TestTrue(TEXT("Interior view captured"), TakeView(TEXT("seed1001_inside"), InsideCamera,
        Ground(Interior) + FVector(0.f, 0.f, 190.f)));
    FVector ExitForwardPoint, ExitForward;
    Cave.SampleCave(1.f, ExitForwardPoint, ExitForward);
    const FVector ExitCamera = Ground(Exit) + ExitForward * 650.f
        + FVector(0.f, 0.f, 180.f);
    TestTrue(TEXT("Exit view captured"), TakeView(TEXT("seed1001_exit"), ExitCamera,
        Ground(Exit) - ExitForward * 250.f + FVector(0.f, 0.f, 190.f)));

    FVector Previous = FVector::ZeroVector;
    bool bFloorFound = true, bTraversable = true;
    for (int32 Step = 0; Step <= 64; ++Step)
    {
        FVector Point, Along;
        Cave.SampleCave(Step / 64.f, Point, Along);
        const FVector Center = Ground(Point) + FVector(0.f, 0.f, 130.f);
        FHitResult Floor;
        bFloorFound &= World->LineTraceSingleByChannel(Floor,
            Center + FVector(0.f, 0.f, 180.f), Center - FVector(0.f, 0.f, 180.f),
            ECC_Visibility) && Floor.GetActor() == Island;
        if (Step > 0)
        {
            FHitResult Blocker;
            if (World->SweepSingleByChannel(Blocker, Previous, Center, FQuat::Identity,
                ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 92.f)))
            {
                AddError(FString::Printf(TEXT("Long cave blocked at step %d"), Step));
                bTraversable = false;
                break;
            }
        }
        Previous = Center;
    }
    TestTrue(TEXT("Long cave has a continuous floor"), bFloorFound);
    TestTrue(TEXT("Player capsule traverses the long cave"), bTraversable);
    Capture->DestroyComponent();
    World->DestroyActor(Island);
    return true;
}

#endif
