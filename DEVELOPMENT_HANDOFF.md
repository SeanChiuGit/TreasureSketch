# TreasureSketch 开发接手说明

## 相机取景与房间开关（`codex/photo-clue`）

- 地图师地面探索时按 B 进入 16:9 取景框，再按 B 拍照；按 Esc 取消取景，不消耗每局一张的额度。鬼魂视角及画纸打开时仍不能拍照。
- 房主可在“房间画图规则”中开启或关闭相机；默认开启。设置开局后锁定，重玩和返回房间时保留。服务端也会拒绝关闭状态下提交的照片。
- 放大照片右上角“收起照片”使用独立命中区域与关闭动作，修复点击无效问题。
- Win64 Development 游戏目标完整编译通过。另一个峡谷项目的 UE 编辑器启用了 Live Coding，编辑器目标完整构建与 `TreasureSketch.RoomSettings` 无窗口测试仍待该编辑器关闭后执行。窗口画面和多人交互由用户验证。

## 地图旋转（`codex/photo-clue`）

- 地图师绘图、探索者等待／寻宝、地图师观战、赛后复盘以及本地历史图纸均可点“左转90°／右转90°”；游戏内画纸也可按 Z／X。每次转 90°，共四档，当前角度显示在画纸上。
- 预印岛屿轮廓与所有笔画一起等比旋转；画纸是横向矩形，竖向两档会缩放内容以保持完整和比例。照片与工具栏不随地图旋转。
- 绘图鼠标坐标反向换算回原始图纸坐标，旋转时结束当前笔画，避免跨档笔画相连。旋转仅影响玩家本地视角，不修改已提交图纸或其他玩家的朝向；新局重置为 0°。
- UE 5.6 TreasureSketchEditor 与 TreasureSketch Win64 Development 编译通过；`TreasureSketch.RoomSettings` 四项无窗口测试通过，新增旋转坐标往返、岛屿轮廓四分之一转、绘图者与等待者旋转检查。日志在独立工作树 `Saved/rotation-room-tests.log`。用户负责窗口交互和多人视觉验证。

## 地图师相机道具（`codex/photo-clue`）

- 从 `codex/three-player-race` 独立分支开发；主仓库的峡谷分支及未提交场景素材未改动。此功能尚未打包 Release。
- 地图师在探索阶段地面视角按 B 拍一张 384×216 JPEG 照片；每人每局一张，不可重拍。照片与本人图纸关联，等待中的探索者可实时收到，交图后在寻宝、观战、复盘和本地游玩历史中保留。
- 打开画纸后点击“查看照片”放大，点击“收起照片”继续画图；查看放大照片时禁止误画。照片不包含本地宝藏标记或自己角色的模型。
- 画纸打开、暂停、交图后、俯视鬼魂状态及观战状态不能拍照；服务端按回合、身份、一次拍摄限额、地面视角状态、相机位置和 JPEG 尺寸再次验证。新局清空照片。
- UE 5.6 TreasureSketchEditor 与 TreasureSketch Win64 Development 完整编译通过；`TreasureSketch.RoomSettings` 四项无窗口测试通过，其中 MultiMapmaker 验证鬼魂拒拍、照片实时归属、交图保留及新局清空。日志在独立工作树 `Saved/camera-room-tests.log`。用户负责实际画面、拍摄手感和多人交互测试。

## 小/大橡皮擦与本地游玩历史（当前工作）

- 画纸顶部提供小橡皮擦（22 px）和大橡皮擦（48 px）；尺寸作为笔画属性通过服务端与实时图纸同步，旧笔画按小橡皮擦读取。
- 完成回合时由服务端向每位玩家发送种子、地图主题、模式、胜负、用时与个人对抗积分；客户端结合已收到的最终图纸，保存到本机 `Saved/SaveGames/TreasureSketchHistory.sav`。历史包含预印岛屿轮廓快照，避免生成算法变化后旧图显示走样。
- 主菜单新增“游玩历史”，按对抗赛分组，详情可切换局数及每位地图师的图纸。未完成的回合不会保存；不同电脑的历史不会自动同步。
- TreasureSketch 与 TreasureSketchEditor Win64 Development 完整构建通过；无窗口 `TreasureSketch.RoomSettings` 四项测试全部成功，包含新增的 HistorySave 测试和大橡皮擦最终交图检查。日志在 `Saved/Logs/Saved/CodexRelease/history-tests.log`。用户负责交互测试；此版本随后已发布，见下方 v0.11.0 记录。

