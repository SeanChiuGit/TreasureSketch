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

    if (PC->IsFrontEndVisible())
    {
        const float W = Canvas->SizeX;
        const float H = Canvas->SizeY;
        const float PanelX = W * 0.075f;
        const float PanelY = H * 0.10f;
        const float PanelW = FMath::Clamp(W * 0.38f, 440.f, 650.f);
        const float PanelH = H * 0.80f;
        DrawRect(FLinearColor(0.018f, 0.035f, 0.045f, 0.38f), 0.f, 0.f, W, H);
        DrawRect(FLinearColor(0.035f, 0.075f, 0.085f, 0.91f), PanelX, PanelY, PanelW, PanelH);
        DrawRect(FLinearColor(0.88f, 0.66f, 0.25f, 0.95f), PanelX, PanelY, 7.f, PanelH);

        DrawText(TEXT("TREASURE SKETCH"), FLinearColor(0.96f, 0.79f, 0.40f),
            PanelX + 46.f, PanelY + 42.f, GEngine->GetLargeFont(), 1.45f);
        DrawText(TEXT("画下岛屿，交出地图，一起找到宝藏"), FLinearColor(0.80f, 0.90f, 0.88f),
            PanelX + 48.f, PanelY + 92.f, GEngine->GetSmallFont(), 1.f);

        auto DrawMenuButton = [&](FName Name, const FString& Label, float Y, bool bPrimary = false)
        {
            const FVector2D Min(PanelX + 48.f, Y);
            const FVector2D Size(PanelW - 96.f, 54.f);
            const bool bHovered = HitBoxesOver.Contains(Name);
            const FLinearColor Normal = bPrimary ? FLinearColor(0.72f, 0.43f, 0.16f, 0.98f)
                : FLinearColor(0.09f, 0.17f, 0.18f, 0.96f);
            const FLinearColor Hover = bPrimary ? FLinearColor(0.93f, 0.61f, 0.22f, 1.f)
                : FLinearColor(0.15f, 0.30f, 0.30f, 1.f);
            DrawRect(bHovered ? Hover : Normal, Min.X, Min.Y, Size.X, Size.Y);
            float TextW = 0.f, TextH = 0.f;
            GetTextSize(Label, TextW, TextH, GEngine->GetMediumFont(), 1.f);
            DrawText(Label, FLinearColor::White, Min.X + 22.f, Min.Y + (Size.Y - TextH) * 0.5f,
                GEngine->GetMediumFont(), 1.f);
            AddHitBox(Min, Size, Name, true, 10);
        };

        const EFrontEndPage Page = PC->GetFrontEndPage();
        if (Page == EFrontEndPage::MainMenu)
        {
            float Y = PanelY + 155.f;
            DrawMenuButton(TEXT("MenuCreate"), TEXT("创建房间"), Y, true); Y += 68.f;
            DrawMenuButton(TEXT("MenuJoin"), TEXT("加入房间"), Y); Y += 68.f;
            DrawMenuButton(TEXT("MenuSolo"), TEXT("单人探险"), Y); Y += 68.f;
            DrawMenuButton(TEXT("MenuSettings"), TEXT("设置"), Y); Y += 68.f;
            DrawMenuButton(TEXT("MenuQuit"), TEXT("退出游戏"), Y);
            DrawText(TEXT("双人合作寻宝原型"), FLinearColor(0.55f, 0.70f, 0.68f),
                PanelX + 48.f, PanelY + PanelH - 45.f, GEngine->GetSmallFont(), 0.9f);
        }
        else if (Page == EFrontEndPage::SoloTest)
        {
            DrawText(TEXT("单人测试"), FLinearColor::White, PanelX + 48.f, PanelY + 150.f,
                GEngine->GetLargeFont(), 1.25f);
            DrawText(TEXT("地图测试显示宝藏；玩法测试：空白图、隐藏宝藏、120秒"),
                FLinearColor(0.75f, 0.84f, 0.82f), PanelX + 48.f, PanelY + 205.f,
                GEngine->GetSmallFont(), 0.9f);
            DrawMenuButton(TEXT("SoloRuins"), TEXT("测试遗迹岛"), PanelY + 255.f, true);
            DrawMenuButton(TEXT("SoloBeach"), TEXT("测试海盗沙滩岛"), PanelY + 323.f);
            DrawMenuButton(TEXT("SoloRandom"), TEXT("随机主题与新种子"), PanelY + 391.f);
            DrawMenuButton(TEXT("SoloHunter"), TEXT("探索者玩法测试"), PanelY + 459.f, true);
            DrawMenuButton(TEXT("MenuBack"), TEXT("返回主页面"), PanelY + 527.f);
        }
        else if (Page == EFrontEndPage::Settings)
        {
            DrawText(TEXT("设置"), FLinearColor::White, PanelX + 48.f, PanelY + 185.f,
                GEngine->GetLargeFont(), 1.25f);
            DrawText(TEXT("设置页面仍在开发中"), FLinearColor(0.75f, 0.84f, 0.82f), PanelX + 48.f, PanelY + 250.f,
                GEngine->GetMediumFont(), 1.f);
            DrawMenuButton(TEXT("MenuBack"), TEXT("返回主页面"), PanelY + 330.f, true);
        }
        else if (Page == EFrontEndPage::JoinBrowser)
        {
            const UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>();
            DrawText(TEXT("加入房间"), FLinearColor::White, PanelX + 48.f, PanelY + 150.f,
                GEngine->GetLargeFont(), 1.2f);
            DrawText(Online ? Online->GetStatus() : TEXT("Steam 在线服务未初始化"),
                FLinearColor(0.96f, 0.79f, 0.40f), PanelX + 48.f, PanelY + 205.f,
                GEngine->GetSmallFont(), 0.9f);
            float RoomY = PanelY + 250.f;
            if (Online && Online->GetRoomLines().Num() > 0)
            {
                for (const FString& Room : Online->GetRoomLines())
                {
                    DrawText(Room, FLinearColor(0.86f, 0.92f, 0.90f), PanelX + 48.f, RoomY,
                        GEngine->GetSmallFont(), 0.82f);
                    RoomY += 27.f;
                    if (RoomY > PanelY + 390.f) break;
                }
                DrawMenuButton(TEXT("MenuJoinFirst"), TEXT("加入第一个房间"), PanelY + 420.f, true);
            }
            else
            {
                DrawText(TEXT("正在搜索，或暂时没有可加入房间"), FLinearColor(0.70f, 0.80f, 0.78f),
                    PanelX + 48.f, RoomY, GEngine->GetSmallFont(), 0.9f);
            }
            DrawMenuButton(TEXT("MenuBack"), TEXT("返回主页面"), PanelY + 500.f);
        }
        else if (Page == EFrontEndPage::RoomLobby)
        {
            const UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>();
            DrawText(TEXT("双人房间"), FLinearColor::White, PanelX + 48.f, PanelY + 150.f,
                GEngine->GetLargeFont(), 1.2f);
            DrawText(FString::Printf(TEXT("玩家  %d / 2"), GS->PlayerArray.Num()),
                FLinearColor(0.86f, 0.92f, 0.90f), PanelX + 48.f, PanelY + 215.f,
                GEngine->GetLargeFont(), 1.f);
            DrawText(Online ? Online->GetStatus() : TEXT("正在建立 Steam 房间……"),
                FLinearColor(0.96f, 0.79f, 0.40f), PanelX + 48.f, PanelY + 265.f,
                GEngine->GetSmallFont(), 0.9f);
            if (GetNetMode() == NM_ListenServer)
            {
                DrawMenuButton(TEXT("RoomInvite"), TEXT("邀请 Steam 好友"), PanelY + 335.f, true);
                DrawMenuButton(TEXT("RoomStart"), TEXT("开始游戏"), PanelY + 405.f,
                    GS->PlayerArray.Num() >= 2);
                DrawText(GS->PlayerArray.Num() >= 2 ? TEXT("两名玩家已到齐") : TEXT("等待另一名玩家加入……"),
                    FLinearColor(0.70f, 0.82f, 0.78f), PanelX + 48.f, PanelY + 478.f,
                    GEngine->GetSmallFont(), 0.9f);
            }
            else
            {
                DrawText(TEXT("已加入房间，等待房主开始游戏……"), FLinearColor(0.70f, 0.82f, 0.78f),
                    PanelX + 48.f, PanelY + 345.f, GEngine->GetMediumFont(), 0.9f);
            }
            DrawMenuButton(TEXT("RoomBack"), TEXT("离开房间并返回主菜单"), PanelY + 545.f);
        }
        return;
    }

    if (GetNetMode() == NM_Standalone && !GS->bGameStarted)
    {
        const UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>();
        const FString OnlineText = Online ? Online->GetStatus() : TEXT("Steam 在线服务未初始化");
        DrawRect(FLinearColor(0.015f, 0.02f, 0.035f, 0.94f), Canvas->SizeX * 0.16f, Canvas->SizeY * 0.16f, Canvas->SizeX * 0.68f, Canvas->SizeY * 0.64f);
        DrawText(TEXT("TREASURESKETCH STEAM 大厅"), FLinearColor(0.3f,0.85f,1.f), Canvas->SizeX * 0.20f, Canvas->SizeY * 0.21f, GEngine->GetLargeFont(), 1.15f);
        DrawText(TEXT("H 创建房间    I 邀请 Steam 好友    J 搜索    K 加入列表第一个房间"), FLinearColor::White, Canvas->SizeX * 0.20f, Canvas->SizeY * 0.28f, GEngine->GetMediumFont(), 1.f);
        DrawText(OnlineText, FLinearColor(0.95f,0.85f,0.25f), Canvas->SizeX * 0.20f, Canvas->SizeY * 0.34f, GEngine->GetSmallFont(), 1.f);
        float RoomY = Canvas->SizeY * 0.41f;
        const TArray<FString>& Rooms = Online->GetRoomLines();
        DrawText(Rooms.IsEmpty() ? TEXT("房间列表为空") : TEXT("可加入房间："), FLinearColor(0.75f,0.8f,0.9f), Canvas->SizeX * 0.20f, RoomY, GEngine->GetMediumFont(), 1.f);
        for (const FString& Room : Rooms)
        {
            RoomY += 38.f;
            DrawText(Room, FLinearColor::White, Canvas->SizeX * 0.22f, RoomY, GEngine->GetSmallFont(), 1.f);
        }
        RoomY += 52.f;
        DrawText(TEXT("联机诊断："), FLinearColor(0.45f,0.9f,0.65f), Canvas->SizeX * 0.20f, RoomY, GEngine->GetSmallFont(), 1.f);
        for (const FString& Line : Online->GetDiagnostics())
        {
            RoomY += 23.f;
            DrawText(Line, FLinearColor(0.78f,0.82f,0.88f), Canvas->SizeX * 0.21f, RoomY, GEngine->GetSmallFont(), 0.82f);
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
        if (const UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>())
        {
            float DiagY = Canvas->SizeY * 0.58f;
            if (GetNetMode() == NM_ListenServer)
                DrawText(TEXT("按 I 打开 Steam 好友邀请"), FLinearColor(0.45f,0.9f,0.65f), Canvas->SizeX * 0.36f, DiagY, GEngine->GetSmallFont(), 1.f);
            for (const FString& Line : Online->GetDiagnostics())
            {
                DiagY += 21.f;
                DrawText(Line, FLinearColor(0.72f,0.77f,0.85f), Canvas->SizeX * 0.24f, DiagY, GEngine->GetSmallFont(), 0.78f);
                if (DiagY > Canvas->SizeY * 0.78f) break;
            }
        }
        return;
    }

    if (GS->IsRoundOver())
    {
        const bool bWon = GS->Phase == ETreasureRoundPhase::Won;
        const float CenterX = Canvas->SizeX * 0.5f;
        const float CenterY = Canvas->SizeY * 0.5f;
        const float PanelWidth = FMath::Min(620.f, Canvas->SizeX * 0.82f);
        const float PanelHeight = 380.f;
        const float ButtonWidth = FMath::Min(360.f, PanelWidth - 48.f);
        const float ButtonHeight = 56.f;
        const FVector2D SameRolesButtonMin(CenterX - ButtonWidth * 0.5f, CenterY + 8.f);
        const FVector2D SwapRolesButtonMin(CenterX - ButtonWidth * 0.5f, CenterY + 78.f);
        const FName SameRolesButtonName(TEXT("ReplaySameRoles"));
        const FName SwapRolesButtonName(TEXT("ReplaySwapRoles"));

        DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.94f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
        DrawRect(FLinearColor(0.07f, 0.11f, 0.14f, 1.f), CenterX - PanelWidth * 0.5f,
            CenterY - PanelHeight * 0.5f, PanelWidth, PanelHeight);

        const FString Title = bWon ? TEXT("合作成功！") : TEXT("时间到！");
        float TextWidth = 0.f, TextHeight = 0.f;
        GetTextSize(Title, TextWidth, TextHeight, GEngine->GetLargeFont(), 1.35f);
        DrawText(Title, bWon ? FLinearColor(0.95f, 0.85f, 0.25f) : FLinearColor(1.f, 0.35f, 0.25f),
            CenterX - TextWidth * 0.5f,
            CenterY - 140.f, GEngine->GetLargeFont(), 1.35f);

        const FString Hint = bWon ? TEXT("找到宝藏了！再来一座新岛屿？")
            : GS->Phase == ETreasureRoundPhase::ScoutTimedOut ? TEXT("侦察者未能及时交图，再试一次？")
            : TEXT("寻宝者未能及时找到宝藏，再试一次？");
        GetTextSize(Hint, TextWidth, TextHeight, GEngine->GetMediumFont(), 1.f);
        DrawText(Hint, FLinearColor::White, CenterX - TextWidth * 0.5f,
            CenterY - 72.f, GEngine->GetMediumFont(), 1.f);

        const bool bSolo = GS->PlayerArray.Num() == 1;
        const FString ChoiceHint = bSolo ? TEXT("单人测试：再玩一次会开始新的岛屿")
            : TEXT("任一人选择后，双方立即开始新的一局");
        GetTextSize(ChoiceHint, TextWidth, TextHeight, GEngine->GetSmallFont(), 1.f);
        DrawText(ChoiceHint, FLinearColor(0.7f, 0.8f, 0.9f), CenterX - TextWidth * 0.5f,
            CenterY - 30.f, GEngine->GetSmallFont(), 1.f);

        const bool bSameRolesHovered = HitBoxesOver.Contains(SameRolesButtonName);
        DrawRect(bSameRolesHovered ? FLinearColor(0.24f, 0.69f, 0.52f) : FLinearColor(0.16f, 0.52f, 0.40f),
            SameRolesButtonMin.X, SameRolesButtonMin.Y, ButtonWidth, ButtonHeight);
        const FString SameRolesText = TEXT("保持角色，再玩一次");
        GetTextSize(SameRolesText, TextWidth, TextHeight, GEngine->GetLargeFont(), 1.f);
        DrawText(SameRolesText, FLinearColor::White, CenterX - TextWidth * 0.5f,
            SameRolesButtonMin.Y + (ButtonHeight - TextHeight) * 0.5f, GEngine->GetLargeFont(), 1.f);
        AddHitBox(SameRolesButtonMin, FVector2D(ButtonWidth, ButtonHeight), SameRolesButtonName, true, 0);

        if (bSolo) return;
        const bool bSwapRolesHovered = HitBoxesOver.Contains(SwapRolesButtonName);
        DrawRect(bSwapRolesHovered ? FLinearColor(0.30f, 0.57f, 0.82f) : FLinearColor(0.20f, 0.42f, 0.67f),
            SwapRolesButtonMin.X, SwapRolesButtonMin.Y, ButtonWidth, ButtonHeight);
        const FString SwapRolesText = TEXT("交换角色，再玩一次");
        GetTextSize(SwapRolesText, TextWidth, TextHeight, GEngine->GetLargeFont(), 1.f);
        DrawText(SwapRolesText, FLinearColor::White, CenterX - TextWidth * 0.5f,
            SwapRolesButtonMin.Y + (ButtonHeight - TextHeight) * 0.5f, GEngine->GetLargeFont(), 1.f);
        AddHitBox(SwapRolesButtonMin, FVector2D(ButtonWidth, ButtonHeight), SwapRolesButtonName, true, 0);
        return;
    }

    const bool bScout = PS->PlayerRole == ETreasurePlayerRole::Scout;
    const FString RoleLabel = FString::Printf(TEXT("%s / %s"),
        bScout ? TEXT("侦察者") : TEXT("寻宝者"),
        GetNetMode() == NM_ListenServer ? TEXT("主机") : TEXT("已连接客户端"));
    const FString Help = PC->IsScoutSpectating()
        ? TEXT("观战：WASD 飞行 | Space 上升 | Ctrl 下降 | Shift 加速")
        : bScout ? TEXT("WASD 移动 | M 打开白纸画图 | C 清空 | Enter 交图")
        : TEXT("等待交图；收到后 M 查看地图 | E 挖掘");
    DrawText(FString::Printf(TEXT("%s  |  岛屿种子 %d"), *RoleLabel, GS->IslandSeed), FLinearColor::White, 35.f, 28.f, GEngine->GetLargeFont(), 1.f);
    DrawText(Help, FLinearColor(0.9f,0.9f,0.9f), 35.f, 62.f, GEngine->GetSmallFont(), 1.f);
    if (PC->IsScoutSpectating())
    {
        DrawText(TEXT("鼠标转向 | Tab 切换视角 | T 显示/隐藏宝藏"),
            FLinearColor(0.9f, 0.9f, 0.9f), 35.f, 86.f, GEngine->GetSmallFont(), 1.f);
        DrawText(FString::Printf(TEXT("%s  |  宝藏标记：%s"),
            PC->IsHunterFirstPersonView() ? TEXT("寻宝者第一视角") : TEXT("自由飞行"),
            PC->IsSpectatorTreasureVisible() ? TEXT("显示") : TEXT("隐藏")),
            FLinearColor(0.45f, 0.9f, 0.85f), 35.f, 110.f, GEngine->GetSmallFont(), 1.f);
    }

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

void ATreasureSketchHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);
    if (BoxName == FName(TEXT("ReplaySameRoles")) || BoxName == FName(TEXT("ReplaySwapRoles")))
    {
        if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PlayerOwner))
            PC->RequestReplay(BoxName == FName(TEXT("ReplaySwapRoles")));
    }
    else if (ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PlayerOwner))
    {
        PC->HandleFrontEndAction(BoxName);
    }
}
