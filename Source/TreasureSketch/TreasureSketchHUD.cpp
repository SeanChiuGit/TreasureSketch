#include "TreasureSketchHUD.h"
#include "TreasureSurfacePaint.h"

#include "TreasureSketchGameState.h"
#include "TreasureSketchPlayerController.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureOnlineSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Misc/Paths.h"

void ATreasureSketchHUD::EnsureGameFonts()
{
    if (!DisplayFont)
    {
        DisplayFont = NewObject<UFont>(this, TEXT("TreasureDisplayFont"));
        DisplayFont->FontCacheType = EFontCacheType::Runtime;
        DisplayFont->LegacyFontSize = 30;
        DisplayFont->LegacyFontName = TEXT("Regular");
        DisplayFont->CompositeFont = FCompositeFont(
            TEXT("Regular"),
            FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI/Fonts/ZCOOLKuaiLe-Regular.ttf")),
            EFontHinting::Auto,
            EFontLoadingPolicy::LazyLoad);
    }

    if (!BodyFont)
    {
        BodyFont = NewObject<UFont>(this, TEXT("TreasureBodyFont"));
        BodyFont->FontCacheType = EFontCacheType::Runtime;
        BodyFont->LegacyFontSize = 22;
        BodyFont->LegacyFontName = TEXT("Heavy");
        BodyFont->CompositeFont = FCompositeFont(
            TEXT("Heavy"),
            FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI/Fonts/SourceHanSansCN-Heavy.otf")),
            EFontHinting::Auto,
            EFontLoadingPolicy::LazyLoad);
    }
}