## 最新 Windows 测试 Release：v0.11.0

- 已发布预发布版 `v0.11.0-race-history-rc1`：https://github.com/SeanChiuGit/TreasureSketch/releases/tag/v0.11.0-race-history-rc1
- 标签指向源码提交 `a0e1325`，分支 `codex/three-player-race` 后续补交本文档。
- 附件 `TreasureSketch-v0.11.0-race-history-rc1-a0e1325-Windows.zip`，381224329 字节；SHA-256：`8321e68528ab512e746215ebe478761b0d677ce566001e87a18b31431f0f14b0`。GitHub 附件大小和 digest 与本机 ZIP 一致；Release 为已发布的 prerelease，非 draft。
- UE 5.6 编辑器与 Win64 Development 游戏完整编译、Build/Cook/Stage/Pak/Archive、ZIP 完整性检查成功；`TreasureSketch.RoomSettings` 四项无窗口自动化测试通过。包中检查了 EXE、Steam DLL、PAK、`steam_appid.txt` 和测试说明。用户负责多人交互与画面手感测试。
- 打包仅使用版本库中的 `Config/DefaultEngine.ini`；结束后本机私有配置已逐字节恢复，附带的本机令牌未进入包或提交。测试包继续使用 Steam AppID 480。
- 本机归档和 ZIP 位于 `Builds/TreasureSketch-v0.11.0-race-history-rc1-a0e1325*`，UAT 日志在 `Saved/CodexRelease/package-v0.11.log`。

## 探索者对抗分支（开发中）

- 后续平衡调整：推人加约 0.35 秒抬手预警、推空冷却、命中反馈和被推中者 1.5 秒防连续推，均由服务端判定。挖掘提示已在下方“画纸、对抗计分与反馈扩展”中更新。
- UE 5.6 Win64 Development 游戏目标及编辑器目标完整构建通过；`TreasureSketch.RoomSettings` 下的 ExplorerRace、MultiMapmaker、RoundFlow 三项无窗口测试通过。真实多人动作可读性与手感仍需用户游玩验证。

- 新分支 `codex/three-player-race` 从 `codex/multiplayer-modes` 建立，房间内的“探索者对抗”开关默认关闭；开启后至少 3 人开局，一名地图师、其余探索者竞速。
- 寻宝阶段探索者按 G 推开正前方近距离对手，服务端核对角色、距离、朝向、遮挡和 5 秒冷却。当前计分规则见下方“画纸、对抗计分与反馈扩展”；结算显示本局胜者和累计排名。
- 竞赛按人数轮换地图师，三人进行三局；最终展示总分领先者及并列情况。可重开竞赛或回大厅，合作玩法保持原规则。
- UE 5.6 完整编辑器构建成功；`TreasureSketch.RoomSettings.ExplorerRace` 无窗口测试通过。交互测试仍由用户负责，重点验证实际多人推人手感、战绩同步、三局轮换与复盘。
- `Config/DefaultEngine.ini` 有本机私有配置，未修改，不可加入提交或发布包。

当前开发分支：`codex/multiplayer-modes`。
本机仓库：`D:\My projects\TreasureSketch`，项目文件 `TreasureSketch.uproject`，引擎 UE 5.6。

## 当前功能

