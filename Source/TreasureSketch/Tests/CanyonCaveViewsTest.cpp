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
    Island->Seed = 1000;
    Island->Theme = EIslandTheme::CanyonGraybox;
    Island->MapScale = 1.f;
    Island->OnConstruction(Island->GetActorTransform());
    const FCanyonGrayboxLayout& Cave = Island->GetCanyonLayout();
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
    Cave.SampleCave(0.74f, Interior, InteriorForward);
    const FVector FarCamera = Ground(Mid) + Side * 2400.f + FVector(0.f, 0.f, 1450.f);
    const FVector MouthCamera = Ground(Entrance) - (Cave.Nodes[Cave.CavePathNodes[1]].Position
        - Entrance).GetSafeNormal2D() * 650.f + Side * 180.f + FVector(0.f, 0.f, 180.f);
    const FVector InsideCamera = Ground(Mid) + FVector(0.f, 0.f, 170.f);
    TestTrue(TEXT("Far mountain view captured"), TakeView(TEXT("seed1000_far"), FarCamera,
        Ground(Mid) + FVector(0.f, 0.f, 580.f)));
    TestTrue(TEXT("Entrance view captured"), TakeView(TEXT("seed1000_entrance"), MouthCamera,
        Ground(Cave.Nodes[Cave.CavePathNodes[1]].Position) + FVector(0.f, 0.f, 190.f)));
    TestTrue(TEXT("Interior view captured"), TakeView(TEXT("seed1000_inside"), InsideCamera,
        Ground(Interior) + FVector(0.f, 0.f, 190.f)));
    Capture->DestroyComponent();
    World->DestroyActor(Island);
    return true;
}

#endif
