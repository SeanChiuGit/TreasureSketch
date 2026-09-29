#include "TreasureSketchHUD.h"
#include "TreasureSurfacePaint.h"
#include "ProceduralIsland.h"
#include "TreasureSketchCharacter.h"

#include "TreasureSketchGameState.h"
#include "TreasureSketchPlayerController.h"
#include "TreasureSketchPlayerState.h"
#include "TreasureOnlineSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "ImageUtils.h"

namespace
{
FLinearColor SketchInkColor(uint8 Index)
{
    switch (Index)
    {
    case 1: return FLinearColor(0.78f, 0.12f, 0.12f);
    case 2: return FLinearColor(0.08f, 0.27f, 0.78f);
    case 3: return FLinearColor(0.08f, 0.54f, 0.28f);
    case 4: return FLinearColor(0.82f, 0.51f, 0.08f);
    case 5: return FLinearColor(0.96f, 0.94f, 0.86f);
    default: return FLinearColor(0.08f, 0.07f, 0.05f);
    }
}
}

UTexture2D* ATreasureSketchHUD::GetPhotoTexture(const TArray<uint8>& PhotoJpeg)
{
    if (CachedPhotoJpeg != PhotoJpeg)
    {
        CachedPhotoJpeg = PhotoJpeg;
        CachedPhotoTexture = PhotoJpeg.IsEmpty() ? nullptr : FImageUtils::ImportBufferAsTexture2D(PhotoJpeg);
    }
    return CachedPhotoTexture;
}

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
                DrawRect(FLinearColor(0.06f, 0.12f, 0.13f), SettingsX, Y, SettingsW, 30.f);
                DrawText(Label, FLinearColor::White, SettingsX + 10.f, Y + 8.f, GEngine->GetSmallFont(), 0.9f);
                const float ButtonX = SettingsX + SettingsW - 84.f;
                for (int32 I = 0; I < 2; ++I)
                {
                    const bool bEnabled = bCanAdjustSettings && (I == 0 ? bCanLess : bCanMore);
                    const float X = ButtonX + I * 42.f;
                    DrawRect(bEnabled ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.08f, 0.12f, 0.13f), X, Y + 2.f, 38.f, 26.f);
                    DrawText(I == 0 ? TEXT("-") : TEXT("+"), bEnabled ? FLinearColor::White : FLinearColor(0.35f, 0.40f, 0.40f), X + 13.f, Y + 4.f, GEngine->GetMediumFont(), 1.f);
                    if (bEnabled) AddHitBox(FVector2D(X, Y + 2.f), FVector2D(38.f, 26.f), I == 0 ? Less : More, true, 10);
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
            DrawRect(bEditingScale ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.08f, 0.12f, 0.13f), InputX, SettingsY + 2.f, InputW, 26.f);
            DrawText(ScaleText + (bEditingScale ? TEXT("_") : TEXT(" 倍")), bValidScale ? FLinearColor::White : FLinearColor(1.f, 0.4f, 0.3f),
                InputX + 8.f, SettingsY + 8.f, GEngine->GetSmallFont(), 0.9f);
            if (bCanAdjustSettings) AddHitBox(FVector2D(InputX, SettingsY + 2.f), FVector2D(InputW, 26.f), TEXT("MapScaleInput"), true, 10);
            DrawRoomSetting(FString::Printf(TEXT("绘图时间：%d 秒"), GS->DrawingDurationSeconds), SettingsY + 30.f, TEXT("RoomDrawingLess"), TEXT("RoomDrawingMore"), GS->DrawingDurationSeconds > GS->MinPhaseSeconds, GS->DrawingDurationSeconds < GS->MaxPhaseSeconds);
            DrawRoomSetting(FString::Printf(TEXT("寻宝时间：%d 秒"), GS->SearchingDurationSeconds), SettingsY + 60.f, TEXT("RoomSearchingLess"), TEXT("RoomSearchingMore"), GS->SearchingDurationSeconds > GS->MinPhaseSeconds, GS->SearchingDurationSeconds < GS->MaxPhaseSeconds);
            DrawRoomSetting(FString::Printf(TEXT("挖掘冷却：%d 秒"), GS->DigCooldownSeconds), SettingsY + 90.f,
                TEXT("RoomDigCooldownLess"), TEXT("RoomDigCooldownMore"),
                GS->DigCooldownSeconds > GS->MinDigCooldownSeconds, GS->DigCooldownSeconds < GS->MaxDigCooldownSeconds);
            DrawRoomSetting(FString::Printf(TEXT("移动速度：%.2f 倍"), GS->MovementSpeedMultiplier), SettingsY + 120.f, TEXT("RoomSpeedLess"), TEXT("RoomSpeedMore"), GS->MovementSpeedMultiplier > GS->MinMovementSpeed, GS->MovementSpeedMultiplier < GS->MaxMovementSpeed);
            auto DrawRoomToggle = [&](FName Name, const FString& Label, float Y)
            {
                DrawRect(bCanAdjustSettings ? FLinearColor(0.15f, 0.30f, 0.30f) : FLinearColor(0.08f, 0.12f, 0.13f), SettingsX, Y, SettingsW, 30.f);
                DrawText(Label, FLinearColor::White, SettingsX + 10.f, Y + 8.f, GEngine->GetSmallFont(), 0.9f);
                if (bCanAdjustSettings) AddHitBox(FVector2D(SettingsX, Y), FVector2D(SettingsW, 30.f), Name, true, 10);
            };
            DrawRoomToggle(TEXT("ToggleTreasureRange"), GS->bTreasureRangeVisible ? TEXT("宝藏判定范围：显示（点击切换）") : TEXT("宝藏判定范围：隐藏（点击切换）"), SettingsY + 150.f);
            DrawRoomToggle(TEXT("ToggleSpreadPlayerSpawns"), GS->bSpreadPlayerSpawns ? TEXT("出生点：分散登岛（点击切换）") : TEXT("出生点：同一区域（点击切换）"), SettingsY + 180.f);
        };

        const EFrontEndPage Page = PC->GetFrontEndPage();
        if (Page == EFrontEndPage::MainMenu)
        {
            float Y = PanelY + 155.f;
            DrawMenuButton(TEXT("MenuCreate"), TEXT("创建房间"), Y, true); Y += 60.f;
            DrawMenuButton(TEXT("MenuJoin"), TEXT("加入房间"), Y); Y += 60.f;
            DrawMenuButton(TEXT("MenuSolo"), TEXT("单人探险"), Y); Y += 60.f;
            DrawMenuButton(TEXT("MenuHistory"), TEXT("游玩历史"), Y); Y += 60.f;
            DrawMenuButton(TEXT("MenuSettings"), TEXT("设置"), Y); Y += 60.f;
            DrawMenuButton(TEXT("MenuQuit"), TEXT("退出游戏"), Y);
            DrawText(TEXT("2至4人合作寻宝"), FLinearColor(0.55f, 0.70f, 0.68f),
                PanelX + 48.f, PanelY + PanelH - 45.f, GEngine->GetSmallFont(), 0.9f);
        }
        else if (Page == EFrontEndPage::History)
        {
            const TArray<FPlayedRoundRecord>& Records = PC->GetHistoryRecords();
            const FPlayedRoundRecord* Selected = PC->GetSelectedHistoryRecord();
            const float RightX = PanelX + PanelW + 24.f;
            const float RightW = W - RightX - 24.f;
            DrawText(Selected ? TEXT("历史详情") : TEXT("游玩历史"), FLinearColor::White,
                PanelX + 48.f, PanelY + 155.f, GEngine->GetLargeFont(), 1.2f);
            if (!Selected)
            {
                if (Records.IsEmpty())
                    DrawText(TEXT("还没有完成的对局"), FLinearColor(0.75f, 0.84f, 0.82f),
                        PanelX + 48.f, PanelY + 225.f, GEngine->GetMediumFont(), 1.f);
                for (int32 Row = 0; Row < 5; ++Row)
                {
                    const int32 Index = PC->GetHistoryGroupStart(PC->GetHistoryListOffset() + Row);
                    if (!Records.IsValidIndex(Index)) break;
                    const FPlayedRoundRecord& Record = Records[Index];
                    const FString TimeText = Record.LocalTimeText.IsEmpty()
                        ? Record.UtcTimeIso.Left(16) : Record.LocalTimeText;
                    int32 SavedRounds = 0;
                    if (!Record.SeriesId.IsEmpty())
                        for (const FPlayedRoundRecord& Candidate : Records)
                            SavedRounds += Candidate.SeriesId == Record.SeriesId;
                    const FString Label = Record.SeriesId.IsEmpty()
                        ? FString::Printf(TEXT("%s  种子 %d"), *TimeText, Record.IslandSeed)
                        : FString::Printf(TEXT("%s  对抗赛 %d/%d 局"),
                            *TimeText, SavedRounds, Record.RaceTotalRounds);
                    DrawMenuButton(FName(*FString::Printf(TEXT("HistorySelect%d"), Row)), Label,
                        PanelY + 200.f + Row * 55.f, Row == 0);
                }
                const float NavY = PanelY + PanelH - 100.f;
                if (PC->GetHistoryListOffset() > 0)
                    DrawMenuButton(TEXT("HistoryPrevList"), TEXT("上一页"), NavY);
                if (PC->GetHistoryListOffset() + 5 < PC->GetHistoryGroupCount())
                {
                    const FVector2D NavMin(RightX, NavY);
                    DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), NavMin.X, NavMin.Y, 145.f, 40.f);
                    DrawText(TEXT("下一页 →"), FLinearColor::White, NavMin.X + 15.f, NavMin.Y + 10.f,
                        GEngine->GetSmallFont(), 1.f);
                    AddHitBox(NavMin, FVector2D(145.f, 40.f), TEXT("HistoryNextList"), true, 10);
                }
            }
            else
            {
                const FString TimeText = Selected->LocalTimeText.IsEmpty()
                    ? Selected->UtcTimeIso.Left(16) : Selected->LocalTimeText;
                const TCHAR* ThemeName = Selected->Theme == EIslandTheme::MistForest ? TEXT("雾森林")
                    : Selected->Theme == EIslandTheme::JungleRuins ? TEXT("遗迹") : TEXT("海盗沙滩");
                const TCHAR* ModeName = Selected->RoomMode == ETreasureRoomMode::ExplorerRace ? TEXT("探索者对抗")
                    : Selected->RoomMode == ETreasureRoomMode::OneExplorer ? TEXT("多地图师") : TEXT("合作寻宝");
                const TCHAR* RoleName = Selected->LocalRole == ETreasurePlayerRole::Scout ? TEXT("地图师") : TEXT("探索者");
                const FString Outcome = Selected->Outcome == ETreasureRoundPhase::Won
                    ? Selected->WinnerName.IsEmpty() ? TEXT("找到宝藏")
                        : FString::Printf(TEXT("%s 找到宝藏"), *Selected->WinnerName)
                    : TEXT("寻宝超时");
                TArray<FString> Lines = {
                    TimeText,
                    FString::Printf(TEXT("种子 %d · %s · %.2f 倍"), Selected->IslandSeed, ThemeName, Selected->MapScale),
                    FString::Printf(TEXT("%s · 你是%s"), ModeName, RoleName),
                    Outcome,
                    FString::Printf(TEXT("寻宝用时 %.1f 秒"), Selected->SearchSeconds)
                };
                if (Selected->RoomMode == ETreasureRoomMode::ExplorerRace)
                {
                    Lines.Add(FString::Printf(TEXT("第 %d / %d 局"), Selected->RaceRoundIndex, Selected->RaceTotalRounds));
                    Lines.Add(FString::Printf(TEXT("本局 %+d 分 · 累计 %d 分"),
                        Selected->RaceRoundPoints, Selected->RaceTotalPoints));
                }
                for (int32 I = 0; I < Lines.Num(); ++I)
                    DrawText(Lines[I], FLinearColor(0.88f, 0.94f, 0.91f), PanelX + 48.f,
                        PanelY + 215.f + I * 38.f, GEngine->GetSmallFont(), 1.f);
                if (!Selected->SeriesId.IsEmpty())
                {
                    const float SwitchY = PanelY + PanelH - 112.f;
                    const float SwitchW = (PanelW - 106.f) * 0.5f;
                    for (int32 Direction = 0; Direction < 2; ++Direction)
                    {
                        const float SwitchX = PanelX + 48.f + Direction * (SwitchW + 10.f);
                        const FName Name = Direction == 0 ? TEXT("HistoryPrevRound") : TEXT("HistoryNextRound");
                        DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), SwitchX, SwitchY, SwitchW, 42.f);
                        DrawText(Direction == 0 ? TEXT("上一局") : TEXT("下一局"), FLinearColor::White,
                            SwitchX + 12.f, SwitchY + 11.f, GEngine->GetSmallFont());
                        AddHitBox(FVector2D(SwitchX, SwitchY), FVector2D(SwitchW, 42.f), Name, true, 10);
                    }
                }
                if (RightW > 120.f)
                {
                    const float PaperW = FMath::Min(RightW - 20.f, H * 0.80f);
                    const float PaperH = PaperW / 1.74f;
                    const FVector2D PaperMin(RightX + (RightW - PaperW) * 0.5f, PanelY + 150.f);
                    const FVector2D PaperExtent(PaperW, PaperH);
                    DrawRect(SketchInkColor(5), PaperMin.X, PaperMin.Y, PaperW, PaperH);
                    constexpr int32 Samples = 48;
                    if (Selected->IslandTemplateMask.Num() == Samples * Samples)
                    {
                        const float ToolbarH = PaperH * 0.13f;
                        const float TemplateSize = FMath::Min(PaperW, PaperH - ToolbarH) * 0.9f;
                        const FVector2D Origin(PaperMin.X + (PaperW - TemplateSize) * 0.5f,
                            PaperMin.Y + ToolbarH + (PaperH - ToolbarH - TemplateSize) * 0.5f);
                        const float Cell = TemplateSize / Samples;
                        const FLinearColor Coast(0.53f, 0.45f, 0.32f, 0.72f);
                        for (int32 Y = 0; Y < Samples; ++Y)
                            for (int32 X = 0; X < Samples; ++X)
                                if (Selected->IslandTemplateMask[Y * Samples + X])
                                {
                                    const float Left = Origin.X + X * Cell, Top = Origin.Y + Y * Cell;
                                    if (X == 0 || !Selected->IslandTemplateMask[Y * Samples + X - 1])
                                        DrawLine(Left, Top, Left, Top + Cell, Coast, 1.f);
                                    if (X == Samples - 1 || !Selected->IslandTemplateMask[Y * Samples + X + 1])
                                        DrawLine(Left + Cell, Top, Left + Cell, Top + Cell, Coast, 1.f);
                                    if (Y == 0 || !Selected->IslandTemplateMask[(Y - 1) * Samples + X])
                                        DrawLine(Left, Top, Left + Cell, Top, Coast, 1.f);
                                    if (Y == Samples - 1 || !Selected->IslandTemplateMask[(Y + 1) * Samples + X])
                                        DrawLine(Left, Top + Cell, Left + Cell, Top + Cell, Coast, 1.f);
                                }
                    }
                    const int32 PageIndex = PC->GetHistorySketchPageIndex();
                    if (Selected->Pages.IsValidIndex(PageIndex))
                    {
                        const FSketchPage& Sketch = Selected->Pages[PageIndex];
                        for (const FSketchStroke& Stroke : Sketch.Strokes)
                            for (int32 I = 1; I < Stroke.Points.Num(); ++I)
                            {
                                const FVector2D A = PaperMin + Stroke.Points[I - 1] * PaperExtent;
                                const FVector2D B = PaperMin + Stroke.Points[I] * PaperExtent;
                                const float Width = Stroke.ColorIndex == 5
                                    ? (Stroke.EraserSize ? 48.f : 22.f) : 4.f;
                                DrawLine(A.X, A.Y, B.X, B.Y, SketchInkColor(Stroke.ColorIndex),
                                    FMath::Max(1.f, Width * PaperW / 1000.f));
                            }
                        if (UTexture2D* HistoryPhoto = GetPhotoTexture(Sketch.PhotoJpeg))
                        {
                            const float PhotoW = FMath::Min(260.f, PaperW * 0.36f);
                            const float PhotoH = PhotoW * 9.f / 16.f;
                            DrawRect(FLinearColor::Black, PaperMin.X + PaperW - PhotoW - 16.f,
                                PaperMin.Y + 76.f, PhotoW + 8.f, PhotoH + 8.f);
                            DrawTexture(HistoryPhoto, PaperMin.X + PaperW - PhotoW - 12.f,
                                PaperMin.Y + 80.f, PhotoW, PhotoH, 0.f, 0.f, 1.f, 1.f);
                        }
                        DrawText(FString::Printf(TEXT("图纸 %d / %d · %s"), PageIndex + 1,
                            Selected->Pages.Num(), *Sketch.MapmakerName), FLinearColor::White,
                            RightX + 12.f, PaperMin.Y + PaperH + 20.f, GEngine->GetSmallFont(), 1.f);
                        if (Selected->Pages.Num() > 1)
                        {
                            const float SwitchY = PaperMin.Y + PaperH + 46.f;
                            DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), RightX, SwitchY, 110.f, 32.f);
                            DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), RightX + 120.f, SwitchY, 110.f, 32.f);
                            DrawText(TEXT("上一张"), FLinearColor::White, RightX + 12.f, SwitchY + 8.f, GEngine->GetSmallFont());
                            DrawText(TEXT("下一张"), FLinearColor::White, RightX + 132.f, SwitchY + 8.f, GEngine->GetSmallFont());
                            AddHitBox(FVector2D(RightX, SwitchY), FVector2D(110.f, 32.f), TEXT("HistoryPrevSketch"), true, 10);
                            AddHitBox(FVector2D(RightX + 120.f, SwitchY), FVector2D(110.f, 32.f), TEXT("HistoryNextSketch"), true, 10);
                        }
                    }
                }
            }
            DrawMenuButton(TEXT("HistoryBack"), Selected ? TEXT("返回历史列表") : TEXT("返回主页面"),
                PanelY + PanelH - 56.f, true);
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
            DrawRect(FLinearColor(0.15f, 0.30f, 0.30f), ModeX, PanelY + 510.f, ModeW, 36.f);
            DrawText(TEXT("画图规则：颜色、笔墨与场景限制 →"), FLinearColor::White,
                ModeX + 10.f, PanelY + 521.f, GEngine->GetSmallFont(), 0.9f);
            AddHitBox(FVector2D(ModeX, PanelY + 510.f), FVector2D(ModeW, 36.f), TEXT("RoomDrawingRules"), true, 10);
            DrawText(TEXT("游戏模式"), FLinearColor::White, ModeX, PanelY + 150.f, GEngine->GetMediumFont());
            const TCHAR* Modes[] = { TEXT("一名地图师，多名探索者"), TEXT("多名地图师，一名探索者"), TEXT("探索者对抗：开启后至少三人") };
            for (int32 ModeIndex = 0; ModeIndex < 3; ++ModeIndex)
            {
                const float ModeY = PanelY + 180.f + ModeIndex * 34.f;
                const bool bSelected = ModeIndex == 0 ? GS->RoomMode == ETreasureRoomMode::OneMapmaker
                    || GS->RoomMode == ETreasureRoomMode::ExplorerRace : ModeIndex == 1
                    ? GS->RoomMode == ETreasureRoomMode::OneExplorer : GS->RoomMode == ETreasureRoomMode::ExplorerRace;
                DrawRect(bSelected ? FLinearColor(0.72f, 0.43f, 0.16f) : FLinearColor(0.08f, 0.12f, 0.13f), ModeX, ModeY, ModeW, 30.f);
                const FString Label = ModeIndex == 2 ? FString::Printf(TEXT("%s %s"), bSelected ? TEXT("✓") : TEXT("○"), Modes[ModeIndex])
                    : FString(Modes[ModeIndex]) + (bSelected ? TEXT("（已选择）") : TEXT(""));
                DrawText(Label, FLinearColor::White, ModeX + 10.f, ModeY + 8.f, GEngine->GetSmallFont(), 0.9f);
                if (bHost)
                    AddHitBox(FVector2D(ModeX, ModeY), FVector2D(ModeW, 30.f), ModeIndex == 0 ? TEXT("RoomModeCoop")
                        : ModeIndex == 1 ? TEXT("RoomModeOneExplorer") : TEXT("RoomModeRaceToggle"), true, 10);
            }
            DrawDifficultySettings(ModeX, PanelY + 290.f, ModeW, bHost);
            float PlayerY = PanelY + 560.f;
            DrawText(bHost ? TEXT("面积0.5至5倍，Enter确认；重玩沿用") : TEXT("房主调整设置；开局生效"), FLinearColor(0.75f, 0.84f, 0.82f), ModeX, PanelY + 548.f, GEngine->GetSmallFont(), 0.85f);
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
                        FLinearColor::White, ModeX, PlayerY, GEngine->GetSmallFont(), 0.9f);
                    if (bOwnRow && Member->PlayerRole != SingleRole)
                    {
                        const float ClaimX = ModeX + ModeW - 122.f;
                        DrawRect(HitBoxesOver.Contains(TEXT("RoomClaimSingleRole")) ? FLinearColor(0.23f, 0.49f, 0.40f)
                            : FLinearColor(0.13f, 0.34f, 0.29f), ClaimX, PlayerY - 3.f, 122.f, 20.f);
                        DrawText(SingleRole == ETreasurePlayerRole::Scout ? TEXT("换成地图师") : TEXT("换成探索者"),
                            FLinearColor::White, ClaimX + 6.f, PlayerY + 2.f, GEngine->GetSmallFont(), 0.75f);
                        AddHitBox(FVector2D(ClaimX, PlayerY - 3.f), FVector2D(122.f, 20.f), TEXT("RoomClaimSingleRole"), true, 10);
                    }
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
                const int32 ExpectedScouts = GS->RoomMode == ETreasureRoomMode::OneExplorer ? GS->PlayerArray.Num() - 1 : 1;
                const bool bCanStart = GS->PlayerArray.Num() >= (GS->RoomMode == ETreasureRoomMode::ExplorerRace ? 3 : 2)
                    && Scouts == ExpectedScouts
                    && Hunters == GS->PlayerArray.Num() - ExpectedScouts;
                if (bCanStart) DrawMenuButton(TEXT("RoomStart"), TEXT("开始游戏"), PanelY + 405.f, true);
                else
                {
                    DrawRect(FLinearColor(0.08f, 0.12f, 0.13f), PanelX + 48.f, PanelY + 405.f, PanelW - 96.f, 54.f);
                    DrawText(TEXT("开始游戏"), FLinearColor(0.45f, 0.5f, 0.5f), PanelX + 70.f, PanelY + 421.f, GEngine->GetMediumFont());
                }
                DrawText(bCanStart ? TEXT("地图师和探索者均已就位，可以开始")
                    : GS->RoomMode == ETreasureRoomMode::ExplorerRace && GS->PlayerArray.Num() < 3
                    ? TEXT("探索者对抗需要至少三名玩家") : GS->PlayerArray.Num() < 2
                    ? TEXT("等待另一名玩家加入……") : TEXT("至少需要一名地图师和一名探索者"),
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
        else if (Page == EFrontEndPage::RoomDrawingRules)
        {
            const bool bHost = GetNetMode() == NM_ListenServer;
            DrawText(TEXT("房间画图规则"), FLinearColor::White, PanelX + 48.f, PanelY + 150.f,
                GEngine->GetLargeFont(), 1.25f);
            DrawText(bHost ? TEXT("点击设置，开局后锁定；重玩保留") : TEXT("由房主调整，开局后全员生效"),
                FLinearColor(0.75f, 0.84f, 0.82f), PanelX + 48.f, PanelY + 200.f, GEngine->GetSmallFont());
            auto DrawRule = [&](FName Name, const FString& Label, float Y)
            {
                const FVector2D Min(PanelX + 48.f, Y), Size(PanelW - 96.f, 42.f);
                DrawRect(bHost ? FLinearColor(0.13f, 0.32f, 0.29f) : FLinearColor(0.08f, 0.13f, 0.14f),
                    Min.X, Min.Y, Size.X, Size.Y);
                DrawText(Label, FLinearColor::White, Min.X + 12.f, Min.Y + 12.f, GEngine->GetSmallFont());
                if (bHost) AddHitBox(Min, Size, Name, true, 10);
            };
            DrawRule(TEXT("ToggleSketchSceneLock"), GS->bSketchSceneLock
                ? TEXT("侦察后进入画纸：开启") : TEXT("侦察后进入画纸：关闭"), PanelY + 230.f);
            DrawText(TEXT("开启后，地图师首次打开画纸便无法再看岛屿；交图照常。"),
                FLinearColor(0.72f, 0.8f, 0.78f), PanelX + 48.f, PanelY + 275.f, GEngine->GetSmallFont(), 0.86f);
            DrawRule(TEXT("TogglePreprintedIsland"), GS->bPreprintedIsland
                ? TEXT("预印岛屿轮廓：开启") : TEXT("预印岛屿轮廓：关闭"), PanelY + 300.f);
            DrawRule(TEXT("ToggleLimitedInk"), GS->bLimitedInk
                ? TEXT("限制笔墨：开启") : TEXT("限制笔墨：关闭"), PanelY + 352.f);
            const float InkY = PanelY + 404.f;
            DrawRect(FLinearColor(0.08f, 0.13f, 0.14f), PanelX + 48.f, InkY, PanelW - 96.f, 42.f);
            DrawText(FString::Printf(TEXT("笔墨上限：%d 点"), GS->InkLimit), FLinearColor::White,
                PanelX + 60.f, InkY + 12.f, GEngine->GetSmallFont());
            for (int32 I = 0; I < 2; ++I)
            {
                const float X = PanelX + PanelW - 148.f + I * 50.f;
                const bool bEnabled = bHost && (I == 0 ? GS->InkLimit > GS->MinInkLimit : GS->InkLimit < GS->MaxInkLimit);
                DrawRect(bEnabled ? FLinearColor(0.15f, 0.37f, 0.34f) : FLinearColor(0.10f, 0.14f, 0.15f),
                    X, InkY + 4.f, 42.f, 34.f);
                DrawText(I == 0 ? TEXT("−") : TEXT("+"), FLinearColor::White,
                    X + 12.f, InkY + 6.f, GEngine->GetMediumFont());
                if (bEnabled) AddHitBox(FVector2D(X, InkY + 4.f), FVector2D(42.f, 34.f),
                    I == 0 ? TEXT("RoomInkLess") : TEXT("RoomInkMore"), true, 10);
            }
            DrawRule(TEXT("ToggleSurfacePaint"), GS->bSurfacePaintEnabled
                ? TEXT("实验喷漆：开启") : TEXT("实验喷漆：关闭"), PanelY + 456.f);
            DrawMenuButton(TEXT("RoomDrawingRulesBack"), TEXT("返回房间"), PanelY + PanelH - 64.f);
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

    if (PC->IsPauseMenuOpen() && !GS->IsRoundOver())
    {
        const float CenterX = Canvas->SizeX * 0.5f;
        const float CenterY = Canvas->SizeY * 0.5f;
        const float Width = 440.f;
        const float Left = CenterX - Width * 0.5f;
        DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.82f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
        DrawRect(FLinearColor(0.07f, 0.11f, 0.14f), Left, CenterY - 175.f, Width, 350.f);
        DrawText(TEXT("游戏菜单"), FLinearColor::White, Left + 32.f, CenterY - 145.f, GEngine->GetLargeFont(), 1.25f);
        auto DrawPauseButton = [&](FName Name, const FString& Label, float Y)
        {
            const FVector2D Min(Left + 30.f, Y);
            DrawRect(HitBoxesOver.Contains(Name) ? FLinearColor(0.22f, 0.50f, 0.44f)
                : FLinearColor(0.12f, 0.31f, 0.28f), Min.X, Min.Y, Width - 60.f, 48.f);
            DrawText(Label, FLinearColor::White, Min.X + 16.f, Min.Y + 13.f, GEngine->GetMediumFont());
            AddHitBox(Min, FVector2D(Width - 60.f, 48.f), Name, true, 10);
        };
        DrawPauseButton(TEXT("PauseResume"), TEXT("继续游戏（Esc）"), CenterY - 80.f);
        const bool bCanOpenMap = GS->Phase == ETreasureRoundPhase::HunterSearching
            || (PS->PlayerRole == ETreasurePlayerRole::Scout && !PS->bSketchSubmitted);
        if (bCanOpenMap) DrawPauseButton(TEXT("PauseOpenMap"), TEXT("关闭菜单并查看地图"), CenterY - 20.f);
        else DrawText(TEXT("地图暂不可打开；等待交图时画纸直接显示"), FLinearColor(0.7f, 0.8f, 0.8f),
            Left + 32.f, CenterY - 2.f, GEngine->GetSmallFont());
        if (GetNetMode() == NM_ListenServer)
            DrawPauseButton(TEXT("PauseReturnToLobby"), TEXT("结束本局，全队返回大厅"), CenterY + 40.f);
        else DrawText(TEXT("只有房主可以结束本局并带全队返回大厅"), FLinearColor(0.7f, 0.8f, 0.8f),
            Left + 32.f, CenterY + 58.f, GEngine->GetSmallFont());
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
        DrawRect(FLinearColor(0.07f, 0.11f, 0.14f, 1.f), CenterX - PanelWidth * 0.5f,
            CenterY - PanelHeight * 0.5f, PanelWidth, PanelHeight);

        const FString Title = bRace ? (bWon ? FString::Printf(TEXT("本局胜者：%s"), *GS->RaceRoundWinner)
            : TEXT("本局无人找到宝藏")) : bWon ? TEXT("合作成功！") : TEXT("时间到！");
        float TextWidth = 0.f, TextHeight = 0.f;
        const float TitleScale = bRace ? 1.05f : 1.35f;
        GetTextSize(Title, TextWidth, TextHeight, GEngine->GetLargeFont(), TitleScale);
        DrawText(Title, bWon ? FLinearColor(0.95f, 0.85f, 0.25f) : FLinearColor(1.f, 0.35f, 0.25f),
            CenterX - TextWidth * 0.5f,
            CenterY - 185.f, GEngine->GetLargeFont(), TitleScale);

        const FString Hint = bRace ? FString::Printf(TEXT("第 %d / %d 局  |  找到越快分越高；挖错按接近程度计分"),
            GS->RaceRoundIndex, GS->RaceTotalRounds) : bWon ? TEXT("找到宝藏了！再来一座新岛屿？")
            : GS->Phase == ETreasureRoundPhase::ScoutTimedOut ? TEXT("侦察者未能及时交图，再试一次？")
            : TEXT("寻宝者未能及时找到宝藏，再试一次？");
        GetTextSize(Hint, TextWidth, TextHeight, GEngine->GetMediumFont(), 1.f);
        DrawText(Hint, FLinearColor::White, CenterX - TextWidth * 0.5f,
            CenterY - 117.f, GEngine->GetMediumFont(), 1.f);

        const bool bSolo = GS->PlayerArray.Num() == 1;
        FString ChoiceHint = bSolo ? TEXT("单人测试：再玩一次会开始新的岛屿")
            : TEXT("任一人选择后，全队立即开始新的一局");
        if (bRace)
        {
            int32 BestPoints = TNumericLimits<int32>::Lowest();
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
        GetTextSize(ChoiceHint, TextWidth, TextHeight, GEngine->GetSmallFont(), 1.f);
        DrawText(ChoiceHint, FLinearColor(0.7f, 0.8f, 0.9f), CenterX - TextWidth * 0.5f,
            CenterY - 75.f, GEngine->GetSmallFont(), 1.f);

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
                DrawText(FString::Printf(TEXT("%d. %s  %d 分  本局 %+d  找到 %d 次"), Rank + 1,
                    *Member->GetPlayerName(), Member->RacePoints, Member->RaceLastRoundPoints, Member->RaceFinds),
                    Member == PS ? FLinearColor(0.95f, 0.85f, 0.35f) : FLinearColor::White,
                    CenterX - PanelWidth * 0.5f + 80.f, CenterY - 47.f + Rank * 24.f,
                    GEngine->GetSmallFont(), 0.95f);
            }
            DrawText(TEXT("计分：先找到基础 8–16；地图师 3–7；接近 0–4；远挖最多扣 2"),
                FLinearColor(0.78f, 0.84f, 0.79f), CenterX - PanelWidth * 0.5f + 46.f,
                CenterY + 45.f, GEngine->GetSmallFont(), 0.82f);
        }

        const bool bSameRolesHovered = HitBoxesOver.Contains(SameRolesButtonName);
        DrawRect(bSameRolesHovered ? FLinearColor(0.24f, 0.69f, 0.52f) : FLinearColor(0.16f, 0.52f, 0.40f),
            SameRolesButtonMin.X, SameRolesButtonMin.Y, ButtonWidth, ButtonHeight);
        const FString SameRolesText = bRace ? (bRaceFinished ? TEXT("重新开始竞赛") : TEXT("下一局：轮换地图师"))
            : TEXT("保持角色，再玩一次");
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

        if (!bSolo && !bRace)
        {
            const bool bSwapRolesHovered = HitBoxesOver.Contains(SwapRolesButtonName);
            DrawRect(bSwapRolesHovered ? FLinearColor(0.30f, 0.57f, 0.82f) : FLinearColor(0.20f, 0.42f, 0.67f),
                SwapRolesButtonMin.X, SwapRolesButtonMin.Y, ButtonWidth, ButtonHeight);
            const FString SwapRolesText = PS->PlayerRole == ETreasurePlayerRole::Scout
                ? TEXT("我来探索，开始下一局") : TEXT("我来画图，开始下一局");
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
        if (bWon && GS->ResultServerTime > 0.f)
        {
            const float Age = GS->GetServerWorldTimeSeconds() - GS->ResultServerTime;
            if (Age >= 0.f && Age < 4.f)
            {
                const float Fade = FMath::Clamp(4.f - Age, 0.f, 1.f);
                const FLinearColor Gold(1.f, 0.77f, 0.12f, 0.38f * Fade);
                DrawRect(Gold, 0.f, 0.f, Canvas->SizeX, 130.f);
                DrawRect(Gold, 0.f, Canvas->SizeY - 20.f, Canvas->SizeX, 20.f);
                const FString Victory = bRace && PS->GetPlayerName() == GS->RaceRoundWinner
                    ? TEXT("冠军！你率先找到宝藏！")
                    : bRace ? FString::Printf(TEXT("%s 抢先找到宝藏！"), *GS->RaceRoundWinner)
                    : TEXT("宝藏找到了！");
                GetTextSize(Victory, TextWidth, TextHeight, GEngine->GetLargeFont(), 1.75f);
                DrawText(Victory, FLinearColor(1.f, 0.93f, 0.44f), CenterX - TextWidth * 0.5f,
                    48.f, GEngine->GetLargeFont(), 1.75f);
                for (int32 I = 0; I < 36; ++I)
                {
                    const float X = FMath::Fmod(I * 127.f + Age * (I % 2 ? 44.f : -36.f) + Canvas->SizeX,
                        Canvas->SizeX);
                    const float Y = FMath::Fmod(I * 71.f + Age * (70.f + I % 5 * 15.f), Canvas->SizeY);
                    DrawRect(I % 3 ? FLinearColor(1.f, 0.78f, 0.18f, Fade)
                        : FLinearColor(0.35f, 0.94f, 0.65f, Fade), X, Y, 8.f, 14.f);
                }
            }
        }
        return;
    }

    if (!PC->IsMapOpen() && !PC->IsHunterWaiting())
        for (APlayerState* State : GS->PlayerArray)
            if (const ATreasureSketchPlayerState* Other = Cast<ATreasureSketchPlayerState>(State); Other && Other != PS)
                if (const ATreasureSketchCharacter* Character = Cast<ATreasureSketchCharacter>(Other->GetPawn());
                    Character && !Character->IsHidden())
                {
                    FVector2D Screen;
                    if (PC->ProjectWorldLocationToScreen(Character->GetActorLocation() + FVector(0.f, 0.f, 135.f), Screen))
                    {
                        const FString Name = Other->GetPlayerName();
                        float NameW = 0.f, NameH = 0.f;
                        GetTextSize(Name, NameW, NameH, GEngine->GetSmallFont(), 1.f);
                        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), Screen.X - NameW * 0.5f - 8.f,
                            Screen.Y - 3.f, NameW + 16.f, NameH + 6.f);
                        DrawText(Name, FLinearColor::White, Screen.X - NameW * 0.5f,
                            Screen.Y, GEngine->GetSmallFont());
                    }
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
    const FString Help = GS->bReviewingRound ? TEXT("复盘中：WASD 逛岛 | M 看图及操作按钮 | T 切换宝箱标记 | Esc 返回结算")
        : bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing && PC->IsDrawingOverheadView()
        ? (PC->HasSubmittedSketch() ? TEXT("俯视等待：WASD 飞行 | Space 上升 | Ctrl 下降 | Tab 返回地面")
            : TEXT("俯视侦察：WASD 飞行 | Space 上升 | Ctrl 下降 | Shift 加速 | Tab 返回地面 | M 画图"))
        : PC->IsScoutSpectating()
        ? TEXT("观战：WASD 飞行 | Space 上升 | Ctrl 下降 | Shift 加速 | M 查看地图")
        : bScout && PC->HasSubmittedSketch() ? TEXT("已交图，等待其他地图师完成；到时自动收齐")
        : bScout ? TEXT("WASD 移动 | Tab 俯视侦察 | M 打开白纸画图 | C 清空 | Enter 交图")
        : GS->Phase == ETreasureRoundPhase::HunterSearching && GS->RoomMode == ETreasureRoomMode::ExplorerRace
            ? TEXT("探索者竞赛：WASD 移动 | M 看图 | E 挖掘 | G 推开对手")
        : GS->Phase == ETreasureRoundPhase::HunterSearching ? TEXT("寻宝中：WASD 移动 | M 查看地图 | E 挖掘")
        : TEXT("等待交图；收到后 M 查看地图 | E 挖掘");
    DrawText(FString::Printf(TEXT("%s  |  岛屿种子 %d"), *RoleLabel, GS->IslandSeed), FLinearColor::White, 35.f, 28.f, GEngine->GetLargeFont(), 1.f);
    DrawText(Help, FLinearColor(0.9f,0.9f,0.9f), 35.f, 62.f, GEngine->GetSmallFont(), 1.f);
    if (PC->IsScoutSpectating() && GS->Phase == ETreasureRoundPhase::HunterSearching)
    {
        DrawText(TEXT("鼠标转向 | Tab 切换视角 | Q 切换探索者 | T 宝藏"),
            FLinearColor(0.9f, 0.9f, 0.9f), 35.f, 86.f, GEngine->GetSmallFont(), 1.f);
        DrawText(FString::Printf(TEXT("%s  |  宝藏标记：%s"),
            PC->IsHunterFirstPersonView() ? TEXT("寻宝者第一视角") : TEXT("自由飞行"),
            PC->IsSpectatorTreasureVisible() ? TEXT("显示") : TEXT("隐藏")),
            FLinearColor(0.45f, 0.9f, 0.85f), 35.f, 110.f, GEngine->GetSmallFont(), 1.f);
    }
    if (!bScout && GS->Phase == ETreasureRoundPhase::HunterSearching)
    {
        const int32 CooldownRemaining = FMath::CeilToInt(PS->GetDigCooldownRemaining(
            GS->RoundSerial, GS->GetServerWorldTimeSeconds()));
        DrawText(CooldownRemaining > 0 ? FString::Printf(TEXT("挖掘冷却：%d 秒"), CooldownRemaining)
            : GS->DigCooldownSeconds == 0 ? TEXT("挖掘就绪（无冷却）：按 E") : TEXT("挖掘就绪：按 E"),
            CooldownRemaining > 0 ? FLinearColor(1.f, 0.72f, 0.35f) : FLinearColor(0.4f, 0.95f, 0.65f),
            35.f, 86.f, GEngine->GetSmallFont(), 1.f);
        if (GS->RoomMode == ETreasureRoomMode::ExplorerRace)
        {
            const int32 ShoveCooldown = FMath::CeilToInt(FMath::Max(0.f,
                PS->NextShoveServerTime - GS->GetServerWorldTimeSeconds()));
            DrawText(ShoveCooldown > 0 ? FString::Printf(TEXT("推人冷却：%d 秒"), ShoveCooldown)
                : TEXT("推人就绪：靠近并面向对手按 G"), FLinearColor(0.95f, 0.75f, 0.40f),
                35.f, 109.f, GEngine->GetSmallFont(), 1.f);
            const int32 ProtectionRemaining = FMath::CeilToInt(FMath::Max(0.f,
                PS->ShoveProtectedUntilServerTime - GS->GetServerWorldTimeSeconds()));
            if (ProtectionRemaining > 0)
                DrawText(FString::Printf(TEXT("防连续推：%d 秒"), ProtectionRemaining),
                    FLinearColor(0.55f, 0.9f, 1.f), 35.f, 132.f, GEngine->GetSmallFont(), 1.f);
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
            FLinearColor(0.1f, 0.6f, 1.f), 35.f, 86.f,
            GEngine->GetSmallFont(), 0.9f);
    }
    if (bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing && !PC->HasSubmittedSketch()
        && !PC->IsMapOpen())
        DrawText(PC->HasTakenPhoto() ? TEXT("照片已拍好 · 按 M 在画纸上查看")
            : PC->IsDrawingOverheadView() ? TEXT("相机：返回地面后按 B 拍照")
            : TEXT("相机：按 B 拍一张照片 · 鬼魂视角不可拍"),
            FLinearColor(0.95f, 0.84f, 0.52f), 35.f, 155.f, GEngine->GetSmallFont(), 0.9f);
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

    if (PC->GetDigFeedbackRemaining() > 0.f && !GS->IsRoundOver())
    {
        const int32 Band = PC->GetDigFeedbackBand();
        const TCHAR* Labels[] = { TEXT("就在附近！"), TEXT("很近了！"), TEXT("有些接近"), TEXT("还有一段距离"), TEXT("离得很远！") };
        const FLinearColor Colors[] = { FLinearColor(0.10f, 0.85f, 0.28f), FLinearColor(0.50f, 0.86f, 0.18f),
            FLinearColor(0.96f, 0.84f, 0.16f), FLinearColor(1.f, 0.48f, 0.10f), FLinearColor(0.87f, 0.08f, 0.08f) };
        if (Band >= 0 && Band < 5)
        {
            const float Fade = FMath::Clamp(PC->GetDigFeedbackRemaining(), 0.f, 1.f);
            const FLinearColor Glow(Colors[Band].R, Colors[Band].G, Colors[Band].B, 0.23f * Fade);
            DrawRect(Glow, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
            DrawRect(Colors[Band], 0.f, 0.f, Canvas->SizeX, 16.f);
            DrawRect(Colors[Band], 0.f, Canvas->SizeY - 16.f, Canvas->SizeX, 16.f);
            DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f * Fade), Canvas->SizeX * 0.24f,
                Canvas->SizeY * 0.38f, Canvas->SizeX * 0.52f, 136.f);
            float WordW = 0.f, WordH = 0.f;
            GetTextSize(Labels[Band], WordW, WordH, GEngine->GetLargeFont(), 1.8f);
            DrawText(Labels[Band], Colors[Band], (Canvas->SizeX - WordW) * 0.5f,
                Canvas->SizeY * 0.42f, GEngine->GetLargeFont(), 1.8f);
        }
    }

    const bool bWatchingLiveSketch = PC->IsHunterWaiting();
    auto DrawReviewHint = [&]()
    {
        if (!GS->bReviewingRound) return;
        const FVector2D ButtonMin(Canvas->SizeX - 244.f, 22.f);
        DrawRect(FLinearColor(0.48f, 0.29f, 0.13f), ButtonMin.X, ButtonMin.Y, 220.f, 46.f);
        DrawText(TEXT("按 Esc 返回结算"), FLinearColor::White, ButtonMin.X + 12.f, ButtonMin.Y + 12.f, GEngine->GetSmallFont(), 1.f);
        const FVector2D ToggleMin(ButtonMin.X, ButtonMin.Y + 54.f);
        if (PC->IsMapOpen())
        {
            DrawRect(HitBoxesOver.Contains(TEXT("ToggleReviewTreasure")) ? FLinearColor(0.22f, 0.54f, 0.47f)
                : FLinearColor(0.13f, 0.35f, 0.31f), ToggleMin.X, ToggleMin.Y, 220.f, 42.f);
            DrawText(PC->IsSpectatorTreasureVisible() ? TEXT("隐藏宝箱标记") : TEXT("显示宝箱标记"),
                FLinearColor::White, ToggleMin.X + 12.f, ToggleMin.Y + 11.f, GEngine->GetSmallFont(), 1.f);
            AddHitBox(ToggleMin, FVector2D(220.f, 42.f), TEXT("ToggleReviewTreasure"), true, 10);
        }
        else DrawText(TEXT("按 T 显示／隐藏宝箱"), FLinearColor::White,
            ToggleMin.X + 12.f, ToggleMin.Y + 11.f, GEngine->GetSmallFont(), 0.9f);
    };
    if (!PC->IsMapOpen() && !bWatchingLiveSketch) { DrawReviewHint(); return; }
    if (bWatchingLiveSketch)
        DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.96f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
    const FVector2D Min = PC->GetPaperMin();
    const FVector2D Size = PC->GetPaperSize();
    DrawRect(FLinearColor(0.96f, 0.94f, 0.86f, 0.98f), Min.X, Min.Y, Size.X, Size.Y);
    if (GS->bPreprintedIsland)
    {
        AProceduralIsland* Island = nullptr;
        for (TActorIterator<AProceduralIsland> It(GetWorld()); It; ++It) { Island = *It; break; }
        if (Island)
        {
            constexpr int32 Samples = 48;
            if (CachedTemplateIsland.Get() != Island || CachedTemplateSeed != Island->Seed
                || IslandTemplateMask.Num() != Samples * Samples)
            {
                CachedTemplateIsland = Island;
                CachedTemplateSeed = Island->Seed;
                IslandTemplateMask.SetNumZeroed(Samples * Samples);
                const float Half = (Island->GridSize - 1) * Island->CellSize * 0.5f;
                for (int32 Y = 0; Y < Samples; ++Y)
                    for (int32 X = 0; X < Samples; ++X)
                        IslandTemplateMask[Y * Samples + X] = Island->HeightAt(
                            -Half + (X + 0.5f) * 2.f * Half / Samples,
                            -Half + (Y + 0.5f) * 2.f * Half / Samples) > 0.f;
            }
            const float TemplateSize = FMath::Min(Size.X, Size.Y - 100.f) * 0.9f;
            const FVector2D Origin(Min.X + (Size.X - TemplateSize) * 0.5f,
                Min.Y + 90.f + (Size.Y - 90.f - TemplateSize) * 0.5f);
            const float Cell = TemplateSize / Samples;
            const FLinearColor Coast(0.53f, 0.45f, 0.32f, 0.72f);
            for (int32 Y = 0; Y < Samples; ++Y)
                for (int32 X = 0; X < Samples; ++X)
                    if (IslandTemplateMask[Y * Samples + X])
                    {
                        const float Left = Origin.X + X * Cell, Top = Origin.Y + Y * Cell;
                        if (X == 0 || !IslandTemplateMask[Y * Samples + X - 1])
                            DrawLine(Left, Top, Left, Top + Cell, Coast, 2.f);
                        if (X == Samples - 1 || !IslandTemplateMask[Y * Samples + X + 1])
                            DrawLine(Left + Cell, Top, Left + Cell, Top + Cell, Coast, 2.f);
                        if (Y == 0 || !IslandTemplateMask[(Y - 1) * Samples + X])
                            DrawLine(Left, Top, Left + Cell, Top, Coast, 2.f);
                        if (Y == Samples - 1 || !IslandTemplateMask[(Y + 1) * Samples + X])
                            DrawLine(Left, Top + Cell, Left + Cell, Top + Cell, Coast, 2.f);
                    }
        }
    }
    const FString PaperTitle = GS->bReviewingRound ? TEXT("复盘地图 · 可对照宝藏位置")
        : bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing ? TEXT("空白纸：请画岛屿轮廓、地形地标和藏宝点")
        : bWatchingLiveSketch ? TEXT("地图师的实时画纸 · 只能观看") : TEXT("地图师留下的手绘地图");
    const FString PaintCounter = GS->bSurfacePaintEnabled && bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing
        ? FString::Printf(TEXT("  |  喷漆剩余 %d / %d"),
            FMath::Clamp(ATreasureSurfacePaint::MaxStamps - GS->SurfacePaintStampsUsed, 0, ATreasureSurfacePaint::MaxStamps),
            ATreasureSurfacePaint::MaxStamps) : FString();
    DrawText(PaperTitle + PaintCounter,
        FLinearColor::Black, Min.X + 18.f, Min.Y + 14.f, GEngine->GetSmallFont(), 1.f);
    if (bScout && GS->Phase == ETreasureRoundPhase::ScoutDrawing && !PC->HasSubmittedSketch())
    {
        const FName Names[] = { TEXT("InkBlack"), TEXT("InkRed"), TEXT("InkBlue"),
            TEXT("InkGreen"), TEXT("InkGold"), TEXT("InkEraserSmall"), TEXT("InkEraserLarge") };
        const TCHAR* Labels[] = { TEXT("黑"), TEXT("红"), TEXT("蓝"), TEXT("绿"), TEXT("金"), TEXT("小擦"), TEXT("大擦") };
        for (int32 I = 0; I < 7; ++I)
        {
            const float X = Min.X + 18.f + I * 82.f, Y = Min.Y + 42.f;
            const bool bSelected = PC->GetSelectedInkColor() == (I < 5 ? I : 5)
                && (I < 5 || PC->GetSelectedEraserSize() == I - 5);
            DrawRect(bSelected ? FLinearColor(0.20f, 0.31f, 0.29f) : FLinearColor(0.78f, 0.76f, 0.68f),
                X, Y, 76.f, 38.f);
            DrawRect(SketchInkColor(I < 5 ? I : 5), X + 6.f, Y + 6.f, 20.f, 26.f);
            DrawText(Labels[I], FLinearColor::Black, X + 31.f, Y + 11.f, GEngine->GetSmallFont(), 0.85f);
            AddHitBox(FVector2D(X, Y), FVector2D(76.f, 38.f), Names[I], true, 10);
        }
        if (GS->bLimitedInk)
            DrawText(FString::Printf(TEXT("笔墨 %d / %d"), FMath::Max(0, GS->InkLimit - PC->GetInkUsed()), GS->InkLimit),
                FLinearColor::Black, Min.X + Size.X - 190.f, Min.Y + 52.f, GEngine->GetSmallFont(), 1.f);
        if (GS->bSketchSceneLock)
            DrawText(TEXT("画纸已锁定：交图前不能返回场景"), FLinearColor(0.62f, 0.20f, 0.12f),
                Min.X + 18.f, Min.Y + 85.f, GEngine->GetSmallFont(), 0.85f);
    }
    if (bWatchingLiveSketch && PC->GetSketchPageCount() == 0)
        DrawText(TEXT("正在接收地图师画纸……"), FLinearColor(0.30f, 0.33f, 0.35f),
            Min.X + 18.f, Min.Y + 52.f, GEngine->GetMediumFont(), 1.f);

    for (const FSketchStroke& Stroke : PC->GetStrokes())
    {
        for (int32 I = 1; I < Stroke.Points.Num(); ++I)
        {
            const FVector2D A = Min + Stroke.Points[I-1] * Size;
            const FVector2D B = Min + Stroke.Points[I] * Size;
            DrawLine(A.X, A.Y, B.X, B.Y, SketchInkColor(Stroke.ColorIndex),
                Stroke.ColorIndex == 5 ? (Stroke.EraserSize ? 48.f : 22.f) : 4.f);
        }
    }
    const TArray<uint8>& PhotoJpeg = PC->GetPhotoJpeg();
    if (UTexture2D* PhotoTexture = GetPhotoTexture(PhotoJpeg))
    {
        const FVector2D PhotoButton(Min.X + Size.X - 174.f, Min.Y + 96.f);
        DrawRect(FLinearColor(0.13f, 0.35f, 0.31f), PhotoButton.X, PhotoButton.Y, 154.f, 36.f);
        DrawText(PC->IsPhotoExpanded() ? TEXT("收起照片") : TEXT("查看照片"), FLinearColor::White,
            PhotoButton.X + 23.f, PhotoButton.Y + 9.f, GEngine->GetSmallFont(), 1.f);
        AddHitBox(PhotoButton, FVector2D(154.f, 36.f), TEXT("TogglePhoto"), true, 10);
        if (PC->IsPhotoExpanded())
        {
            const float PhotoW = FMath::Min(Canvas->SizeX * 0.72f, 960.f);
            const float PhotoH = PhotoW * 9.f / 16.f;
            const float PhotoX = (Canvas->SizeX - PhotoW) * 0.5f;
            const float PhotoY = (Canvas->SizeY - PhotoH) * 0.5f;
            DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.88f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
            DrawTexture(PhotoTexture, PhotoX, PhotoY, PhotoW, PhotoH, 0.f, 0.f, 1.f, 1.f);
            DrawText(TEXT("拍摄的线索照片 · 点击右上角收起"), FLinearColor::White,
                PhotoX, PhotoY - 26.f, GEngine->GetSmallFont(), 1.f);
            DrawRect(FLinearColor(0.13f, 0.35f, 0.31f), PhotoX + PhotoW - 110.f, PhotoY - 44.f, 110.f, 36.f);
            DrawText(TEXT("收起照片"), FLinearColor::White, PhotoX + PhotoW - 98.f, PhotoY - 35.f,
                GEngine->GetSmallFont(), 0.85f);
            AddHitBox(FVector2D(PhotoX + PhotoW - 110.f, PhotoY - 44.f), FVector2D(110.f, 36.f),
                TEXT("TogglePhoto"), true, 20);
        }
    }
    if ((!bScout || GS->bReviewingRound || GS->Phase == ETreasureRoundPhase::HunterSearching)
        && PC->GetSketchPageCount() > 0)
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