- 联机合作房间支持 2–4 人，已实现“一名地图师、多名探索者”和“多名地图师、一名探索者”两种模式；探索者找到宝藏全队成功。2对2 对抗仍为预留入口。
- 单人探索者测试、完整流程测试共用正式玩法，支持指定种子。
- 大厅和两个单人玩法测试支持面积倍数 0.5–5：边长按倍数平方根增长。沙滩与森林的 1 倍生成范围统一为约 125 米边长，实际陆地面积随形状变化。
- 森林替代遗迹进入菜单与随机主题池。朋友分支已合并至 `e462b9a`，遗迹旧素材保留。
- 资源目标数量按面积倍率增长，物体模型大小不变。森林基础数量已按缩小后的基准面积折算，保留原密度。生成间距检测使用空间分桶。
- 绘图、寻宝时间分别可调 30–600 秒，步长 30 秒，默认各 120 秒。
- 移动速度倍率 0.5–5，步长 0.25，默认 1 倍（5.2 米/秒），同步到服务端与客户端。加速与刹车同步缩放，跳跃高度、观战飞行速度不变。
- 联机设置由房主调整，开局锁定，重玩保留；结算后房主可让全队返回大厅调整，单人可返回测试菜单。
- 喷漆是默认关闭的实验功能，地图师按住右键沿视角喷漆，每局最多 15 个色点。交图保留，新一局清除。

## 验证与协作约定

用户已反馈本轮测试完成。最近的森林合并、面积倍率与速度改动由用户编译和游玩测试，助手没有独立验证最新版的多人运行表现。

用户希望自己负责交互测试，助手不应主动打开或控制 UE/游戏窗口来测试。改动完成后保存提交并说明如何测；编译、打包与发布按用户指示进行。新增反射字段或大量合并时优先关闭编辑器完整构建，此前 Live Coding 曾崩溃。

`Config/DefaultEngine.ini` 有 UE 自动生成的本机 Android File Server 配置，包含本机连接令牌，保留原文件，不提交、打印或打包分发该令牌。打包时需备份本机配置、使用版本库中的配置，并在完成后恢复本机文件。

已知待验证项：森林雾效仍可能不显示；部分石头与树木的喷漆/碰撞支持不完整；实际资源数量受地形、坡度、间距和尝试次数约束，不保证严格等比。

## 版本发布

现有 GitHub Windows 测试 Release `v0.7.0-multiplayer-paint-rc1` 来自 `2477a13`，不含随后加入的可调地图/时间、森林、面积倍率和移动速度设置。这次仅提交并推送源码，不更新 Release。

历史自动化测试在 `Source/TreasureSketch/Tests/RoomSettingsTest.cpp`；最近修改过断言以适配面积倍率与统一地图基准，但尚未重新运行最新版测试。

## 本轮改动：等待计时、自动交图、房间显示与出生点设置

- 等待地图画面补上绘图倒计时，到时服务端自动交付图纸并开始寻宝阶段。保留提前 Enter 交图。
- 地图师笔画每 0.1 秒以小批次可靠同步给服务端；清空同步清空缓存，以回合编号隔离旧数据。自动交图使用服务端已收到的图纸，极近截止时间的网络在途笔画可能不包含。
- 新增房主可调、全员同步、开局锁定的 `bTreasureRangeVisible`（默认显示）和 `bSpreadPlayerSpawns`（默认同一区域），重玩与返回大厅保留。
- 用户确认“绘图宝藏提示”指宝藏周围的挖掘判定范围可视化。开关只影响圆柱/圆环显示，保留红色 X，不改实际判定范围；地图测试与观战显示也遵循开关。
- 分散模式探索者与地图师分开登岛；附近模式防止角色重叠。角色轮换后地图师仍从固定登岛区域开始。
- 保留已有结算返回大厅流程，按钮改成“全队返回大厅”；客户端显示由房主操作的说明。
- 扩充 RoomSettings.RoundFlow 自动化测试：设置锁定/保留、绘图超时自动进入寻宝、迟到交图不重置寻宝计时、单人自动交图。
- 本轮仅做源码检查，未编译、未运行自动化测试、未操作游戏窗口。新增 UPROPERTY 和 RPC，应关闭编辑器后完整构建。用户负责交互测试；按既有约定仅提交推送源码，不更新 Release。
- 建议用户验证：2–4 人等待倒计时；主机/客户端分别当地图师画图并等待超时；清空后重画、空白超时、连续重玩无旧图；寻宝阶段独立计时；两项设置客户端一致且重玩保留；轮换角色后出生点两种模式；成功/失败后房主返回大厅与单人返回测试菜单。
## 本轮 Windows Release 已发布

