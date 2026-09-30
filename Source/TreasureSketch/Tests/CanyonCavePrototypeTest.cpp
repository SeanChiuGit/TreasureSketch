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
#include "../CanyonCavePrototype.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanyonCavePrototypeTest,
    "TreasureSketch.Canyon.FixedMountainPrototype",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCanyonCavePrototypeTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.World() && (Context.WorldType == EWorldType::Editor
            || Context.WorldType == EWorldType::Game)) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("A renderable world exists"), World)) return false;

    ACanyonCavePrototype* Mountain = World->SpawnActor<ACanyonCavePrototype>();
    if (!TestNotNull(TEXT("One-piece mountain exists"), Mountain)) return false;
    Mountain->OnConstruction(Mountain->GetActorTransform());

    bool bTunnelFloor = true, bTunnelClear = true;
    bool bBypassFloor = true, bBypassClear = true;
    FVector Previous = FVector::ZeroVector, PreviousBypass = FVector::ZeroVector;
    for (int32 I = 0; I <= 56; ++I)
    {
        const float X = FMath::Lerp(-3500.f, 3500.f, I / 56.f);
        const FVector Center = Mountain->GetActorLocation() + FVector(X, 0.f, 130.f);
        FHitResult Ground;
        const bool bTunnelHit = World->LineTraceSingleByChannel(Ground,
            Center + FVector(0.f, 0.f, 160.f), Center - FVector(0.f, 0.f, 180.f),
            ECC_Visibility);
        bTunnelFloor &= bTunnelHit && Ground.GetActor() == Mountain
            && FMath::Abs(Ground.ImpactPoint.Z - Mountain->GetActorLocation().Z) < 35.f;
        if (I > 0)
        {
            FHitResult Block;
            if (World->SweepSingleByChannel(Block, Previous, Center, FQuat::Identity,
                ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 92.f)))
            {
                AddError(FString::Printf(TEXT("Tunnel capsule blocked at x=%.0f"), X));
                bTunnelClear = false;
                break;
            }
        }
        Previous = Center;
        const float BypassY = ACanyonCavePrototype::BypassY(X);
        const float BypassZ = ACanyonCavePrototype::SurfaceHeight(X, BypassY);
        const FVector Bypass = Mountain->GetActorLocation() + FVector(X, BypassY, BypassZ + 130.f);
        const bool bBypassHit = World->LineTraceSingleByChannel(Ground,
            Bypass + FVector(0.f, 0.f, 160.f), Bypass - FVector(0.f, 0.f, 180.f),
            ECC_Visibility);
        bBypassFloor &= bBypassHit && Ground.GetActor() == Mountain
            && FMath::Abs(Ground.ImpactPoint.Z - (Mountain->GetActorLocation().Z + BypassZ)) < 35.f;
        if (I > 0)
        {
            FHitResult Block;
            bBypassClear &= !World->SweepSingleByChannel(Block, PreviousBypass, Bypass,
                FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 92.f));
        }
        PreviousBypass = Bypass;
    }
    TestTrue(TEXT("Tunnel floor is continuous"), bTunnelFloor);
    TestTrue(TEXT("Player capsule traverses both mouths and tunnel"), bTunnelClear);
    TestTrue(TEXT("Exterior bypass has continuous ground"), bBypassFloor);
    TestTrue(TEXT("Player capsule can follow the exterior bypass"), bBypassClear);

    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(Mountain);
    Target->InitCustomFormat(960, 540, PF_B8G8R8A8, false);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Mountain);
    Capture->TextureTarget = Target;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->FOVAngle = 78.f;
    Capture->RegisterComponentWithWorld(World);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CanyonCavePrototype"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(
        TEXT("ImageWrapper"));
    auto CaptureView = [&](const TCHAR* Name, const FVector& Camera, const FVector& LookAt)
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
    const FVector Origin = Mountain->GetActorLocation();
    TestTrue(TEXT("Far view saved"), CaptureView(TEXT("fixed_far"),
        Origin + FVector(-4300.f, -2450.f, 1050.f), Origin + FVector(-500.f, 0.f, 580.f)));
    TestTrue(TEXT("Entrance view saved"), CaptureView(TEXT("fixed_entrance"),
        Origin + FVector(-3450.f, 0.f, 190.f), Origin + FVector(-2100.f, 0.f, 230.f)));
    TestTrue(TEXT("Interior view saved"), CaptureView(TEXT("fixed_inside"),
        Origin + FVector(0.f, 0.f, 160.f), Origin + FVector(2600.f, 0.f, 200.f)));
    TestTrue(TEXT("Exit view saved"), CaptureView(TEXT("fixed_exit"),
        Origin + FVector(3450.f, 0.f, 190.f), Origin + FVector(2100.f, 0.f, 230.f)));
    Capture->DestroyComponent();
    World->DestroyActor(Mountain);
    return true;
}

#endif
