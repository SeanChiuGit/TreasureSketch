# TreasureSketch 开发接手说明

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