用户随后明确要求 push、打包和发布，已完成：
- Release：`v0.8.0-room-settings-rc1`（预发布测试版）。
- 地址：https://github.com/SeanChiuGit/TreasureSketch/releases/tag/v0.8.0-room-settings-rc1
- 游戏源码提交：`4825626`，来自 `codex/multiplayer-modes`。
- 包：`TreasureSketch-v0.8.0-room-settings-rc1-4825626-Windows.zip`，386996303 字节。
- SHA-256：`7cceaa71a3f2adf34a3aa7c8393083a490796b534583dad292887cac4af1196f`。
- UE 5.6 编辑器与 Win64 Development 游戏编译，以及 Build/Cook/Stage/Pak/Archive 成功。
- `TreasureSketch.RoomSettings.RoundFlow` 通过无窗口自动化测试。引擎测试启动与测试世界清理有诊断信息，未声称日志零警告。
- ZIP 必需文件检查通过（游戏 EXE、Steam DLL、steam_appid、测试说明），上传文件大小和 SHA-256 校验通过。
- 打包期间使用版本库配置，结束后已逐字节恢复本机 DefaultEngine.ini；本机配置未提交。
- 未打开或控制游戏窗口，未进行多人交互验证；游玩测试仍由用户负责。
- 本机包保存在 `Builds/TreasureSketch-v0.8.0-room-settings-rc1-4825626`；构建、自动化测试和发布日志在 `Saved/CodexRelease/`。
## 多地图师、单个探索者模式已实现

- 用户明确选择独立图纸：每位地图师各画一张图，探索者切换查看；全部交图或绘图到时进入寻宝。
- 房主在大厅可选 OneMapmaker / OneExplorer，模式开局锁定。切换时保留唯一角色的玩家身份（原唯一地图师变为唯一探索者，或反过来），加入/离开时维持当前模式的角色数量。
- FSketchPage 保存作者与笔画；服务端按玩家保存已提交图纸，按房间玩家顺序收集。提交状态 bSketchSubmitted 复制给全员，展示交图进度。
- 地图师交图后不能修改或喷漆，等其他人完成；重复提交不会覆盖。绘图超时收齐已提交和未提交图纸，空白图纸也保留。
- 探索者按 M 查看，左右方向键或图纸下方“上一张 / 下一张”按钮翻页。页码和作者显示在纸张外，避免遮挡笔画。
- 所有地图师均可观战，第一视角更新不再只发送给最后一个地图师；原单地图师多探索者模式保持选择探索者观战的行为。
- 重玩可轮换唯一探索者；模式和设置保留。每局清除图纸与提交状态，旧回合交图拒收。
- 多地图师出生位置避免角色重叠，分散模式探索者登岛避开各地图师的初始出生区域。
- UE 5.6 TreasureSketchEditor Win64 Development 完整编译成功（使用 -NoUBTMakefiles 纳入新增测试文件）。无窗口自动化 TreasureSketch.RoomSettings.MultiMapmaker 和 RoundFlow 均通过。
- 新测试覆盖四人角色切换、非法模式与开局锁定、独立图纸/重复提交/空白图纸、全部交图、超时收齐、翻页与旧回合拒收、角色轮换、回大厅、探索者离开后补位。
- 未控制游戏窗口、未进行多人游玩；交互由用户负责。本轮提交推送源码，现有 v0.8.0 Release 不含本功能，未自动更新发布包。
- 建议三或四人测试：各画不同图纸，分别提前提交/等待超时；探索者翻页看作者和内容；地图师全部观战；轮换探索者重玩；唯一探索者离开回大厅并补位；切回原模式再开局。
## 等待时实时观看画纸

