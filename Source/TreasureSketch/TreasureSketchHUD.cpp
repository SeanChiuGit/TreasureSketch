#include "TreasureSketchHUD.h"
#include "TreasureSurfacePaint.h"

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

        auto DrawDifficultySettings = [&](float SettingsX, float SettingsY, float SettingsW, bool bCanAdjustSettings)
        {
            auto DrawRoomSetting = [&](const FString& Label, float Y, FName Less, FName More, bool bCanLess, bool bCanMore)
            {
                DrawRect(FLinearColor(0.06f, 0.12f, 0.13f), SettingsX, Y, SettingsW, 32.f);
                DrawText(Label, FLinearColor::White, SettingsX + 10.f, Y + 9.f, GEngine->GetSmallFont(), 0.9f);
                const float ButtonX = SettingsX + SettingsW - 84.f;
                for (int32 I = 0; I < 2; ++I)
                {
                    const bool bEnabled = bCanAdjustSettings && (I == 0 ? bCanLess : bCanMore);
                    const float X = ButtonX + I * 42.f;
                    DrawRect(bEnabled ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.08f, 0.12f, 0.13f), X, Y + 2.f, 38.f, 28.f);
                    DrawText(I == 0 ? TEXT("-") : TEXT("+"), bEnabled ? FLinearColor::White : FLinearColor(0.35f, 0.40f, 0.40f), X + 13.f, Y + 5.f, GEngine->GetMediumFont(), 1.f);
                    if (bEnabled) AddHitBox(FVector2D(X, Y + 2.f), FVector2D(38.f, 28.f), I == 0 ? Less : More, true, 10);
                }
            };
            DrawRoomSetting(TEXT("面积倍数："), SettingsY, TEXT("RoomMapSmaller"), TEXT("RoomMapLarger"), GS->RoomMapScale > GS->MinMapScale, GS->RoomMapScale < GS->MaxMapScale);
            const float InputX = SettingsX + 110.f;
            const float InputW = SettingsW - 202.f;
            const bool bEditingScale = PC->IsMapScaleEditing();
            const FString ScaleText = bEditingScale ? PC->GetMapScaleText() : FString::Printf(TEXT("%.6g"), GS->RoomMapScale);
            float ParsedScale = 0.f;
            const bool bValidScale = LexTryParseString(ParsedScale, *ScaleText) && FMath::IsFinite(ParsedScale)
                && ParsedScale >= GS->MinMapScale && ParsedScale <= GS->MaxMapScale;
            DrawRect(bEditingScale ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.08f, 0.12f, 0.13f), InputX, SettingsY + 2.f, InputW, 28.f);
            DrawText(ScaleText + (bEditingScale ? TEXT("_") : TEXT(" 倍")), bValidScale ? FLinearColor::White : FLinearColor(1.f, 0.4f, 0.3f),
                InputX + 8.f, SettingsY + 9.f, GEngine->GetSmallFont(), 0.9f);
            if (bCanAdjustSettings) AddHitBox(FVector2D(InputX, SettingsY + 2.f), FVector2D(InputW, 28.f), TEXT("MapScaleInput"), true, 10);
            DrawRoomSetting(FString::Printf(TEXT("绘图时间：%d 秒"), GS->DrawingDurationSeconds), SettingsY + 36.f, TEXT("RoomDrawingLess"), TEXT("RoomDrawingMore"), GS->DrawingDurationSeconds > GS->MinPhaseSeconds, GS->DrawingDurationSeconds < GS->MaxPhaseSeconds);
            DrawRoomSetting(FString::Printf(TEXT("寻宝时间：%d 秒"), GS->SearchingDurationSeconds), SettingsY + 72.f, TEXT("RoomSearchingLess"), TEXT("RoomSearchingMore"), GS->SearchingDurationSeconds > GS->MinPhaseSeconds, GS->SearchingDurationSeconds < GS->MaxPhaseSeconds);
            DrawRoomSetting(FString::Printf(TEXT("移动速度：%.2f 倍"), GS->MovementSpeedMultiplier), SettingsY + 108.f, TEXT("RoomSpeedLess"), TEXT("RoomSpeedMore"), GS->MovementSpeedMultiplier > GS->MinMovementSpeed, GS->MovementSpeedMultiplier < GS->MaxMovementSpeed);
            auto DrawRoomToggle = [&](FName Name, const FString& Label, float Y)
            {
                DrawRect(bCanAdjustSettings ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.08f, 0.12f, 0.13f), SettingsX, Y, SettingsW, 32.f);
                DrawText(Label, FLinearColor::White, SettingsX + 10.f, Y + 9.f, GEngine->GetSmallFont(), 0.9f);
                if (bCanAdjustSettings) AddHitBox(FVector2D(SettingsX, Y), FVector2D(SettingsW, 32.f), Name, true, 10);
            };
            DrawRoomToggle(TEXT("ToggleTreasureRange"), GS->bTreasureRangeVisible ? TEXT("宝藏判定范围：显示（点击切换）") : TEXT("宝藏判定范围：隐藏（点击切换）"), SettingsY + 144.f);
            DrawRoomToggle(TEXT("ToggleSpreadPlayerSpawns"), GS->bSpreadPlayerSpawns ? TEXT("出生点：分散登岛（点击切换）") : TEXT("出生点：同一区域（点击切换）"), SettingsY + 180.f);
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
            DrawText(TEXT("2至4人合作寻宝"), FLinearColor(0.55f, 0.70f, 0.68f),
                PanelX + 48.f, PanelY + PanelH - 45.f, GEngine->GetSmallFont(), 0.9f);
        }
        else if (Page == EFrontEndPage::SoloTest)
        {
            DrawText(TEXT("单人测试"), FLinearColor::White, PanelX + 48.f, PanelY + 150.f,
                GEngine->GetLargeFont(), 1.25f);
            DrawText(TEXT("地图测试显示宝藏；玩法测试可调大小和时间"),
                FLinearColor(0.75f, 0.84f, 0.82f), PanelX + 48.f, PanelY + 205.f,
                GEngine->GetSmallFont(), 0.9f);
            DrawMenuButton(TEXT("SoloForest"), TEXT("测试雾森林"), PanelY + 225.f, true);
            DrawMenuButton(TEXT("SoloBeach"), TEXT("测试海盗沙滩岛"), PanelY + 285.f);
            DrawMenuButton(TEXT("SoloRandom"), TEXT("随机主题与新种子"), PanelY + 345.f);
            DrawMenuButton(TEXT("SoloHunter"), TEXT("探索者玩法测试"), PanelY + 405.f, true);
            DrawMenuButton(TEXT("SoloFullFlow"), TEXT("完整流程测试：自己画，自己找"), PanelY + 465.f, true);
            DrawMenuButton(TEXT("MenuBack"), TEXT("返回主页面"), PanelY + 525.f);
            const float SeedX = PanelX + PanelW + 28.f;
            const float SeedW = FMath::Max(180.f, FMath::Min(390.f, W - SeedX - 24.f));
            const float SeedY = PanelY + 150.f;
            DrawText(TEXT("玩法测试种子（可留空）"), FLinearColor::White, SeedX, SeedY,
                GEngine->GetMediumFont(), 1.f);
            DrawRect(PC->IsTestSeedEditing() ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.06f, 0.12f, 0.13f),
                SeedX, SeedY + 35.f, SeedW, 44.f);
            const FString SeedText = PC->GetTestSeedText();
            DrawText(SeedText.IsEmpty() ? TEXT("点击输入；留空随机") : SeedText,
                FLinearColor::White, SeedX + 12.f, SeedY + 47.f, GEngine->GetMediumFont(), 1.f);
            AddHitBox(FVector2D(SeedX, SeedY + 35.f), FVector2D(SeedW, 44.f), TEXT("TestSeedInput"), true, 10);
            DrawText(TEXT("数字键输入，退格删除，Enter完成"), FLinearColor(0.75f, 0.84f, 0.82f),
                SeedX, SeedY + 90.f, GEngine->GetSmallFont(), 0.9f);
            DrawText(TEXT("面积0.5至5倍；点击输入，Enter确认"), FLinearColor(0.75f, 0.84f, 0.82f),
                SeedX, SeedY + 114.f, GEngine->GetSmallFont(), 0.9f);
            DrawRect(FLinearColor(0.09f, 0.17f, 0.18f), SeedX, SeedY + 140.f, 130.f, 32.f);
            DrawText(TEXT("清空种子"), FLinearColor::White, SeedX + 12.f, SeedY + 149.f, GEngine->GetMediumFont(), 1.f);
            AddHitBox(FVector2D(SeedX, SeedY + 140.f), FVector2D(130.f, 32.f), TEXT("TestSeedClear"), true, 10);
            DrawDifficultySettings(SeedX, SeedY + 182.f, SeedW, true);
            DrawRect(GS->bSurfacePaintEnabled ? FLinearColor(0.12f, 0.38f, 0.60f) : FLinearColor(0.09f, 0.17f, 0.18f), SeedX, SeedY + 402.f, SeedW, 36.f);
            DrawText(GS->bSurfacePaintEnabled ? TEXT("实验喷漆：开启（点击关闭）") : TEXT("实验喷漆：关闭（点击开启）"), FLinearColor::White, SeedX + 10.f, SeedY + 413.f, GEngine->GetSmallFont(), 0.9f);
            AddHitBox(FVector2D(SeedX, SeedY + 402.f), FVector2D(SeedW, 36.f), TEXT("ToggleSurfacePaint"), true, 10);
            DrawText(TEXT("完整流程测试：转动视角瞄准，按住右键喷漆"), FLinearColor::White, SeedX, SeedY + 444.f, GEngine->GetSmallFont(), 0.85f);
            if (!SeedText.IsEmpty() && PC->GetTestSeed() == 0)
                DrawText(TEXT("请输入1至2147483647，或清空以随机"), FLinearColor(1.f, 0.4f, 0.3f),
                    SeedX, SeedY + 470.f, GEngine->GetSmallFont(), 0.9f);
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
            const float ModeX = PanelX + PanelW + 28.f;
            const float ModeW = FMath::Max(180.f, FMath::Min(410.f, W - ModeX - 24.f));
            const bool bHost = GetNetMode() == NM_ListenServer;
            DrawRect(GS->bSurfacePaintEnabled ? FLinearColor(0.12f, 0.38f, 0.60f) : FLinearColor(0.09f, 0.17f, 0.18f), ModeX, PanelY + 510.f, ModeW, 36.f);
            DrawText(GS->bSurfacePaintEnabled ? TEXT("实验喷漆：开启") : TEXT("实验喷漆：关闭"), FLinearColor::White, ModeX + 10.f, PanelY + 521.f, GEngine->GetSmallFont(), 0.9f);
            if (bHost)
                AddHitBox(FVector2D(ModeX, PanelY + 510.f), FVector2D(ModeW, 36.f), TEXT("ToggleSurfacePaint"), true, 10);
            DrawText(TEXT("游戏模式"), FLinearColor::White, ModeX, PanelY + 150.f, GEngine->GetMediumFont());
            const TCHAR* Modes[] = { TEXT("一名地图师，多名探索者"), TEXT("多名地图师，一名探索者"), TEXT("2对2 对抗（待开发）") };
            for (int32 ModeIndex = 0; ModeIndex < 3; ++ModeIndex)
            {
                const float ModeY = PanelY + 180.f + ModeIndex * 34.f;
                const bool bSelected = ModeIndex == static_cast<int32>(GS->RoomMode);
                DrawRect(bSelected ? FLinearColor(0.72f, 0.43f, 0.16f) : FLinearColor(0.08f, 0.12f, 0.13f), ModeX, ModeY, ModeW, 30.f);
                DrawText(FString(Modes[ModeIndex]) + (bSelected ? TEXT("（已选择）") : TEXT("")), ModeIndex < 2 ? FLinearColor::White : FLinearColor(0.45f, 0.5f, 0.5f), ModeX + 10.f, ModeY + 8.f, GEngine->GetSmallFont(), 0.9f);
                if (bHost && ModeIndex < 2)
                    AddHitBox(FVector2D(ModeX, ModeY), FVector2D(ModeW, 30.f), ModeIndex == 0 ? TEXT("RoomModeCoop") : TEXT("RoomModeOneExplorer"), true, 10);
            }
            DrawDifficultySettings(ModeX, PanelY + 290.f, ModeW, bHost);
            float PlayerY = PanelY + 564.f;
            DrawText(bHost ? TEXT("面积0.5至5倍，Enter确认；重玩沿用") : TEXT("房主调整设置；开局生效"), FLinearColor(0.75f, 0.84f, 0.82f), ModeX, PanelY + 548.f, GEngine->GetSmallFont(), 0.85f);
            for (APlayerState* State : GS->PlayerArray)
                if (const ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
                {
                    DrawText(FString::Printf(TEXT("%s — %s"), *Member->GetPlayerName(), Member->PlayerRole == ETreasurePlayerRole::Scout ? TEXT("地图师") : TEXT("探索者")), FLinearColor::White, ModeX, PlayerY, GEngine->GetSmallFont(), 0.9f);
                    PlayerY += 22.f;
                }
            DrawText(TEXT("合作房间"), FLinearColor::White, PanelX + 48.f, PanelY + 150.f,
                GEngine->GetLargeFont(), 1.2f);
            DrawText(FString::Printf(TEXT("玩家  %d / 4"), GS->PlayerArray.Num()),
                FLinearColor(0.86f, 0.92f, 0.90f), PanelX + 48.f, PanelY + 215.f,
                GEngine->GetLargeFont(), 1.f);
            DrawText(Online ? Online->GetStatus() : TEXT("正在建立 Steam 房间……"),
                FLinearColor(0.96f, 0.79f, 0.40f), PanelX + 48.f, PanelY + 265.f,
                GEngine->GetSmallFont(), 0.9f);
            const float PoolX = PanelX + 48.f;
            const float PoolY = PanelY + 294.f;
            const float PoolButtonW = (PanelW - 181.f) * 0.5f;
            DrawText(TEXT("地图池"), FLinearColor::White, PoolX, PoolY + 8.f, GEngine->GetSmallFont(), 0.9f);
            for (int32 PoolIndex = 0; PoolIndex < 2; ++PoolIndex)
            {
                const bool bSelected = PoolIndex == 0 ? GS->bBeachInMapPool : GS->bForestInMapPool;
                const bool bOtherSelected = PoolIndex == 0 ? GS->bForestInMapPool : GS->bBeachInMapPool;
                const float ButtonX = PoolX + 75.f + PoolIndex * (PoolButtonW + 10.f);
                DrawRect(bSelected ? FLinearColor(0.53f, 0.36f, 0.14f) : FLinearColor(0.08f, 0.12f, 0.13f),
                    ButtonX, PoolY, PoolButtonW, 30.f);
                DrawText(PoolIndex == 0 ? (bSelected ? TEXT("沙滩 ✓") : TEXT("沙滩 ○"))
                    : (bSelected ? TEXT("森林 ✓") : TEXT("森林 ○")),
                    FLinearColor::White, ButtonX + 9.f, PoolY + 8.f, GEngine->GetSmallFont(), 0.9f);
                if (bHost && (bOtherSelected || !bSelected))
                    AddHitBox(FVector2D(ButtonX, PoolY), FVector2D(PoolButtonW, 30.f),
                        PoolIndex == 0 ? TEXT("RoomPoolBeach") : TEXT("RoomPoolForest"), true, 10);
            }
            if (GetNetMode() == NM_ListenServer)
            {
                DrawMenuButton(TEXT("RoomInvite"), TEXT("邀请 Steam 好友"), PanelY + 335.f, true);
                DrawMenuButton(TEXT("RoomStart"), TEXT("开始游戏"), PanelY + 405.f,
                    GS->PlayerArray.Num() >= 2);
                DrawText(GS->PlayerArray.Num() >= 2 ? TEXT("已有至少两人，可以开始（最多四人）") : TEXT("等待另一名玩家加入……"),
                    FLinearColor(0.70f, 0.82f, 0.78f), PanelX + 48.f, PanelY + 478.f,
                    GEngine->GetSmallFont(), 0.9f);
            }
            else
            {
                DrawText(TEXT("已加入房间，等待房主开始游戏……"), FLinearColor(0.70f, 0.82f, 0.78f),
                    PanelX + 48.f, PanelY + 345.f, GEngine->GetMediumFont(), 0.9f);
            }
            DrawMenuButton(TEXT("RoomBack"), TEXT("离开房间并返回主菜单"), PanelY + PanelH - 64.f);
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
        DrawText(FString::Printf(TEXT("当前玩家：%d / 4"), GS->PlayerArray.Num()), FLinearColor::White, Canvas->SizeX * 0.39f, Canvas->SizeY * 0.42f, GEngine->GetLargeFont(), 1.f);
        DrawText(GetNetMode() == NM_ListenServer ? TEXT("至少两人时，房主按 P 开始") : TEXT("等待房主开始游戏……"),
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

    if (GS->IsRoundOver() && !GS->bReviewingRound)
    {
        const bool bWon = GS->Phase == ETreasureRoundPhase::Won;
        const float CenterX = Canvas->SizeX * 0.5f;
        const float CenterY = Canvas->SizeY * 0.5f;
        const float PanelWidth = FMath::Min(620.f, Canvas->SizeX * 0.82f);
        const float PanelHeight = 510.f;
        const float ButtonWidth = FMath::Min(360.f, PanelWidth - 48.f);
        const float ButtonHeight = 56.f;
        const FVector2D SameRolesButtonMin(CenterX - ButtonWidth * 0.5f, CenterY + 8.f);
        const FVector2D SwapRolesButtonMin(CenterX - ButtonWidth * 0.5f, CenterY + 78.f);
        const FVector2D ReviewButtonMin(CenterX - ButtonWidth * 0.5f, CenterY + (GS->PlayerArray.Num() == 1 ? 78.f : 142.f));
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
            CenterY - 185.f, GEngine->GetLargeFont(), 1.35f);

        const FString Hint = bWon ? TEXT("找到宝藏了！再来一座新岛屿？")
            : GS->Phase == ETreasureRoundPhase::ScoutTimedOut ? TEXT("侦察者未能及时交图，再试一次？")
            : TEXT("寻宝者未能及时找到宝藏，再试一次？");
        GetTextSize(Hint, TextWidth, TextHeight, GEngine->GetMediumFont(), 1.f);
        DrawText(Hint, FLinearColor::White, CenterX - TextWidth * 0.5f,
            CenterY - 117.f, GEngine->GetMediumFont(), 1.f);

        const bool bSolo = GS->PlayerArray.Num() == 1;
        const FString ChoiceHint = bSolo ? TEXT("单人测试：再玩一次会开始新的岛屿")
            : TEXT("任一人选择后，全队立即开始新的一局");
        GetTextSize(ChoiceHint, TextWidth, TextHeight, GEngine->GetSmallFont(), 1.f);
        DrawText(ChoiceHint, FLinearColor(0.7f, 0.8f, 0.9f), CenterX - TextWidth * 0.5f,
            CenterY - 75.f, GEngine->GetSmallFont(), 1.f);

        const bool bSameRolesHovered = HitBoxesOver.Contains(SameRolesButtonName);
        DrawRect(bSameRolesHovered ? FLinearColor(0.24f, 0.69f, 0.52f) : FLinearColor(0.16f, 0.52f, 0.40f),
            SameRolesButtonMin.X, SameRolesButtonMin.Y, ButtonWidth, ButtonHeight);
        const FString SameRolesText = TEXT("保持角色，再玩一次");
        GetTextSize(SameRolesText, TextWidth, TextHeight, GEngine->GetLargeFont(), 1.f);
        DrawText(SameRolesText, FLinearColor::White, CenterX - TextWidth * 0.5f,
            SameRolesButtonMin.Y + (ButtonHeight - TextHeight) * 0.5f, GEngine->GetLargeFont(), 1.f);
        AddHitBox(SameRolesButtonMin, FVector2D(ButtonWidth, ButtonHeight), SameRolesButtonName, true, 0);

        if (GetNetMode() != NM_Client)
        {
            const FVector2D SetupMin(CenterX - ButtonWidth * 0.5f, CenterY + (bSolo ? 142.f : 207.f));
            DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), SetupMin.X, SetupMin.Y, ButtonWidth, 40.f);
            const FString Label = bSolo ? TEXT("返回测试菜单") : TEXT("全队返回大厅");
            GetTextSize(Label, TextWidth, TextHeight, GEngine->GetSmallFont(), 1.f);
            DrawText(Label, FLinearColor::White, CenterX - TextWidth * 0.5f, SetupMin.Y + 12.f, GEngine->GetSmallFont(), 1.f);
            AddHitBox(SetupMin, FVector2D(ButtonWidth, 40.f), TEXT("ReturnToSetup"), true, 0);
        }
        else
        {
            const FString Label = TEXT("由房主选择让全队返回大厅");
            GetTextSize(Label, TextWidth, TextHeight, GEngine->GetSmallFont(), 1.f);
            DrawText(Label, FLinearColor(0.7f, 0.8f, 0.9f), CenterX - TextWidth * 0.5f,
                CenterY + 213.f, GEngine->GetSmallFont(), 1.f);
        }

        if (!bSolo)
        {
            const bool bSwapRolesHovered = HitBoxesOver.Contains(SwapRolesButtonName);
            DrawRect(bSwapRolesHovered ? FLinearColor(0.30f, 0.57f, 0.82f) : FLinearColor(0.20f, 0.42f, 0.67f),
                SwapRolesButtonMin.X, SwapRolesButtonMin.Y, ButtonWidth, ButtonHeight);
            const FString SwapRolesText = GS->RoomMode == ETreasureRoomMode::OneExplorer
                ? TEXT("轮换探索者，再玩一次") : TEXT("轮换地图师，再玩一次");
            GetTextSize(SwapRolesText, TextWidth, TextHeight, GEngine->GetLargeFont(), 1.f);
            DrawText(SwapRolesText, FLinearColor::White, CenterX - TextWidth * 0.5f,
                SwapRolesButtonMin.Y + (ButtonHeight - TextHeight) * 0.5f, GEngine->GetLargeFont(), 1.f);
            AddHitBox(SwapRolesButtonMin, FVector2D(ButtonWidth, ButtonHeight), SwapRolesButtonName, true, 0);
        }
        DrawRect(HitBoxesOver.Contains(TEXT("BeginRoundReview")) ? FLinearColor(0.65f, 0.48f, 0.18f) : FLinearColor(0.48f, 0.34f, 0.13f),
            ReviewButtonMin.X, ReviewButtonMin.Y, ButtonWidth, 50.f);
        const FString ReviewText = TEXT("留在岛上复盘");
        GetTextSize(ReviewText, TextWidth, TextHeight, GEngine->GetLargeFont(), 1.f);
        DrawText(ReviewText, FLinearColor::White, CenterX - TextWidth * 0.5f,
            ReviewButtonMin.Y + (50.f - TextHeight) * 0.5f, GEngine->GetLargeFont(), 1.f);
        AddHitBox(ReviewButtonMin, FVector2D(ButtonWidth, 50.f), TEXT("BeginRoundReview"), true, 0);
        return;
    }

    const bool bScout = PS->PlayerRole == ETreasurePlayerRole::Scout;
    int32 Mapmakers = 0, Submitted = 0;
    for (APlayerState* State : GS->PlayerArray)
        if (const ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
            if (Member->PlayerRole == ETreasurePlayerRole::Scout)
            { ++Mapmakers; Submitted += Member->bSketchSubmitted; }
    const FString RoleLabel = FString::Printf(TEXT("%s / %s"),
        bScout ? TEXT("侦察者") : TEXT("寻宝者"),
        GetNetMode() == NM_ListenServer ? TEXT("主机") : TEXT("已连接客户端"));
    const FString Help = GS->bReviewingRound ? TEXT("复盘中：WASD 逛岛 | M 查看图纸 | 宝藏位置已显示")
        : PC->IsScoutSpectating()
        ? TEXT("观战：WASD 飞行 | Space 上升 | Ctrl 下降 | Shift 加速")
        : bScout && PC->HasSubmittedSketch() ? TEXT("已交图，等待其他地图师完成；到时自动收齐")
        : bScout ? TEXT("WASD 移动 | M 打开白纸画图 | C 清空 | Enter 交图")
        : TEXT("等待交图；收到后 M 查看地图 | E 挖掘");
    DrawText(FString::Printf(TEXT("%s  |  岛屿种子 %d"), *RoleLabel, GS->IslandSeed), FLinearColor::White, 35.f, 28.f, GEngine->GetLargeFont(), 1.f);
    DrawText(Help, FLinearColor(0.9f,0.9f,0.9f), 35.f, 62.f, GEngine->GetSmallFont(), 1.f);
    if (PC->IsScoutSpectating())
    {
        DrawText(TEXT("鼠标转向 | Tab 切换视角 | Q 切换探索者 | T 宝藏"),
            FLinearColor(0.9f, 0.9f, 0.9f), 35.f, 86.f, GEngine->GetSmallFont(), 1.f);
        DrawText(FString::Printf(TEXT("%s  |  宝藏标记：%s"),
            PC->IsHunterFirstPersonView() ? TEXT("寻宝者第一视角") : TEXT("自由飞行"),
            PC->IsSpectatorTreasureVisible() ? TEXT("显示") : TEXT("隐藏")),
            FLinearColor(0.45f, 0.9f, 0.85f), 35.f, 110.f, GEngine->GetSmallFont(), 1.f);
    }

    if (GS->bSurfacePaintEnabled && bScout && !PC->HasSubmittedSketch() && GS->Phase == ETreasureRoundPhase::ScoutDrawing && !PC->IsMapOpen())
    {
        DrawLine(Canvas->SizeX * 0.5f - 7.f, Canvas->SizeY * 0.5f, Canvas->SizeX * 0.5f + 7.f, Canvas->SizeY * 0.5f, FLinearColor(0.1f, 0.6f, 1.f), 2.f);
        DrawLine(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f - 7.f, Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f + 7.f, FLinearColor(0.1f, 0.6f, 1.f), 2.f);
        DrawText(FString::Printf(TEXT("实验喷漆：按住右键喷漆 | M画图 | 每局最多%d个色点"), ATreasureSurfacePaint::MaxStamps),
            FLinearColor(0.1f, 0.6f, 1.f), 35.f, 86.f, GEngine->GetSmallFont(), 0.9f);
    }
    const int32 SecondsRemaining = GS->GetSecondsRemaining();
    const FString TimerText = FString::Printf(TEXT("%02d:%02d"), SecondsRemaining / 60, SecondsRemaining % 60);
    const FLinearColor TimerColor = SecondsRemaining <= 10 ? FLinearColor(1.f, 0.12f, 0.08f) : FLinearColor::White;
    const float TimerWidth = 150.f;
    if (!GS->bReviewingRound)
    {
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), Canvas->SizeX - TimerWidth - 28.f, 22.f, TimerWidth, 58.f);
        DrawText(TimerText, TimerColor, Canvas->SizeX - TimerWidth - 6.f, 30.f, GEngine->GetLargeFont(), 1.25f);
    }
    if (!GS->bReviewingRound && GS->RoomMode == ETreasureRoomMode::OneExplorer && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
        DrawText(FString::Printf(TEXT("已交图 %d / %d"), Submitted, Mapmakers), FLinearColor::White,
            Canvas->SizeX - TimerWidth - 28.f, 86.f, GEngine->GetSmallFont(), 1.f);

    if (!PC->GetStatusMessage().IsEmpty())
        DrawText(PC->GetStatusMessage(), FLinearColor::Yellow, 35.f, Canvas->SizeY - 70.f, GEngine->GetMediumFont(), 1.f);

    const bool bWatchingLiveSketch = PC->IsHunterWaiting();
    auto DrawReviewButton = [&]()
    {
        if (!GS->bReviewingRound) return;
        const FVector2D ButtonMin(Canvas->SizeX - 244.f, 22.f);
        DrawRect(HitBoxesOver.Contains(TEXT("EndRoundReview")) ? FLinearColor(0.67f, 0.41f, 0.19f)
            : FLinearColor(0.48f, 0.29f, 0.13f), ButtonMin.X, ButtonMin.Y, 220.f, 46.f);
        DrawText(TEXT("复盘完了 · 返回结算"), FLinearColor::White, ButtonMin.X + 12.f, ButtonMin.Y + 12.f, GEngine->GetSmallFont(), 1.f);
        AddHitBox(ButtonMin, FVector2D(220.f, 46.f), TEXT("EndRoundReview"), true, 0);
    };
    if (!PC->IsMapOpen() && !bWatchingLiveSketch) { DrawReviewButton(); return; }
    if (bWatchingLiveSketch)
        DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.96f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
    const FVector2D Min = PC->GetPaperMin();
    const FVector2D Size = PC->GetPaperSize();
    DrawRect(FLinearColor(0.96f, 0.94f, 0.86f, 0.98f), Min.X, Min.Y, Size.X, Size.Y);
    DrawText(GS->bReviewingRound ? TEXT("复盘地图 · 可对照宝藏位置")
        : bScout ? TEXT("空白纸：请画岛屿轮廓、地形地标和藏宝点")
        : bWatchingLiveSketch ? TEXT("地图师的实时画纸 · 只能观看") : TEXT("地图师留下的手绘地图"),
        FLinearColor::Black, Min.X + 18.f, Min.Y + 14.f, GEngine->GetSmallFont(), 1.f);
    if (bWatchingLiveSketch && PC->GetSketchPageCount() == 0)
        DrawText(TEXT("正在接收地图师画纸……"), FLinearColor(0.30f, 0.33f, 0.35f),
            Min.X + 18.f, Min.Y + 52.f, GEngine->GetMediumFont(), 1.f);

    for (const FSketchStroke& Stroke : PC->GetStrokes())
    {
        for (int32 I = 1; I < Stroke.Points.Num(); ++I)
        {
            const FVector2D A = Min + Stroke.Points[I-1] * Size;
            const FVector2D B = Min + Stroke.Points[I] * Size;
            DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(0.08f,0.07f,0.05f), 4.f);
        }
    }
    if ((!bScout || GS->bReviewingRound) && PC->GetSketchPageCount() > 0)
    {
        const float FooterY = Min.Y + Size.Y + 4.f;
        DrawRect(FLinearColor(0.96f, 0.94f, 0.86f), Min.X, FooterY, Size.X, 28.f);
        const FString PageLabel = bWatchingLiveSketch
            ? FString::Printf(TEXT("实时图纸 %d / %d · %s · 已交图 %d / %d"), PC->GetActiveSketchPage() + 1,
                PC->GetSketchPageCount(), *PC->GetActiveMapmakerName(), Submitted, Mapmakers)
            : FString::Printf(TEXT("图纸 %d / %d · %s"), PC->GetActiveSketchPage() + 1,
                PC->GetSketchPageCount(), *PC->GetActiveMapmakerName());
        DrawText(PageLabel, FLinearColor::Black, Min.X + 18.f, FooterY + 7.f, GEngine->GetSmallFont(), 1.f);
        if (PC->GetSketchPageCount() > 1)
            for (int32 I = 0; I < 2; ++I)
            {
                const FVector2D ButtonMin(Min.X + Size.X - 244.f + I * 120.f, FooterY);
                const FName Name = I == 0 ? TEXT("PreviousSketchPage") : TEXT("NextSketchPage");
                DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), ButtonMin.X, ButtonMin.Y, 110.f, 28.f);
                DrawText(I == 0 ? TEXT("上一张 ←") : TEXT("下一张 →"), FLinearColor::White, ButtonMin.X + 10.f, ButtonMin.Y + 7.f, GEngine->GetSmallFont(), 1.f);
                AddHitBox(ButtonMin, FVector2D(110.f, 28.f), Name, true, 10);
            }
    }
    if (bWatchingLiveSketch)
    {
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.78f), Canvas->SizeX - TimerWidth - 195.f, 18.f, TimerWidth + 173.f, 42.f);
        DrawText(FString::Printf(TEXT("绘图剩余 %s · 到时自动交图"), *TimerText), TimerColor,
            Canvas->SizeX - TimerWidth - 185.f, 28.f, GEngine->GetSmallFont(), 1.f);
    }
    DrawReviewButton();
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
