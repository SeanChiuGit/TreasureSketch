#include "TreasureSketchHUD.h"

#include "TreasureSketchGameState.h"
#include "TreasureSketchPlayerController.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureOnlineSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ATreasureSketchHUD::DrawHUD()
{
    Super::DrawHUD();
    ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PlayerOwner);
    ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
    if (!PC || !GS || !PS || !Canvas) return;

    if (GetNetMode() == NM_Standalone)
    {
        const UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>();
        const FString OnlineText = Online ? Online->GetStatus() : TEXT("Steam 在线服务未初始化");
        DrawRect(FLinearColor(0.015f, 0.02f, 0.035f, 0.94f), Canvas->SizeX * 0.16f, Canvas->SizeY * 0.16f, Canvas->SizeX * 0.68f, Canvas->SizeY * 0.64f);
        DrawText(TEXT("TREASURESKETCH STEAM 大厅"), FLinearColor(0.3f,0.85f,1.f), Canvas->SizeX * 0.20f, Canvas->SizeY * 0.21f, GEngine->GetLargeFont(), 1.15f);
        DrawText(TEXT("H 创建房间    J 刷新房间列表    K 加入列表第一个房间"), FLinearColor::White, Canvas->SizeX * 0.20f, Canvas->SizeY * 0.28f, GEngine->GetMediumFont(), 1.f);
        DrawText(OnlineText, FLinearColor(0.95f,0.85f,0.25f), Canvas->SizeX * 0.20f, Canvas->SizeY * 0.34f, GEngine->GetSmallFont(), 1.f);
        float RoomY = Canvas->SizeY * 0.41f;
        const TArray<FString>& Rooms = Online->GetRoomLines();
        DrawText(Rooms.IsEmpty() ? TEXT("房间列表为空") : TEXT("可加入房间："), FLinearColor(0.75f,0.8f,0.9f), Canvas->SizeX * 0.20f, RoomY, GEngine->GetMediumFont(), 1.f);
        for (const FString& Room : Rooms)
        {
            RoomY += 38.f;
            DrawText(Room, FLinearColor::White, Canvas->SizeX * 0.22f, RoomY, GEngine->GetSmallFont(), 1.f);
        }
        return;
    }

    if (!GS->bGameStarted)
    {
        DrawRect(FLinearColor(0.015f, 0.02f, 0.035f, 0.94f), Canvas->SizeX * 0.20f, Canvas->SizeY * 0.25f, Canvas->SizeX * 0.60f, Canvas->SizeY * 0.42f);
        DrawText(TEXT("STEAM 房间已连接"), FLinearColor(0.3f,0.85f,1.f), Canvas->SizeX * 0.32f, Canvas->SizeY * 0.32f, GEngine->GetLargeFont(), 1.2f);
        DrawText(FString::Printf(TEXT("当前玩家：%d / 2"), GS->PlayerArray.Num()), FLinearColor::White, Canvas->SizeX * 0.39f, Canvas->SizeY * 0.42f, GEngine->GetLargeFont(), 1.f);
        DrawText(GetNetMode() == NM_ListenServer ? TEXT("两人到齐后，房主按 P 开始") : TEXT("等待房主开始游戏……"),
            FLinearColor(0.95f,0.85f,0.25f), Canvas->SizeX * 0.34f, Canvas->SizeY * 0.52f, GEngine->GetMediumFont(), 1.f);
        return;
    }

    const bool bScout = PS->PlayerRole == ETreasurePlayerRole::Scout;
    FString RoleLabel;
    if (GetNetMode() == NM_Standalone)
        RoleLabel = bScout ? TEXT("侦察者 / 尚未联机") : TEXT("寻宝者 / 尚未联机");
    else if (GetNetMode() == NM_ListenServer)
        RoleLabel = TEXT("侦察者 / 主机");
    else
        RoleLabel = TEXT("寻宝者 / 已连接客户端");
    const FString Help = bScout
        ? TEXT("WASD 移动 | M 打开白纸画图 | C 清空 | Enter 交图")
        : TEXT("等待交图；收到后 M 查看地图 | E 挖掘");
    DrawText(FString::Printf(TEXT("%s  |  岛屿种子 %d"), *RoleLabel, GS->IslandSeed), FLinearColor::White, 35.f, 28.f, GEngine->GetLargeFont(), 1.f);
    DrawText(Help, FLinearColor(0.9f,0.9f,0.9f), 35.f, 62.f, GEngine->GetSmallFont(), 1.f);

    const int32 SecondsRemaining = GS->GetSecondsRemaining();
    const FString TimerText = FString::Printf(TEXT("%02d:%02d"), SecondsRemaining / 60, SecondsRemaining % 60);
    const FLinearColor TimerColor = SecondsRemaining <= 10 ? FLinearColor(1.f, 0.12f, 0.08f) : FLinearColor::White;
    const float TimerWidth = 150.f;
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), Canvas->SizeX - TimerWidth - 28.f, 22.f, TimerWidth, 58.f);
    DrawText(TimerText, TimerColor, Canvas->SizeX - TimerWidth - 6.f, 30.f, GEngine->GetLargeFont(), 1.25f);

    if (!PC->GetStatusMessage().IsEmpty())
        DrawText(PC->GetStatusMessage(), FLinearColor::Yellow, 35.f, Canvas->SizeY - 70.f, GEngine->GetMediumFont(), 1.f);

    if (PC->IsHunterWaiting())
    {
        DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.96f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
        DrawText(TEXT("侦察者正在探索并绘制地图……"), FLinearColor::White,
            Canvas->SizeX * 0.5f - 230.f, Canvas->SizeY * 0.48f, GEngine->GetLargeFont(), 1.f);
        DrawText(TEXT("地图交付后，你将登岛寻宝"), FLinearColor(0.7f,0.8f,0.9f),
            Canvas->SizeX * 0.5f - 190.f, Canvas->SizeY * 0.55f, GEngine->GetMediumFont(), 1.f);
        return;
    }

    if (!PC->IsMapOpen()) return;
    const FVector2D Min = PC->GetPaperMin();
    const FVector2D Size = PC->GetPaperSize();
    DrawRect(FLinearColor(0.96f, 0.94f, 0.86f, 0.98f), Min.X, Min.Y, Size.X, Size.Y);
    DrawText(bScout ? TEXT("空白纸：请画岛屿轮廓、地形地标和藏宝点") : TEXT("侦察者留下的手绘地图"),
        FLinearColor::Black, Min.X + 18.f, Min.Y + 14.f, GEngine->GetSmallFont(), 1.f);

    for (const FSketchStroke& Stroke : PC->GetStrokes())
    {
        for (int32 I = 1; I < Stroke.Points.Num(); ++I)
        {
            const FVector2D A = Min + Stroke.Points[I-1] * Size;
            const FVector2D B = Min + Stroke.Points[I] * Size;
            DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(0.08f,0.07f,0.05f), 4.f);
        }
    }
}