- 用户要求探索者等待时可以实时看画图者画纸。本轮在两种合作模式均启用：服务端接收地图师可靠增量笔画后转发给所有等待中的探索者，清空单独转发，提前提交发送作者最终画纸。
- 新局开始时探索者收到有作者 ID、名称及当前笔画的画纸列表；笔画按回合与作者路由，旧回合增量忽略。寻宝开始时再发送最终完整画纸，保留探索者已选页码。
- 等待画面直接显示只读纸张、倒计时和已交图进度；多地图师可用左右方向键或画纸下方按钮实时翻页。首批画纸尚未到达时显示接收提示。
- `TreasureSketch.RoomSettings.MultiMapmaker` 新增对实时增量、各作者隔离、清空、最终替换、等待中翻页、旧回合拒收以及单地图师模式的检查。
- UE 5.6 TreasureSketchEditor Win64 Development 编译通过；无窗口 `TreasureSketch.RoomSettings.MultiMapmaker` 和 `TreasureSketch.RoomSettings.RoundFlow` 测试均通过，日志位于 `Saved/CodexRelease/live-sketch-tests.log`。没有进行游戏窗口或真实多人交互测试。
- 现有 v0.8.0 Release 不包含本轮改动；本轮仅提交推送源码。

## 结算后岛上复盘

- 结算页新增“留在岛上复盘”，任一玩家点击后全队同步进入复盘；结束按钮返回原结算页。保留原胜负结果、新局与回大厅选择。
- 复盘沿用已生成岛屿，不重置种子、地形或图纸。地图师角色重新显示并可走动；所有人都收到最终图纸及宝藏位置，本地显示红色 X，碰撞范围仍遵守房间设置。M 打开只读图纸，多图纸可翻页。
- 复盘不恢复寻宝计时、挖掘或绘图；离开房间、新局和回大厅仍走原清理流程。此功能未加入现有 v0.8.0 Release；交互测试仍由用户负责。
- UE 5.6 TreasureSketchEditor Win64 Development 编译成功；无窗口 `TreasureSketch.RoomSettings.MultiMapmaker` 与 `TreasureSketch.RoomSettings.RoundFlow` 测试通过，日志在 `Saved/CodexRelease/review-tests.log`。用户负责实际窗口与多人交互测试。

## 森林 1 倍尺寸调整

- 用户要求森林一倍大小缩小为 75%。按地图边长理解，森林 1 倍边长从 125.4 米改为 94.05 米；沙滩与遗迹保持 125.4 米。面积倍率仍按平方根缩放边长。
- 森林资源基础目标数量按新面积相对原始 246 米森林的比例同步缩小，保留原密度；实际摆放数量仍受坡度、间距及尝试次数影响。
- RoomSettings.RoundFlow 的随机主题尺寸断言已改为分别校验沙滩/森林的基准边长，另验证森林 1 倍和 2 倍面积设置。
- UE 5.6 TreasureSketchEditor Win64 Development 编译成功；无窗口 `TreasureSketch.RoomSettings.MultiMapmaker` 和 `TreasureSketch.RoomSettings.RoundFlow` 测试均通过，日志位于 `Saved/CodexRelease/forest-75-tests.log`。用户负责游玩观感测试。

## 最新 Windows 测试 Release

- 用户要求打包 Release，已发布预发布版本 `v0.9.0-multimap-review-rc1`：https://github.com/SeanChiuGit/TreasureSketch/releases/tag/v0.9.0-multimap-review-rc1
- 源码提交 `799620e`，包含多地图师模式、等待时实时画纸、结算后岛上复盘和森林 1 倍边长缩小为旧版的 75%。
- 附件 `TreasureSketch-v0.9.0-multimap-review-rc1-799620e-Windows.zip`，387136326 字节；SHA-256：`d5b35fbf29499afbf76c9d029b55a75fd22ce81743723ffb2ae80624a950f725`。
- UE 5.6 编辑器及 Win64 Development 游戏编译、Build/Cook/Stage/Pak/Archive 成功；两个 RoomSettings 无窗口自动化测试通过。ZIP 检查包含游戏 EXE、Steam DLL、steam_appid、PAK 和测试说明，远端附件大小和摘要已核对。
- 打包使用版本库配置，结束后逐字节恢复本机 `Config/DefaultEngine.ini`，该本机文件未提交或包含于分发包。没有进行游戏窗口或多人交互测试，仍由用户负责。
- 本机归档位于 `Builds/TreasureSketch-v0.9.0-multimap-review-rc1-799620e`，日志位于 `Saved/CodexRelease/package-v0.9.log` 和 `Saved/CodexRelease/publish-v0.9.log`。