void ATreasureSketchHUD::DrawHUD()
{
    Super::DrawHUD();
    ATreasureSketchPlayerController* PC = Cast<ATreasureSketchPlayerController>(PlayerOwner);
    ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    ATreasureSketchPlayerState* PS = PC ? PC->GetPlayerState<ATreasureSketchPlayerState>() : nullptr;
    if (!PC || !GS || !PS || !Canvas) return;

    EnsureGameFonts();

    // The bundled fonts provide the weight. Keep just one crisp offset shadow so
    // lettering lifts from the scene without the blurred halo of repeated passes.
    auto DrawReadableText = [&](const FString& Text, const FLinearColor& Color, float X, float Y,
        UFont* Font, float Scale = 1.f, bool bHeavy = false)
    {
        const float InkOffset = bHeavy ? 2.f : 1.f;
        DrawText(Text, FLinearColor(0.005f, 0.012f, 0.014f, 0.82f), X + InkOffset, Y + InkOffset, Font, Scale);
        DrawText(Text, Color, X, Y, Font, Scale);
    };

    if (PC->IsFrontEndVisible())
    {
        const EFrontEndPage Page = PC->GetFrontEndPage();
        const float W = Canvas->SizeX;
        const float H = Canvas->SizeY;
        const bool bMainMenu = Page == EFrontEndPage::MainMenu;
        const bool bCompactFrontPage = bMainMenu || Page == EFrontEndPage::Settings;
        const float PanelW = bCompactFrontPage ? FMath::Clamp(W * 0.29f, 500.f, 560.f)
            : FMath::Clamp(W * 0.40f, 500.f, 700.f);
        const float PanelH = bMainMenu ? 610.f : Page == EFrontEndPage::Settings ? 500.f : H * 0.82f;
        const float PanelX = W * 0.065f;
        const float PanelY = (H - PanelH) * 0.5f;
        const FLinearColor DeepOcean(0.018f, 0.042f, 0.050f, 0.95f);
        const FLinearColor JournalGreen(0.040f, 0.095f, 0.095f, 0.97f);
        const FLinearColor TreasureGold(1.00f, 0.59f, 0.08f, 1.f);
        const FLinearColor Parchment(1.00f, 0.95f, 0.75f, 1.f);
        const FLinearColor BrightJade(0.20f, 0.95f, 0.72f, 1.f);
        DrawRect(FLinearColor(0.008f, 0.022f, 0.028f, 0.52f), 0.f, 0.f, W, H);
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.30f), PanelX + 12.f, PanelY + 14.f, PanelW, PanelH);
        DrawRect(DeepOcean, PanelX, PanelY, PanelW, PanelH);
        DrawRect(JournalGreen, PanelX + 7.f, PanelY + 7.f, PanelW - 14.f, PanelH - 14.f);
        DrawRect(TreasureGold, PanelX, PanelY, 8.f, PanelH);
        DrawRect(FLinearColor(TreasureGold.R, TreasureGold.G, TreasureGold.B, 0.75f),
            PanelX + 40.f, PanelY + 138.f, PanelW - 80.f, 3.f);

        DrawReadableText(TEXT("寻宝手绘"), TreasureGold,
            PanelX + 42.f, PanelY + 22.f, DisplayFont, 1.48f, true);
        DrawReadableText(TEXT("TREASURE SKETCH"), BrightJade,
            PanelX + 44.f, PanelY + 72.f, BodyFont, 0.82f, false);
        DrawReadableText(TEXT("画下岛屿，交出地图，一起找到宝藏"), Parchment,
            PanelX + 44.f, PanelY + 102.f, BodyFont, 0.96f, false);

        auto DrawMenuButton = [&](FName Name, const FString& Label, float Y, bool bPrimary = false)
        {
            float TextW = 0.f, TextH = 0.f;
            GetTextSize(Label, TextW, TextH, DisplayFont, 0.98f);
            const float ButtonW = FMath::Clamp(TextW + 72.f, 250.f, PanelW - 88.f);
            const float ButtonH = FMath::Clamp(TextH + 24.f, 58.f, 66.f);
            const FVector2D Min(PanelX + (PanelW - ButtonW) * 0.5f, Y);
            const FVector2D Size(ButtonW, ButtonH);
            const bool bHovered = HitBoxesOver.Contains(Name);
            const FLinearColor Normal = bPrimary ? FLinearColor(0.76f, 0.30f, 0.055f, 0.98f)
                : FLinearColor(0.045f, 0.19f, 0.18f, 0.98f);
            const FLinearColor Hover = bPrimary ? FLinearColor(1.f, 0.48f, 0.06f, 1.f)
                : FLinearColor(0.08f, 0.38f, 0.31f, 1.f);
            DrawRect(bHovered ? Hover : Normal, Min.X, Min.Y, Size.X, Size.Y);
            DrawRect(bPrimary ? TreasureGold : BrightJade,
                Min.X, Min.Y, 5.f, Size.Y);
            DrawRect(FLinearColor(1.f, 0.73f, 0.18f, bPrimary ? 0.85f : 0.35f),
                Min.X + 5.f, Min.Y, Size.X - 5.f, 2.f);
            DrawReadableText(Label, bPrimary ? FLinearColor(1.f, 0.94f, 0.68f) : FLinearColor(0.94f, 1.f, 0.91f),
                Min.X + (Size.X - TextW) * 0.5f, Min.Y + (Size.Y - TextH) * 0.5f,
                DisplayFont, 0.98f, true);
            AddHitBox(Min, Size, Name, true, 10);
        };

        auto DrawDifficultySettings = [&](float SettingsX, float SettingsY, float SettingsW, bool bCanAdjustSettings)
        {
            auto DrawRoomSetting = [&](const FString& Label, float Y, FName Less, FName More, bool bCanLess, bool bCanMore)
            {
                DrawRect(FLinearColor(0.045f, 0.105f, 0.11f, 0.98f), SettingsX, Y, SettingsW, 38.f);
                DrawText(Label, FLinearColor::White, SettingsX + 12.f, Y + 9.f, BodyFont, 0.92f);
                const float ButtonX = SettingsX + SettingsW - 92.f;
                for (int32 I = 0; I < 2; ++I)
                {
                    const bool bEnabled = bCanAdjustSettings && (I == 0 ? bCanLess : bCanMore);
                    const float X = ButtonX + I * 47.f;
                    DrawRect(bEnabled ? FLinearColor(0.16f, 0.34f, 0.31f) : FLinearColor(0.07f, 0.105f, 0.11f), X, Y + 3.f, 43.f, 32.f);
                    DrawText(I == 0 ? TEXT("−") : TEXT("+"), bEnabled ? FLinearColor::White : FLinearColor(0.35f, 0.40f, 0.40f), X + 14.f, Y + 6.f, DisplayFont, 0.92f);
                    if (bEnabled) AddHitBox(FVector2D(X, Y + 3.f), FVector2D(43.f, 32.f), I == 0 ? Less : More, true, 10);
                }
            };
            DrawRoomSetting(TEXT("面积倍数："), SettingsY, TEXT("RoomMapSmaller"), TEXT("RoomMapLarger"), GS->RoomMapScale > GS->MinMapScale, GS->RoomMapScale < GS->MaxMapScale);
            const float InputX = SettingsX + 132.f;
            const float InputW = SettingsW - 228.f;
            const bool bEditingScale = PC->IsMapScaleEditing();
            const FString ScaleText = bEditingScale ? PC->GetMapScaleText() : FString::Printf(TEXT("%.6g"), GS->RoomMapScale);
            float ParsedScale = 0.f;
            const bool bValidScale = LexTryParseString(ParsedScale, *ScaleText) && FMath::IsFinite(ParsedScale)
                && ParsedScale >= GS->MinMapScale && ParsedScale <= GS->MaxMapScale;
            DrawRect(bEditingScale ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.08f, 0.12f, 0.13f), InputX, SettingsY + 3.f, InputW, 32.f);
            DrawText(ScaleText + (bEditingScale ? TEXT("_") : TEXT(" 倍")), bValidScale ? FLinearColor::White : FLinearColor(1.f, 0.4f, 0.3f),
                InputX + 9.f, SettingsY + 9.f, BodyFont, 0.9f);
            if (bCanAdjustSettings) AddHitBox(FVector2D(InputX, SettingsY + 3.f), FVector2D(InputW, 32.f), TEXT("MapScaleInput"), true, 10);
            DrawRoomSetting(FString::Printf(TEXT("绘图时间：%d 秒"), GS->DrawingDurationSeconds), SettingsY + 40.f, TEXT("RoomDrawingLess"), TEXT("RoomDrawingMore"), GS->DrawingDurationSeconds > GS->MinPhaseSeconds, GS->DrawingDurationSeconds < GS->MaxPhaseSeconds);
            DrawRoomSetting(FString::Printf(TEXT("寻宝时间：%d 秒"), GS->SearchingDurationSeconds), SettingsY + 80.f, TEXT("RoomSearchingLess"), TEXT("RoomSearchingMore"), GS->SearchingDurationSeconds > GS->MinPhaseSeconds, GS->SearchingDurationSeconds < GS->MaxPhaseSeconds);
            DrawRoomSetting(FString::Printf(TEXT("挖掘冷却：%d 秒"), GS->DigCooldownSeconds), SettingsY + 120.f,
                TEXT("RoomDigCooldownLess"), TEXT("RoomDigCooldownMore"),
                GS->DigCooldownSeconds > GS->MinDigCooldownSeconds, GS->DigCooldownSeconds < GS->MaxDigCooldownSeconds);
            DrawRoomSetting(FString::Printf(TEXT("移动速度：%.2f 倍"), GS->MovementSpeedMultiplier), SettingsY + 160.f, TEXT("RoomSpeedLess"), TEXT("RoomSpeedMore"), GS->MovementSpeedMultiplier > GS->MinMovementSpeed, GS->MovementSpeedMultiplier < GS->MaxMovementSpeed);
            auto DrawRoomToggle = [&](FName Name, const FString& Label, float Y)
            {
                DrawRect(bCanAdjustSettings ? FLinearColor(0.13f, 0.28f, 0.27f) : FLinearColor(0.07f, 0.105f, 0.11f), SettingsX, Y, SettingsW, 38.f);
                DrawText(Label, FLinearColor::White, SettingsX + 12.f, Y + 9.f, BodyFont, 0.9f);
                if (bCanAdjustSettings) AddHitBox(FVector2D(SettingsX, Y), FVector2D(SettingsW, 38.f), Name, true, 10);
            };
            DrawRoomToggle(TEXT("ToggleTreasureRange"), GS->bTreasureRangeVisible ? TEXT("宝藏判定范围：显示") : TEXT("宝藏判定范围：隐藏"), SettingsY + 200.f);
            DrawRoomToggle(TEXT("ToggleSpreadPlayerSpawns"), GS->bSpreadPlayerSpawns ? TEXT("出生点：分散登岛") : TEXT("出生点：同一区域"), SettingsY + 240.f);
        };

        if (Page == EFrontEndPage::MainMenu)
        {
            float Y = PanelY + 160.f;
            DrawMenuButton(TEXT("MenuCreate"), TEXT("创建房间"), Y, true); Y += 70.f;
            DrawMenuButton(TEXT("MenuJoin"), TEXT("加入房间"), Y); Y += 70.f;
            DrawMenuButton(TEXT("MenuSolo"), TEXT("单人探险"), Y); Y += 70.f;
            DrawMenuButton(TEXT("MenuSettings"), TEXT("设置"), Y); Y += 70.f;
            DrawMenuButton(TEXT("MenuQuit"), TEXT("退出游戏"), Y);
            DrawReadableText(TEXT("2 至 4 人合作寻宝"), BrightJade,
                PanelX + 44.f, PanelY + PanelH - 42.f, BodyFont, 0.94f, false);
        }
        else if (Page == EFrontEndPage::SoloTest)
        {
            DrawReadableText(TEXT("单人测试"), TreasureGold, PanelX + 48.f, PanelY + 154.f,
                DisplayFont, 1.25f);
            DrawText(TEXT("地图测试显示宝藏；玩法测试可调大小和时间"),
                Parchment, PanelX + 48.f, PanelY + 207.f, BodyFont, 0.92f);
            DrawMenuButton(TEXT("SoloForest"), TEXT("测试雾森林"), PanelY + 250.f, true);
            DrawMenuButton(TEXT("SoloBeach"), TEXT("测试海盗沙滩岛"), PanelY + 320.f);
            DrawMenuButton(TEXT("SoloRandom"), TEXT("随机主题与新种子"), PanelY + 390.f);
            DrawMenuButton(TEXT("SoloHunter"), TEXT("探索者玩法测试"), PanelY + 460.f, true);
            DrawMenuButton(TEXT("SoloFullFlow"), TEXT("完整流程测试：自己画，自己找"), PanelY + 530.f, true);
            DrawMenuButton(TEXT("MenuBack"), TEXT("返回主页面"), PanelY + 600.f);
            const float SeedX = PanelX + PanelW + 28.f;
            const float SeedW = FMath::Max(180.f, FMath::Min(390.f, W - SeedX - 24.f));
            const float SeedY = PanelY + 150.f;
            DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.24f), SeedX - 4.f, SeedY - 4.f, SeedW + 28.f, 598.f);
            DrawRect(FLinearColor(0.025f, 0.075f, 0.078f, 0.94f), SeedX - 14.f, SeedY - 14.f, SeedW + 28.f, 598.f);
            DrawRect(TreasureGold, SeedX - 14.f, SeedY - 14.f, 4.f, 598.f);
            DrawRect(FLinearColor(TreasureGold.R, TreasureGold.G, TreasureGold.B, 0.62f),
                SeedX - 10.f, SeedY - 14.f, SeedW + 24.f, 3.f);
            DrawText(TEXT("玩法测试种子（可留空）"), BrightJade, SeedX, SeedY,
                BodyFont, 1.f);
            DrawRect(PC->IsTestSeedEditing() ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.06f, 0.12f, 0.13f),
                SeedX, SeedY + 44.f, SeedW, 44.f);
            const FString SeedText = PC->GetTestSeedText();
            DrawText(SeedText.IsEmpty() ? TEXT("点击输入；留空随机") : SeedText,
                FLinearColor(1.f, 0.97f, 0.84f), SeedX + 12.f, SeedY + 54.f, BodyFont, 1.f);
            AddHitBox(FVector2D(SeedX, SeedY + 44.f), FVector2D(SeedW, 44.f), TEXT("TestSeedInput"), true, 10);
            DrawText(TEXT("数字键输入，退格删除，Enter完成"), FLinearColor(0.75f, 0.84f, 0.82f),
                SeedX, SeedY + 102.f, BodyFont, 0.86f);
            DrawText(TEXT("面积0.5至5倍；点击输入，Enter确认"), FLinearColor(0.75f, 0.84f, 0.82f),
                SeedX, SeedY + 130.f, BodyFont, 0.86f);
            DrawRect(FLinearColor(0.09f, 0.24f, 0.22f), SeedX, SeedY + 160.f, 148.f, 38.f);
            DrawText(TEXT("清空种子"), FLinearColor::White, SeedX + 16.f, SeedY + 168.f, BodyFont, 0.92f);
            AddHitBox(FVector2D(SeedX, SeedY + 160.f), FVector2D(148.f, 38.f), TEXT("TestSeedClear"), true, 10);
            DrawDifficultySettings(SeedX, SeedY + 214.f, SeedW, true);
            DrawRect(GS->bSurfacePaintEnabled ? FLinearColor(0.12f, 0.38f, 0.60f) : FLinearColor(0.09f, 0.17f, 0.18f), SeedX, SeedY + 502.f, SeedW, 42.f);
            DrawText(GS->bSurfacePaintEnabled ? TEXT("实验喷漆：开启") : TEXT("实验喷漆：关闭"), FLinearColor::White, SeedX + 12.f, SeedY + 512.f, BodyFont, 1.f);
            AddHitBox(FVector2D(SeedX, SeedY + 502.f), FVector2D(SeedW, 42.f), TEXT("ToggleSurfacePaint"), true, 10);
            DrawText(TEXT("地面瞄准后按住右键喷漆"), FLinearColor(0.78f, 0.87f, 0.84f), SeedX, SeedY + 552.f, BodyFont, 0.86f);
            if (!SeedText.IsEmpty() && PC->GetTestSeed() == 0)
                DrawText(TEXT("请输入1至2147483647，或清空以随机"), FLinearColor(1.f, 0.4f, 0.3f),
                    SeedX, SeedY + 574.f, BodyFont, 0.86f);
        }
        else if (Page == EFrontEndPage::Settings)
        {
            DrawText(TEXT("设置"), FLinearColor::White, PanelX + 48.f, PanelY + 185.f,
                DisplayFont, 1.25f);
            DrawText(TEXT("设置页面仍在开发中"), FLinearColor(0.75f, 0.84f, 0.82f), PanelX + 48.f, PanelY + 250.f,
                DisplayFont, 1.1f);
            DrawMenuButton(TEXT("MenuBack"), TEXT("返回主页面"), PanelY + 330.f, true);
        }
        else if (Page == EFrontEndPage::JoinBrowser)
        {
            const UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>();
            DrawText(TEXT("加入房间"), FLinearColor::White, PanelX + 48.f, PanelY + 150.f,
                DisplayFont, 1.2f);
            DrawText(Online ? Online->GetStatus() : TEXT("Steam 在线服务未初始化"),
                FLinearColor(0.96f, 0.79f, 0.40f), PanelX + 48.f, PanelY + 205.f,
                BodyFont, 1.f);
            float RoomY = PanelY + 250.f;
            if (Online && Online->GetRoomLines().Num() > 0)
            {
                for (const FString& Room : Online->GetRoomLines())
                {
                    DrawText(Room, FLinearColor(0.86f, 0.92f, 0.90f), PanelX + 48.f, RoomY,
                        BodyFont, 0.95f);
                    RoomY += 34.f;
                    if (RoomY > PanelY + 390.f) break;
                }
                DrawMenuButton(TEXT("MenuJoinFirst"), TEXT("加入第一个房间"), PanelY + 420.f, true);
            }
            else
            {
                DrawText(TEXT("正在搜索，或暂时没有可加入房间"), FLinearColor(0.70f, 0.80f, 0.78f),
                    PanelX + 48.f, RoomY, BodyFont, 1.f);
            }
            DrawMenuButton(TEXT("MenuBack"), TEXT("返回主页面"), PanelY + 500.f);
        }
        else if (Page == EFrontEndPage::RoomLobby)
        {
            const UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>();
            const float ModeX = PanelX + PanelW + 28.f;
            const float ModeW = FMath::Max(260.f, FMath::Min(460.f, W - ModeX - 24.f));
            const bool bHost = GetNetMode() == NM_ListenServer;
            DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.24f), ModeX + 10.f, PanelY + 154.f, ModeW, PanelH - 176.f);
            DrawRect(FLinearColor(0.035f, 0.082f, 0.083f, 0.96f), ModeX, PanelY + 144.f, ModeW, PanelH - 176.f);
            DrawRect(TreasureGold, ModeX, PanelY + 144.f, ModeW, 4.f);
            DrawText(TEXT("本局规则"), TreasureGold, ModeX + 18.f, PanelY + 164.f, DisplayFont, 1.12f);
            const TCHAR* Modes[] = { TEXT("一名地图师，多名探索者"), TEXT("多名地图师，一名探索者"), TEXT("探索者对抗：开启后至少三人") };
            for (int32 ModeIndex = 0; ModeIndex < 3; ++ModeIndex)
            {
                const float ModeY = PanelY + 204.f + ModeIndex * 48.f;
                const bool bSelected = ModeIndex == 0 ? GS->RoomMode == ETreasureRoomMode::OneMapmaker
                    || GS->RoomMode == ETreasureRoomMode::ExplorerRace : ModeIndex == 1
                    ? GS->RoomMode == ETreasureRoomMode::OneExplorer : GS->RoomMode == ETreasureRoomMode::ExplorerRace;
                DrawRect(bSelected ? FLinearColor(0.62f, 0.37f, 0.13f) : FLinearColor(0.065f, 0.14f, 0.14f), ModeX + 14.f, ModeY, ModeW - 28.f, 42.f);
                const FString Label = ModeIndex == 2 ? FString::Printf(TEXT("%s %s"), bSelected ? TEXT("✓") : TEXT("○"), Modes[ModeIndex])
                    : FString(Modes[ModeIndex]) + (bSelected ? TEXT("（已选择）") : TEXT(""));
                DrawText(Label, FLinearColor::White, ModeX + 28.f, ModeY + 10.f, BodyFont, 0.93f);
                if (bHost)
                    AddHitBox(FVector2D(ModeX + 14.f, ModeY), FVector2D(ModeW - 28.f, 42.f), ModeIndex == 0 ? TEXT("RoomModeCoop")
                        : ModeIndex == 1 ? TEXT("RoomModeOneExplorer") : TEXT("RoomModeRaceToggle"), true, 10);
            }
            DrawText(TEXT("难度与地图"), Parchment, ModeX + 18.f, PanelY + 360.f, BodyFont, 1.05f);
            DrawDifficultySettings(ModeX + 14.f, PanelY + 392.f, ModeW - 28.f, bHost);
            DrawRect(GS->bSurfacePaintEnabled ? FLinearColor(0.12f, 0.38f, 0.60f) : FLinearColor(0.09f, 0.17f, 0.18f),
                ModeX + 14.f, PanelY + 674.f, ModeW - 28.f, 42.f);
            DrawText(GS->bSurfacePaintEnabled ? TEXT("实验喷漆：开启") : TEXT("实验喷漆：关闭"),
                FLinearColor::White, ModeX + 26.f, PanelY + 684.f, BodyFont, 0.95f);
            if (bHost)
                AddHitBox(FVector2D(ModeX + 14.f, PanelY + 674.f), FVector2D(ModeW - 28.f, 42.f), TEXT("ToggleSurfacePaint"), true, 10);
            DrawText(bHost ? TEXT("房主可调整 · 开局后锁定") : TEXT("等待房主调整本局规则"),
                FLinearColor(0.72f, 0.82f, 0.79f), ModeX + 18.f, PanelY + 728.f, BodyFont, 0.88f);
            float PlayerY = PanelY + 385.f;
            const ETreasurePlayerRole SingleRole = GS->RoomMode == ETreasureRoomMode::OneExplorer
                ? ETreasurePlayerRole::Hunter : ETreasurePlayerRole::Scout;
            int32 Scouts = 0, Hunters = 0;
            for (APlayerState* State : GS->PlayerArray)
                if (const ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
                {
                    Scouts += Member->PlayerRole == ETreasurePlayerRole::Scout;
                    Hunters += Member->PlayerRole == ETreasurePlayerRole::Hunter;
                    const bool bOwnRow = Member == PS;
                    DrawText(FString::Printf(TEXT("%s%s — %s"), bOwnRow ? TEXT("你：") : TEXT(""), *Member->GetPlayerName(),
                        Member->PlayerRole == ETreasurePlayerRole::Scout ? TEXT("地图师") : TEXT("探索者")),
                        FLinearColor::White, PanelX + 60.f, PlayerY, BodyFont, 0.96f);
                    if (bOwnRow && Member->PlayerRole != SingleRole)
                    {
                        const float ClaimX = PanelX + PanelW - 206.f;
                        DrawRect(HitBoxesOver.Contains(TEXT("RoomClaimSingleRole")) ? FLinearColor(0.23f, 0.49f, 0.40f)
                            : FLinearColor(0.13f, 0.34f, 0.29f), ClaimX, PlayerY - 6.f, 158.f, 32.f);
                        DrawText(SingleRole == ETreasurePlayerRole::Scout ? TEXT("换成地图师") : TEXT("换成探索者"),
                            FLinearColor::White, ClaimX + 12.f, PlayerY + 2.f, BodyFont, 0.88f);
                        AddHitBox(FVector2D(ClaimX, PlayerY - 6.f), FVector2D(158.f, 32.f), TEXT("RoomClaimSingleRole"), true, 10);
                    }
                    PlayerY += 42.f;
                }
            DrawText(TEXT("合作房间"), FLinearColor::White, PanelX + 48.f, PanelY + 150.f,
                DisplayFont, 1.2f);
            DrawText(FString::Printf(TEXT("玩家  %d / 4"), GS->PlayerArray.Num()),
                FLinearColor(0.86f, 0.92f, 0.90f), PanelX + 48.f, PanelY + 215.f,
                DisplayFont, 1.f);
            DrawText(Online ? Online->GetStatus() : TEXT("正在建立 Steam 房间……"),
                FLinearColor(0.96f, 0.79f, 0.40f), PanelX + 48.f, PanelY + 265.f,
                BodyFont, 0.95f);
            const float PoolX = PanelX + 48.f;
            const float PoolY = PanelY + 294.f;
            const float PoolButtonW = (PanelW - 181.f) * 0.5f;
            DrawText(TEXT("地图池"), Parchment, PoolX, PoolY + 9.f, BodyFont, 0.95f);
            for (int32 PoolIndex = 0; PoolIndex < 2; ++PoolIndex)
            {
                const bool bSelected = PoolIndex == 0 ? GS->bBeachInMapPool : GS->bForestInMapPool;
                const bool bOtherSelected = PoolIndex == 0 ? GS->bForestInMapPool : GS->bBeachInMapPool;
                const float ButtonX = PoolX + 75.f + PoolIndex * (PoolButtonW + 10.f);
                DrawRect(bSelected ? FLinearColor(0.53f, 0.36f, 0.14f) : FLinearColor(0.08f, 0.12f, 0.13f),
                    ButtonX, PoolY, PoolButtonW, 30.f);
                DrawText(PoolIndex == 0 ? (bSelected ? TEXT("沙滩 ✓") : TEXT("沙滩 ○"))
                    : (bSelected ? TEXT("森林 ✓") : TEXT("森林 ○")),
                    FLinearColor::White, ButtonX + 11.f, PoolY + 7.f, BodyFont, 0.9f);
                if (bHost && (bOtherSelected || !bSelected))
                    AddHitBox(FVector2D(ButtonX, PoolY), FVector2D(PoolButtonW, 30.f),
                        PoolIndex == 0 ? TEXT("RoomPoolBeach") : TEXT("RoomPoolForest"), true, 10);
            }
            DrawText(TEXT("探险队成员"), Parchment, PanelX + 48.f, PanelY + 348.f,
                BodyFont, 1.05f);
            if (GetNetMode() == NM_ListenServer)
            {
                DrawMenuButton(TEXT("RoomInvite"), TEXT("邀请 Steam 好友"), PanelY + 570.f, true);
                const int32 ExpectedScouts = GS->RoomMode == ETreasureRoomMode::OneExplorer ? GS->PlayerArray.Num() - 1 : 1;
                const bool bCanStart = GS->PlayerArray.Num() >= (GS->RoomMode == ETreasureRoomMode::ExplorerRace ? 3 : 2)
                    && Scouts == ExpectedScouts
                    && Hunters == GS->PlayerArray.Num() - ExpectedScouts;
                if (bCanStart) DrawMenuButton(TEXT("RoomStart"), TEXT("开始游戏"), PanelY + 640.f, true);
                else
                {
                    DrawRect(FLinearColor(0.08f, 0.12f, 0.13f), PanelX + 48.f, PanelY + 640.f, PanelW - 96.f, 58.f);
                    DrawText(TEXT("开始游戏"), FLinearColor(0.45f, 0.5f, 0.5f), PanelX + 70.f, PanelY + 656.f, BodyFont, 1.15f);
                }
                DrawText(bCanStart ? TEXT("地图师和探索者均已就位，可以开始")
                    : GS->RoomMode == ETreasureRoomMode::ExplorerRace && GS->PlayerArray.Num() < 3
                    ? TEXT("探索者对抗需要至少三名玩家") : GS->PlayerArray.Num() < 2
                    ? TEXT("等待另一名玩家加入……") : TEXT("至少需要一名地图师和一名探索者"),
                    FLinearColor(0.70f, 0.82f, 0.78f), PanelX + 48.f, PanelY + 710.f,
                    BodyFont, 0.95f);
            }
            else
            {
                DrawText(TEXT("已加入房间，等待房主开始游戏……"), FLinearColor(0.70f, 0.82f, 0.78f),
                    PanelX + 48.f, PanelY + 575.f, DisplayFont, 1.f);
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
        DrawText(TEXT("TREASURESKETCH STEAM 大厅"), FLinearColor(0.3f,0.85f,1.f), Canvas->SizeX * 0.20f, Canvas->SizeY * 0.21f, DisplayFont, 1.15f);
        DrawText(TEXT("H 创建房间    I 邀请 Steam 好友    J 搜索    K 加入列表第一个房间"), FLinearColor::White, Canvas->SizeX * 0.20f, Canvas->SizeY * 0.28f, BodyFont, 1.f);
        DrawText(OnlineText, FLinearColor(0.95f,0.85f,0.25f), Canvas->SizeX * 0.20f, Canvas->SizeY * 0.34f, BodyFont, 1.f);
        float RoomY = Canvas->SizeY * 0.41f;
        const TArray<FString>& Rooms = Online->GetRoomLines();
        DrawText(Rooms.IsEmpty() ? TEXT("房间列表为空") : TEXT("可加入房间："), FLinearColor(0.75f,0.8f,0.9f), Canvas->SizeX * 0.20f, RoomY, BodyFont, 1.f);
        for (const FString& Room : Rooms)
        {
            RoomY += 38.f;
            DrawText(Room, FLinearColor::White, Canvas->SizeX * 0.22f, RoomY, BodyFont, 0.95f);
        }
        RoomY += 52.f;
        DrawText(TEXT("联机诊断："), FLinearColor(0.45f,0.9f,0.65f), Canvas->SizeX * 0.20f, RoomY, BodyFont, 0.9f);
        for (const FString& Line : Online->GetDiagnostics())
        {
            RoomY += 23.f;
            DrawText(Line, FLinearColor(0.78f,0.82f,0.88f), Canvas->SizeX * 0.21f, RoomY, BodyFont, 0.82f);
        }
        return;
    }

    if (!GS->bGameStarted)
    {
        DrawRect(FLinearColor(0.015f, 0.02f, 0.035f, 0.94f), Canvas->SizeX * 0.20f, Canvas->SizeY * 0.25f, Canvas->SizeX * 0.60f, Canvas->SizeY * 0.42f);
        DrawText(TEXT("STEAM 房间已连接"), FLinearColor(0.3f,0.85f,1.f), Canvas->SizeX * 0.32f, Canvas->SizeY * 0.32f, DisplayFont, 1.2f);
        DrawText(FString::Printf(TEXT("当前玩家：%d / 4"), GS->PlayerArray.Num()), FLinearColor::White, Canvas->SizeX * 0.39f, Canvas->SizeY * 0.42f, DisplayFont, 1.f);
        DrawText(GetNetMode() == NM_ListenServer ? TEXT("至少两人时，房主按 P 开始") : TEXT("等待房主开始游戏……"),
            FLinearColor(0.95f,0.85f,0.25f), Canvas->SizeX * 0.34f, Canvas->SizeY * 0.52f, BodyFont, 1.f);
        if (const UTreasureOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UTreasureOnlineSubsystem>())
        {
            float DiagY = Canvas->SizeY * 0.58f;
            if (GetNetMode() == NM_ListenServer)
                DrawText(TEXT("按 I 打开 Steam 好友邀请"), FLinearColor(0.45f,0.9f,0.65f), Canvas->SizeX * 0.36f, DiagY, BodyFont, 1.f);
            for (const FString& Line : Online->GetDiagnostics())
            {
                DiagY += 21.f;
                DrawText(Line, FLinearColor(0.72f,0.77f,0.85f), Canvas->SizeX * 0.24f, DiagY, BodyFont, 0.78f);
                if (DiagY > Canvas->SizeY * 0.78f) break;
            }
        }
        return;
    }

    if (PC->IsPauseMenuOpen() && !GS->IsRoundOver())
    {
        const float CenterX = Canvas->SizeX * 0.5f;
        const float CenterY = Canvas->SizeY * 0.5f;
        const float Width = 440.f;
        const float Left = CenterX - Width * 0.5f;
        DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.82f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.32f), Left + 10.f, CenterY - 165.f, Width, 350.f);
        DrawRect(FLinearColor(0.035f, 0.09f, 0.09f, 0.98f), Left, CenterY - 175.f, Width, 350.f);
        DrawRect(FLinearColor(0.91f, 0.68f, 0.27f), Left, CenterY - 175.f, 7.f, 350.f);
        DrawText(TEXT("游戏菜单"), FLinearColor::White, Left + 32.f, CenterY - 145.f, DisplayFont, 1.45f);
        auto DrawPauseButton = [&](FName Name, const FString& Label, float Y)
        {
            const FVector2D Min(Left + 30.f, Y);
            DrawRect(HitBoxesOver.Contains(Name) ? FLinearColor(0.22f, 0.50f, 0.44f)
                : FLinearColor(0.12f, 0.31f, 0.28f), Min.X, Min.Y, Width - 60.f, 48.f);
            DrawText(Label, FLinearColor::White, Min.X + 16.f, Min.Y + 11.f, BodyFont, 1.12f);
            AddHitBox(Min, FVector2D(Width - 60.f, 48.f), Name, true, 10);
        };
        DrawPauseButton(TEXT("PauseResume"), TEXT("继续游戏（Esc）"), CenterY - 80.f);
        const bool bCanOpenMap = GS->Phase == ETreasureRoundPhase::HunterSearching
            || (PS->PlayerRole == ETreasurePlayerRole::Scout && !PS->bSketchSubmitted);
        if (bCanOpenMap) DrawPauseButton(TEXT("PauseOpenMap"), TEXT("关闭菜单并查看地图"), CenterY - 20.f);
        else DrawText(TEXT("地图暂不可打开；等待交图时画纸直接显示"), FLinearColor(0.7f, 0.8f, 0.8f),
            Left + 32.f, CenterY - 2.f, BodyFont, 0.9f);
        if (GetNetMode() == NM_ListenServer)
            DrawPauseButton(TEXT("PauseReturnToLobby"), TEXT("结束本局，全队返回大厅"), CenterY + 40.f);
        else DrawText(TEXT("只有房主可以结束本局并带全队返回大厅"), FLinearColor(0.7f, 0.8f, 0.8f),
            Left + 32.f, CenterY + 58.f, BodyFont, 0.9f);
        return;
    }

    if (GS->IsRoundOver() && !GS->bReviewingRound)
    {
        const bool bWon = GS->Phase == ETreasureRoundPhase::Won;
        const bool bRace = GS->RoomMode == ETreasureRoomMode::ExplorerRace;
        const bool bRaceFinished = bRace && GS->RaceRoundIndex >= GS->RaceTotalRounds;
        const float CenterX = Canvas->SizeX * 0.5f;
        const float CenterY = Canvas->SizeY * 0.5f;
        const float PanelWidth = FMath::Min(620.f, Canvas->SizeX * 0.82f);
        const float PanelHeight = 510.f;
        const float ButtonWidth = FMath::Min(360.f, PanelWidth - 48.f);
        const float ButtonHeight = 56.f;
        const FVector2D SameRolesButtonMin(CenterX - ButtonWidth * 0.5f, CenterY + (bRace ? 78.f : 8.f));
        const FVector2D SwapRolesButtonMin(CenterX - ButtonWidth * 0.5f, CenterY + 78.f);
        const FVector2D ReviewButtonMin(CenterX - ButtonWidth * 0.5f, CenterY + (GS->PlayerArray.Num() == 1 ? 78.f : 142.f));
        const FName SameRolesButtonName(TEXT("ReplaySameRoles"));
        const FName SwapRolesButtonName(TEXT("ReplaySwapRoles"));

        DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.94f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), CenterX - PanelWidth * 0.5f + 12.f,
            CenterY - PanelHeight * 0.5f + 14.f, PanelWidth, PanelHeight);
        DrawRect(FLinearColor(0.035f, 0.09f, 0.09f, 1.f), CenterX - PanelWidth * 0.5f,
            CenterY - PanelHeight * 0.5f, PanelWidth, PanelHeight);
        DrawRect(FLinearColor(0.91f, 0.68f, 0.27f), CenterX - PanelWidth * 0.5f,
            CenterY - PanelHeight * 0.5f, PanelWidth, 6.f);

        const FString Title = bRace ? (bWon ? FString::Printf(TEXT("本局胜者：%s"), *GS->RaceRoundWinner)
            : TEXT("本局无人找到宝藏")) : bWon ? TEXT("合作成功！") : TEXT("时间到！");
        float TextWidth = 0.f, TextHeight = 0.f;
        const float TitleScale = bRace ? 1.05f : 1.35f;
        GetTextSize(Title, TextWidth, TextHeight, DisplayFont, TitleScale);
        DrawText(Title, bWon ? FLinearColor(0.95f, 0.85f, 0.25f) : FLinearColor(1.f, 0.35f, 0.25f),
            CenterX - TextWidth * 0.5f,
            CenterY - 185.f, DisplayFont, TitleScale);

        const FString Hint = bRace ? FString::Printf(TEXT("第 %d / %d 局  |  首位找到 +2 分，地图师 +1 分"),
            GS->RaceRoundIndex, GS->RaceTotalRounds) : bWon ? TEXT("找到宝藏了！再来一座新岛屿？")
            : GS->Phase == ETreasureRoundPhase::ScoutTimedOut ? TEXT("侦察者未能及时交图，再试一次？")
            : TEXT("寻宝者未能及时找到宝藏，再试一次？");
        GetTextSize(Hint, TextWidth, TextHeight, BodyFont, 1.15f);
        DrawText(Hint, FLinearColor::White, CenterX - TextWidth * 0.5f,
            CenterY - 117.f, BodyFont, 1.15f);

        const bool bSolo = GS->PlayerArray.Num() == 1;
        FString ChoiceHint = bSolo ? TEXT("单人测试：再玩一次会开始新的岛屿")
            : TEXT("任一人选择后，全队立即开始新的一局");
        if (bRace)
        {
            int32 BestPoints = -1;
            TArray<FString> Leaders;
            for (APlayerState* State : GS->PlayerArray)
                if (const ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State))
                {
                    if (Member->RacePoints > BestPoints) { BestPoints = Member->RacePoints; Leaders.Reset(); }
                    if (Member->RacePoints == BestPoints) Leaders.Add(Member->GetPlayerName());
                }
            ChoiceHint = bRaceFinished ? FString::Printf(TEXT("竞赛结束！总分第一：%s%s"),
                *FString::Join(Leaders, TEXT("、")), Leaders.Num() > 1 ? TEXT("（并列）") : TEXT(""))
                : TEXT("下一局自动轮换地图师；任一人可继续");
        }
        GetTextSize(ChoiceHint, TextWidth, TextHeight, BodyFont, 0.95f);
        DrawText(ChoiceHint, FLinearColor(0.7f, 0.8f, 0.9f), CenterX - TextWidth * 0.5f,
            CenterY - 75.f, BodyFont, 0.95f);

        if (bRace)
        {
            TArray<const ATreasureSketchPlayerState*> Standings;
            for (APlayerState* State : GS->PlayerArray)
                if (const ATreasureSketchPlayerState* Member = Cast<ATreasureSketchPlayerState>(State)) Standings.Add(Member);
            Standings.Sort([](const ATreasureSketchPlayerState& A, const ATreasureSketchPlayerState& B)
                { return A.RacePoints > B.RacePoints; });
            for (int32 Rank = 0; Rank < Standings.Num(); ++Rank)
            {
                const ATreasureSketchPlayerState* Member = Standings[Rank];
                DrawText(FString::Printf(TEXT("%d. %s  %d 分  找到 %d 次"), Rank + 1,
                    *Member->GetPlayerName(), Member->RacePoints, Member->RaceFinds),
                    Member == PS ? FLinearColor(0.95f, 0.85f, 0.35f) : FLinearColor::White,
                    CenterX - PanelWidth * 0.5f + 80.f, CenterY - 47.f + Rank * 24.f,
                    BodyFont, 0.95f);
            }
        }

        const bool bSameRolesHovered = HitBoxesOver.Contains(SameRolesButtonName);
        DrawRect(bSameRolesHovered ? FLinearColor(0.24f, 0.69f, 0.52f) : FLinearColor(0.16f, 0.52f, 0.40f),
            SameRolesButtonMin.X, SameRolesButtonMin.Y, ButtonWidth, ButtonHeight);
        const FString SameRolesText = bRace ? (bRaceFinished ? TEXT("重新开始竞赛") : TEXT("下一局：轮换地图师"))
            : TEXT("保持角色，再玩一次");
        GetTextSize(SameRolesText, TextWidth, TextHeight, DisplayFont, 1.f);
        DrawText(SameRolesText, FLinearColor::White, CenterX - TextWidth * 0.5f,
            SameRolesButtonMin.Y + (ButtonHeight - TextHeight) * 0.5f, DisplayFont, 1.f);
        AddHitBox(SameRolesButtonMin, FVector2D(ButtonWidth, ButtonHeight), SameRolesButtonName, true, 0);

        if (GetNetMode() != NM_Client)
        {
            const FVector2D SetupMin(CenterX - ButtonWidth * 0.5f, CenterY + (bSolo ? 142.f : 207.f));
            DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), SetupMin.X, SetupMin.Y, ButtonWidth, 40.f);
            const FString Label = bSolo ? TEXT("返回测试菜单") : TEXT("全队返回大厅");
            GetTextSize(Label, TextWidth, TextHeight, BodyFont, 1.f);
            DrawText(Label, FLinearColor::White, CenterX - TextWidth * 0.5f, SetupMin.Y + 9.f, BodyFont, 1.f);
            AddHitBox(SetupMin, FVector2D(ButtonWidth, 40.f), TEXT("ReturnToSetup"), true, 0);
        }
        else
        {
            const FString Label = TEXT("由房主选择让全队返回大厅");
            GetTextSize(Label, TextWidth, TextHeight, BodyFont, 1.f);
            DrawText(Label, FLinearColor(0.7f, 0.8f, 0.9f), CenterX - TextWidth * 0.5f,
                CenterY + 213.f, BodyFont, 1.f);
        }

        if (!bSolo && !bRace)
        {
            const bool bSwapRolesHovered = HitBoxesOver.Contains(SwapRolesButtonName);
            DrawRect(bSwapRolesHovered ? FLinearColor(0.30f, 0.57f, 0.82f) : FLinearColor(0.20f, 0.42f, 0.67f),
                SwapRolesButtonMin.X, SwapRolesButtonMin.Y, ButtonWidth, ButtonHeight);
            const FString SwapRolesText = GS->RoomMode == ETreasureRoomMode::OneExplorer
                ? TEXT("轮换探索者，再玩一次") : TEXT("轮换地图师，再玩一次");
            GetTextSize(SwapRolesText, TextWidth, TextHeight, DisplayFont, 1.f);
            DrawText(SwapRolesText, FLinearColor::White, CenterX - TextWidth * 0.5f,
                SwapRolesButtonMin.Y + (ButtonHeight - TextHeight) * 0.5f, DisplayFont, 1.f);
            AddHitBox(SwapRolesButtonMin, FVector2D(ButtonWidth, ButtonHeight), SwapRolesButtonName, true, 0);
        }
        DrawRect(HitBoxesOver.Contains(TEXT("BeginRoundReview")) ? FLinearColor(0.65f, 0.48f, 0.18f) : FLinearColor(0.48f, 0.34f, 0.13f),
            ReviewButtonMin.X, ReviewButtonMin.Y, ButtonWidth, 50.f);
        const FString ReviewText = TEXT("留在岛上复盘");
        GetTextSize(ReviewText, TextWidth, TextHeight, DisplayFont, 1.f);
        DrawText(ReviewText, FLinearColor::White, CenterX - TextWidth * 0.5f,
            ReviewButtonMin.Y + (50.f - TextHeight) * 0.5f, DisplayFont, 1.f);
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
    const FString Help = GS->bReviewingRound ? TEXT("WASD 逛岛  ·  M 查看地图  ·  T 宝箱标记  ·  Esc 返回结算")
        : bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing && PC->IsDrawingOverheadView()
        ? (PC->HasSubmittedSketch() ? TEXT("WASD 飞行  ·  Space / Ctrl 升降  ·  Tab 返回地面")
            : TEXT("WASD 飞行  ·  Space / Ctrl 升降  ·  Tab 返回地面  ·  M 画图"))
        : PC->IsScoutSpectating()
        ? TEXT("WASD 飞行  ·  Space / Ctrl 升降  ·  M 查看地图")
        : bScout && PC->HasSubmittedSketch() ? TEXT("地图已经交付；等待其他地图师完成")
        : bScout ? TEXT("Tab 俯视侦察  ·  M 打开画纸  ·  C 清空  ·  Enter 交图")
        : GS->Phase == ETreasureRoundPhase::HunterSearching && GS->RoomMode == ETreasureRoomMode::ExplorerRace
            ? TEXT("M 查看地图  ·  E 挖掘  ·  G 推开对手")
        : GS->Phase == ETreasureRoundPhase::HunterSearching ? TEXT("M 查看地图  ·  E 挖掘")
        : TEXT("等待地图师交图；收到后按 M 查看地图");
    const FString Objective = GS->bReviewingRound ? TEXT("自由复盘刚才的岛屿")
        : bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing && PC->HasSubmittedSketch()
            ? TEXT("等待其他地图师交图")
        : bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing
            ? TEXT("观察岛屿并绘制藏宝图")
        : GS->Phase == ETreasureRoundPhase::HunterSearching && GS->RoomMode == ETreasureRoomMode::ExplorerRace
            ? TEXT("抢先找到宝藏")
        : GS->Phase == ETreasureRoundPhase::HunterSearching
            ? TEXT("根据手绘地图找到宝藏")
        : TEXT("等待地图交付");
    const FString IdentityLine = FString::Printf(TEXT("%s  ·  岛屿种子 %d"), *RoleLabel, GS->IslandSeed);
    float IdentityW = 0.f, IdentityH = 0.f;
    float ObjectiveW = 0.f, ObjectiveH = 0.f;
    float HelpW = 0.f, HelpH = 0.f;
    GetTextSize(IdentityLine, IdentityW, IdentityH, BodyFont, 0.90f);
    GetTextSize(Objective, ObjectiveW, ObjectiveH, DisplayFont, 1.04f);
    GetTextSize(Help, HelpW, HelpH, BodyFont, 0.86f);
    const float MissionPanelWidth = FMath::Clamp(FMath::Max3(IdentityW, ObjectiveW, HelpW) + 42.f,
        300.f, FMath::Min(720.f, Canvas->SizeX * 0.55f));
    const float MissionPanelHeight = IdentityH + ObjectiveH + HelpH + 30.f;
    const float IdentityY = 22.f;
    const float ObjectiveY = IdentityY + IdentityH + 2.f;
    const float HelpY = ObjectiveY + ObjectiveH + 3.f;
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.20f), 27.f, 23.f, MissionPanelWidth, MissionPanelHeight);
    DrawRect(FLinearColor(0.012f, 0.045f, 0.050f, 0.88f), 18.f, 14.f, MissionPanelWidth, MissionPanelHeight);
    DrawRect(FLinearColor(1.f, 0.59f, 0.08f, 1.f), 18.f, 14.f, 5.f, MissionPanelHeight);
    DrawRect(FLinearColor(1.f, 0.59f, 0.08f, 0.55f), 23.f, 14.f, MissionPanelWidth - 5.f, 2.f);
    DrawReadableText(IdentityLine, FLinearColor(0.24f, 0.98f, 0.75f),
        36.f, IdentityY, BodyFont, 0.90f, false);
    DrawReadableText(Objective, FLinearColor(1.f, 0.68f, 0.13f),
        36.f, ObjectiveY, DisplayFont, 1.04f, true);
    DrawReadableText(Help, FLinearColor(1.f, 0.97f, 0.84f),
        36.f, HelpY, BodyFont, 0.86f, false);
    const float ContextY = 24.f + MissionPanelHeight;
    if (PC->IsScoutSpectating() && GS->Phase == ETreasureRoundPhase::HunterSearching)
    {
        DrawText(TEXT("鼠标转向 | Tab 切换视角 | Q 切换探索者 | T 宝藏"),
            FLinearColor(1.f, 0.97f, 0.84f), 35.f, ContextY, BodyFont, 0.95f);
        DrawText(FString::Printf(TEXT("%s  |  宝藏标记：%s"),
            PC->IsHunterFirstPersonView() ? TEXT("寻宝者第一视角") : TEXT("自由飞行"),
            PC->IsSpectatorTreasureVisible() ? TEXT("显示") : TEXT("隐藏")),
            FLinearColor(0.24f, 0.98f, 0.75f), 35.f, ContextY + 30.f, BodyFont, 0.95f);
    }
    if (!bScout && GS->Phase == ETreasureRoundPhase::HunterSearching)
    {
        const int32 CooldownRemaining = FMath::CeilToInt(PS->GetDigCooldownRemaining(
            GS->RoundSerial, GS->GetServerWorldTimeSeconds()));
        const FString DigStatus = CooldownRemaining > 0 ? FString::Printf(TEXT("挖掘冷却：%d 秒"), CooldownRemaining)
            : GS->DigCooldownSeconds == 0 ? TEXT("挖掘就绪（无冷却） · E") : TEXT("挖掘就绪 · E");
        float DigStatusW = 0.f, DigStatusH = 0.f;
        GetTextSize(DigStatus, DigStatusW, DigStatusH, BodyFont, 0.96f);
        const float DigPanelW = DigStatusW + 34.f;
        const float DigPanelY = ContextY;
        DrawRect(FLinearColor(0.015f, 0.065f, 0.055f, 0.86f), 18.f, DigPanelY, DigPanelW, DigStatusH + 16.f);
        DrawRect(CooldownRemaining > 0 ? FLinearColor(1.f, 0.42f, 0.08f) : FLinearColor(0.20f, 0.95f, 0.61f),
            18.f, DigPanelY, 4.f, DigStatusH + 16.f);
        DrawReadableText(DigStatus,
            CooldownRemaining > 0 ? FLinearColor(1.f, 0.72f, 0.28f) : FLinearColor(0.42f, 1.f, 0.72f),
            32.f, DigPanelY + 7.f, BodyFont, 0.96f, false);
        if (GS->RoomMode == ETreasureRoomMode::ExplorerRace)
        {
            const int32 ShoveCooldown = FMath::CeilToInt(FMath::Max(0.f,
                PS->NextShoveServerTime - GS->GetServerWorldTimeSeconds()));
            DrawText(ShoveCooldown > 0 ? FString::Printf(TEXT("推人冷却：%d 秒"), ShoveCooldown)
                : TEXT("推人就绪：靠近并面向对手按 G"), FLinearColor(0.95f, 0.75f, 0.40f),
                35.f, DigPanelY + DigStatusH + 24.f, BodyFont, 1.f);
            const int32 ProtectionRemaining = FMath::CeilToInt(FMath::Max(0.f,
                PS->ShoveProtectedUntilServerTime - GS->GetServerWorldTimeSeconds()));
            if (ProtectionRemaining > 0)
                DrawText(FString::Printf(TEXT("防连续推：%d 秒"), ProtectionRemaining),
                    FLinearColor(0.55f, 0.9f, 1.f), 35.f, DigPanelY + DigStatusH + 54.f, BodyFont, 1.f);
        }
    }

    if (GS->bSurfacePaintEnabled && bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing && !PC->IsMapOpen())
    {
        if (!PC->HasSubmittedSketch() && !PC->IsDrawingOverheadView())
        {
            DrawLine(Canvas->SizeX * 0.5f - 7.f, Canvas->SizeY * 0.5f, Canvas->SizeX * 0.5f + 7.f, Canvas->SizeY * 0.5f, FLinearColor(0.1f, 0.6f, 1.f), 2.f);
            DrawLine(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f - 7.f, Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f + 7.f, FLinearColor(0.1f, 0.6f, 1.f), 2.f);
        }
        DrawText(FString::Printf(TEXT("喷漆剩余：%d / %d%s"),
            FMath::Clamp(ATreasureSurfacePaint::MaxStamps - GS->SurfacePaintStampsUsed, 0, ATreasureSurfacePaint::MaxStamps),
            ATreasureSurfacePaint::MaxStamps, PC->HasSubmittedSketch() ? TEXT("（已交图）")
                : PC->IsDrawingOverheadView() ? TEXT("（返回地面后右键使用）") : TEXT("（按住右键使用）")),
            FLinearColor(0.25f, 0.75f, 1.f), 35.f, ContextY + 8.f,
            BodyFont, 0.95f);
    }
    const int32 SecondsRemaining = GS->GetSecondsRemaining();
    const FString TimerText = FString::Printf(TEXT("%02d:%02d"), SecondsRemaining / 60, SecondsRemaining % 60);
    const FLinearColor TimerColor = SecondsRemaining <= 10 ? FLinearColor(1.f, 0.24f, 0.16f) : FLinearColor(1.f, 0.94f, 0.73f);
    constexpr float TimerScale = 1.08f;
    float TimerTextW = 0.f, TimerTextH = 0.f;
    GetTextSize(TimerText, TimerTextW, TimerTextH, DisplayFont, TimerScale);
    const float TimerWidth = TimerTextW + 34.f;
    const float TimerHeight = FMath::Max(50.f, TimerTextH + 16.f);
    const float TimerX = Canvas->SizeX - TimerWidth - 22.f;
    if (!GS->bReviewingRound)
    {
        DrawRect(FLinearColor(0.018f, 0.050f, 0.055f, 0.88f), TimerX, 16.f, TimerWidth, TimerHeight);
        DrawRect(FLinearColor(1.f, 0.59f, 0.08f), TimerX, 16.f, 5.f, TimerHeight);
        DrawReadableText(TimerText, TimerColor, TimerX + (TimerWidth - TimerTextW) * 0.5f,
            16.f + (TimerHeight - TimerTextH) * 0.5f, DisplayFont, TimerScale, true);
    }
    if (!GS->bReviewingRound && GS->RoomMode == ETreasureRoomMode::OneExplorer && GS->Phase == ETreasureRoundPhase::ScoutDrawing)
        DrawText(FString::Printf(TEXT("已交图 %d / %d"), Submitted, Mapmakers), FLinearColor::White,
            TimerX, 16.f + TimerHeight + 8.f, BodyFont, 0.95f);

    if (!PC->GetStatusMessage().IsEmpty())
    {
        DrawRect(FLinearColor(0.04f, 0.075f, 0.075f, 0.90f), 22.f, Canvas->SizeY - 96.f,
            FMath::Min(760.f, Canvas->SizeX * 0.62f), 60.f);
        DrawRect(FLinearColor(0.91f, 0.68f, 0.27f), 22.f, Canvas->SizeY - 96.f, 6.f, 60.f);
        DrawText(PC->GetStatusMessage(), FLinearColor::Yellow, 35.f, Canvas->SizeY - 82.f, DisplayFont, 1.1f);
    }

    const bool bWatchingLiveSketch = PC->IsHunterWaiting();
    auto DrawReviewHint = [&]()
    {
        if (!GS->bReviewingRound) return;
        const FVector2D ButtonMin(Canvas->SizeX - 244.f, 22.f);
        DrawRect(FLinearColor(0.48f, 0.29f, 0.13f), ButtonMin.X, ButtonMin.Y, 220.f, 46.f);
        DrawText(TEXT("按 Esc 返回结算"), FLinearColor::White, ButtonMin.X + 12.f, ButtonMin.Y + 10.f, BodyFont, 0.95f);
        const FVector2D ToggleMin(ButtonMin.X, ButtonMin.Y + 54.f);
        if (PC->IsMapOpen())
        {
            DrawRect(HitBoxesOver.Contains(TEXT("ToggleReviewTreasure")) ? FLinearColor(0.22f, 0.54f, 0.47f)
                : FLinearColor(0.13f, 0.35f, 0.31f), ToggleMin.X, ToggleMin.Y, 220.f, 42.f);
            DrawText(PC->IsSpectatorTreasureVisible() ? TEXT("隐藏宝箱标记") : TEXT("显示宝箱标记"),
                FLinearColor::White, ToggleMin.X + 12.f, ToggleMin.Y + 9.f, BodyFont, 0.95f);
            AddHitBox(ToggleMin, FVector2D(220.f, 42.f), TEXT("ToggleReviewTreasure"), true, 10);
        }
        else DrawText(TEXT("按 T 显示／隐藏宝箱"), FLinearColor::White,
            ToggleMin.X + 12.f, ToggleMin.Y + 9.f, BodyFont, 0.9f);
    };
    if (!PC->IsMapOpen() && !bWatchingLiveSketch) { DrawReviewHint(); return; }
    if (bWatchingLiveSketch)
        DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.96f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
    const FVector2D Min = PC->GetPaperMin();
    const FVector2D Size = PC->GetPaperSize();
    DrawRect(FLinearColor(0.18f, 0.11f, 0.05f, 0.75f), Min.X - 8.f, Min.Y - 8.f, Size.X + 16.f, Size.Y + 16.f);
    DrawRect(FLinearColor(0.96f, 0.92f, 0.78f, 0.99f), Min.X, Min.Y, Size.X, Size.Y);
    DrawRect(FLinearColor(0.53f, 0.36f, 0.14f, 0.75f), Min.X, Min.Y, Size.X, 4.f);
    const FString PaperTitle = GS->bReviewingRound ? TEXT("复盘地图 · 可对照宝藏位置")
        : bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing ? TEXT("空白纸：请画岛屿轮廓、地形地标和藏宝点")
        : bWatchingLiveSketch ? TEXT("地图师的实时画纸 · 只能观看") : TEXT("地图师留下的手绘地图");
    const FString PaintCounter = GS->bSurfacePaintEnabled && bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing
        ? FString::Printf(TEXT("  |  喷漆剩余 %d / %d"),
            FMath::Clamp(ATreasureSurfacePaint::MaxStamps - GS->SurfacePaintStampsUsed, 0, ATreasureSurfacePaint::MaxStamps),
            ATreasureSurfacePaint::MaxStamps) : FString();
    DrawText(PaperTitle + PaintCounter,
        FLinearColor::Black, Min.X + 18.f, Min.Y + 14.f, BodyFont, 1.f);
    if (bWatchingLiveSketch && PC->GetSketchPageCount() == 0)
        DrawText(TEXT("正在接收地图师画纸……"), FLinearColor(0.30f, 0.33f, 0.35f),
            Min.X + 18.f, Min.Y + 52.f, BodyFont, 1.f);

    for (const FSketchStroke& Stroke : PC->GetStrokes())
    {
        for (int32 I = 1; I < Stroke.Points.Num(); ++I)
        {
            const FVector2D A = Min + Stroke.Points[I-1] * Size;
            const FVector2D B = Min + Stroke.Points[I] * Size;
            DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(0.08f,0.07f,0.05f), 4.f);
        }
    }
    if ((!bScout || GS->bReviewingRound || GS->Phase == ETreasureRoundPhase::HunterSearching)
        && PC->GetSketchPageCount() > 0)
    {
        const float FooterY = Min.Y + Size.Y + 4.f;
        DrawRect(FLinearColor(0.96f, 0.94f, 0.86f), Min.X, FooterY, Size.X, 36.f);
        const FString PageLabel = bWatchingLiveSketch
            ? FString::Printf(TEXT("实时图纸 %d / %d · %s · 已交图 %d / %d"), PC->GetActiveSketchPage() + 1,
                PC->GetSketchPageCount(), *PC->GetActiveMapmakerName(), Submitted, Mapmakers)
            : FString::Printf(TEXT("图纸 %d / %d · %s"), PC->GetActiveSketchPage() + 1,
                PC->GetSketchPageCount(), *PC->GetActiveMapmakerName());
        DrawText(PageLabel, FLinearColor::Black, Min.X + 18.f, FooterY + 8.f, BodyFont, 0.9f);
        if (PC->GetSketchPageCount() > 1)
            for (int32 I = 0; I < 2; ++I)
            {
                const FVector2D ButtonMin(Min.X + Size.X - 244.f + I * 120.f, FooterY);
                const FName Name = I == 0 ? TEXT("PreviousSketchPage") : TEXT("NextSketchPage");
                DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), ButtonMin.X, ButtonMin.Y, 110.f, 36.f);
                DrawText(I == 0 ? TEXT("上一张 ←") : TEXT("下一张 →"), FLinearColor::White, ButtonMin.X + 8.f, ButtonMin.Y + 8.f, BodyFont, 0.86f);
                AddHitBox(ButtonMin, FVector2D(110.f, 36.f), Name, true, 10);
            }
    }
    if (bWatchingLiveSketch)
    {
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.78f), Canvas->SizeX - TimerWidth - 195.f, 18.f, TimerWidth + 173.f, 42.f);
        DrawText(FString::Printf(TEXT("绘图剩余 %s · 到时自动交图"), *TimerText), TimerColor,
            Canvas->SizeX - TimerWidth - 185.f, 25.f, BodyFont, 0.95f);
    }
    DrawReviewHint();
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