## 房间地图池

- 房主可在房间大厅启用/排除海盗沙滩和迷雾森林，默认两者都启用；至少保留一种。遗迹仍为未开放主题，不在房间地图池中。
- 选择复制给全员，开局后不可修改；每局按当前地图池随机选主题，重玩和返回大厅保留设置。若大厅预览地图被排除，会立即重建为允许的主题。
- 此功能在 `v0.9.0-multimap-review-rc1` 发布之后开发，尚未打入该 Release；交互测试仍由用户负责。
- UE 5.6 TreasureSketchEditor Win64 Development 编译成功；无窗口 `TreasureSketch.RoomSettings.MultiMapmaker` 和 `TreasureSketch.RoomSettings.RoundFlow` 测试通过，日志位于 `Saved/CodexRelease/map-pool-tests.log`。测试覆盖默认池、单主题筛选、禁止清空、开局锁定、重玩/回大厅保留和预览切换。

## 复盘退出按键

- 用户反馈复盘时无光标，右上角结束按钮无法点击。按用户要求保留无光标的逛岛操作，新增 Esc：任一玩家在复盘中按 Esc，请求全队返回结算页。
- 右上角原按钮改为“按 Esc 返回结算”的纯提示，无鼠标命中区域。现有 v0.9.0 Release 不含此修改；交互验证由用户负责。

## 地图师观战看图

- 寻宝阶段给每位观战地图师发送所有最终图纸；M 打开只读地图，再按 M 关闭。多地图师模式下默认显示自己的图纸，可用方向键或按钮翻页比较。
- 看图时暂停自由飞行/第一人称观战镜头，关图后继续；地图师原本的本地画笔数据保留，避免服务端在复盘收集图纸时丢失主机未提交的同步笔画。
- 此改动未加入现有 v0.9.0 Release；交互测试由用户负责。
- UE 5.6 TreasureSketchEditor Win64 Development 编译成功；无窗口 `TreasureSketch.RoomSettings.MultiMapmaker` 和 `TreasureSketch.RoomSettings.RoundFlow` 测试通过，日志位于 `Saved/CodexRelease/spectator-map-tests.log`。

## 复盘宝箱标记开关

- 复盘默认显示红色 X 宝藏标记及房间允许的判定范围。每位玩家可按 T 本地显示/隐藏；按 M 打开图纸后，右上角另有可点击的“显示/隐藏宝箱标记”按钮。无光标逛岛状态下仅显示 T 操作提示。
- 切换不改变服务端宝藏位置、结果或其他玩家画面；退出复盘时隐藏标记，再次进入复盘恢复默认显示。
- UE 5.6 TreasureSketchEditor Win64 Development 编译成功；无窗口 `TreasureSketch.RoomSettings.MultiMapmaker` 和 `TreasureSketch.RoomSettings.RoundFlow` 测试通过，日志位于 `Saved/CodexRelease/review-treasure-toggle-build.log` 与 `Saved/CodexRelease/review-treasure-toggle-tests.log`。交互测试由用户负责。

## 游戏中 Esc 菜单与大厅身份互换

- 绘图、寻宝进行中按 Esc 打开本地菜单：继续游戏、可用时查看地图；只有房主看得到“结束本局，全队返回大厅”。该操作沿用 `ReturnToSetup`，重建预览岛屿并清理图纸、喷漆与观战状态，全队回房间，保留模式、设置和身份；计时不会因打开菜单而暂停。复盘时 Esc 仍返回结算。
- 两种合作模式仍各有一个唯一身份。大厅中玩家点击自己列表行的按钮可领取这个身份，服务端自动把原持有者换为另一身份，并更新宝藏标记。开局时服务端继续验证至少两人且地图师、探索者人数符合所选模式。游戏中不可换身份。
- 用户负责游戏窗口与多人交互测试；本轮无窗口测试覆盖两种模式的身份互换、开局后拒绝换身份、菜单开关和进行中返回大厅。Esc 按键及鼠标点击需交互测试。
- UE 5.6 TreasureSketchEditor Win64 Development 编译成功；`TreasureSketch.RoomSettings.MultiMapmaker` 和 `TreasureSketch.RoomSettings.RoundFlow` 无窗口测试通过，日志位于 `Saved/CodexRelease/pause-role-build.log` 与 `Saved/CodexRelease/pause-role-tests.log`。尚未打入现有 v0.9.0 Release。

## 绘图俯视与喷漆剩余量

- 实验喷漆仍全队每局最多 15 个色点。服务端只对成功添加的喷点增加 `SurfacePaintStampsUsed`，通过 GameState 同步，HUD 与绘图纸标题显示 `15 - 已用`；重复或超限喷点不扣量。新局及返回大厅重置。
- 地图师在绘图阶段按 Tab 切换地面与自由飞行俯视镜头；即使画纸打开也可按 Tab 切换，M 仍可随时画图。俯视期间锁住角色本体移动与喷漆输入，切回地面保留宝藏标记。进入寻宝阶段后，Tab 仍按原逻辑切换观战自由飞行与探索者第一视角。
- 用户负责实际镜头、鼠标与多人交互测试。此改动尚未包含在 v0.9.0 Release。
- UE 5.6 TreasureSketchEditor Win64 Development 编译成功；`TreasureSketch.RoomSettings.MultiMapmaker` 与 `TreasureSketch.RoomSettings.RoundFlow` 无窗口测试通过，日志位于 `Saved/CodexRelease/spray-overhead-build.log` 和 `Saved/CodexRelease/spray-overhead-tests.log`。测试涵盖俯视与画纸切换、宝藏标记保留、喷点接受/重复扣量和新局重置。

## 挖掘冷却

- 房主在大厅可设置挖掘冷却，默认 10 秒，范围 0–60 秒、每次加减 5 秒；0 秒为无冷却。设置全员同步，开局锁定，重玩和返回大厅保留。
- 冷却按探索者独立计算；服务端仅在寻宝阶段的有效尝试后启动，冷却中重复请求不再次判定也不延长倒计时。PlayerState 同步下一次可挖的服务端时间和回合编号，HUD 显示就绪或剩余秒数；新回合编号使旧冷却失效。服务端使用角色实际位置判定挖掘。
- 用户负责窗口与联机交互测试。此改动未进入现有 v0.9.0 Release。
- UE 5.6 TreasureSketchEditor Win64 Development 编译成功；`TreasureSketch.RoomSettings.MultiMapmaker` 与 `TreasureSketch.RoomSettings.RoundFlow` 无窗口测试通过，日志位于 `Saved/CodexRelease/dig-cooldown-build.log` 和 `Saved/CodexRelease/dig-cooldown-tests.log`。测试覆盖设置锁定与保留、独立冷却、重复挖掘、0 秒冷却及新回合清除旧冷却。

## 最新 Windows 测试 Release：v0.10.0

- 已发布预发布版 `v0.10.0-lobby-overhead-cooldown-rc1`：https://github.com/SeanChiuGit/TreasureSketch/releases/tag/v0.10.0-lobby-overhead-cooldown-rc1
- 源码提交 `23f11cf68ea17594abfb0f1eb0d8609c445e0f61`，包含 v0.9.0 后的房间地图池、复盘控制、观战看图、游戏中 Esc 菜单与全队回大厅、大厅身份互换、绘图俯视、喷漆剩余量及挖掘冷却。
- 附件 `TreasureSketch-v0.10.0-lobby-overhead-cooldown-rc1-23f11cf-Windows.zip`，387168358 字节；SHA-256：`9506f3c00efa1de2e33b2a7ed672944bc16bb637b94932934c99f9445c25688c`。远端附件大小与 digest 已核对。
- UE 5.6 编辑器和 Win64 Development 游戏编译、Build/Cook/Stage/Pak/Archive 成功；两个 RoomSettings 无窗口自动化测试通过。ZIP 检查包含 EXE、Steam DLL、steam_appid、PAK 和包内说明。
- 打包使用版本库配置，结束后本机 `Config/DefaultEngine.ini` 哈希与打包前一致，未提交也未分发本机令牌。未打开游戏窗口，真实多人交互测试由用户负责。
- 本机归档和 ZIP 位于 `Builds/TreasureSketch-v0.10.0-lobby-overhead-cooldown-rc1-23f11cf*`；构建与发布日志在 `Saved/CodexRelease/package-v0.10.log` 和 `Saved/CodexRelease/publish-v0.10.log`。

## 掉出地图与鬼魂视角修复

- 游戏及复盘期间，服务端每帧检查可见玩家角色；低于岛屿原点 600 单位时清除下落速度并传送到岛屿安全出生点。观战地图师的隐藏角色不受影响。
- 玩家脚部低于水面时跳跃初速从 620 提高到 900，回到正常高度即恢复 620；判定放在 `CheckJumpInput`，让联机客户端预测和服务端移动重放使用相同规则。无需记录安全位置。
- 寻宝阶段鬼魂自由飞行的鼠标视角灵敏度从 0.15 提高到 0.45；绘图阶段俯视保持原灵敏度。
- 游戏窗口与多人手感测试由用户负责。
- UE 5.6 TreasureSketchEditor Win64 Development 完整编译成功；`TreasureSketch.RoomSettings` 下的 ExplorerRace、MultiMapmaker、RoundFlow 三项无窗口自动化测试通过，日志在 `Saved/CodexRelease/fall-ghost-tests.log`。掉落与跳跃判定已加入 ExplorerRace 测试。

## 画纸、对抗计分与反馈扩展

- 画纸有黑、红、蓝、绿、金五色笔和宽橡皮擦；每笔颜色进入图纸结构，实时同步及最终图纸一致。橡皮擦用纸色覆盖旧笔画；C 清空会恢复笔墨。
- 大厅“画图规则”子页可选：首次打开画纸后锁定场景、预印当前岛屿的海岸轮廓、限制笔墨。三项默认关闭；笔墨上限默认 600 点、100–2000 点、每次调整 100 点。服务端验证笔画颜色、坐标和笔墨上限。设置开局锁定，重玩保留。
- 对抗赛结算时按剩余寻宝时间计分：先找到者 8–16 分，地图师 3–7 分；其他探索者按本局最接近的一次有效挖掘得 0–4 分，50 米档以外的无效距离尝试每次扣 1 分、最多扣 2 分。胜者自己的远挖也扣分。超时只计算探索者的接近与远挖分。本局增减分和累计总分在结算显示。
- 挖掘距离反馈改为五档整屏红、橙、黄、浅绿、绿视觉提示，不显示精确米数。1 倍地图阈值为 6／10／20／50 米，随地图面积倍率的平方根调整。成功时结算顶部显示胜者姓名、个人冠军文案和短暂彩纸动画。
- 服务端给探索者安排出生点时避开宝藏 30 米范围；游戏中其他可见角色头顶显示姓名。合作结算可由选择“我来画图／我来探索”的玩家直接领取下一局角色；复盘默认隐藏宝藏标记，仍可按 T 或图纸按钮显示。
- 用户负责游戏窗口、多人同步及手感测试。UE 5.6 TreasureSketchEditor 与 TreasureSketch Win64 Development 完整编译通过；ExplorerRace、MultiMapmaker、RoundFlow 三项无窗口测试通过，日志 `Saved/CodexRelease/drawing-race-update-tests.log`。本轮未打包 Release。
