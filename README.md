# 生命游戏 (life-game) —— Qt 6 / C++17 / 标准分层 MVC

康威生命游戏（Conway's Game of Life）的桌面实现。项目使用 **C++17 + Qt 6 Widgets**，
按**标准分层 MVC（被动视图）**组织：

- **Model** 只包含状态、规则和用例，不引用 View/Controller；
- **View** 是纯被动视图：不包含 Model 头文件、不连 Model 信号，只接收 Controller
  推送的只读数据和 `const domain::Grid*`，只发出用户意图；
- **Controller** 是唯一同时持有 Model 与 View 的协调层，负责“查询 Model → 更新 View”
  和“View 意图 → Model 命令”的双向转换。

它同时是一个**可执行的架构范例**：依赖方向由分层工程结构与
`tools/check_layering.py` 共同约束。Domain/Shared 层不依赖 Qt；Model/Service/View
互不依赖，三者都只依赖 Domain + Shared 契约；Controller 依赖三者；App 只做组合。

![界面截图](docs/screenshot.png)

1500 × 1000 大棋盘（150 万格，10 代/秒）渲染实测：

![1500×1000 大棋盘](docs/screenshot-1500x1000.png)

## 为什么值得作为范例

- **严格的 View→Model 隔离**：View 不包含 `model/` 任何头文件，只持有
  `const domain::Grid*` 和 `shared::LifeStats` 等值对象；Model 变化由 Controller
  连接信号后调用 View setter，View 自己不知道 Model 的存在。
- **Controller 是唯一中介**：输入从 View 信号进入 Controller，状态从 Model 信号
  经 Controller 推送回 View；没有 View→Model 的隐式观察链。速度、跳转粒度、密度、
  尺寸粒度和网格开关的规范状态都由 Controller 持有，View 只显示和发出带值意图。
- **Domain 零框架依赖**：`src/domain` 不包含任何 Qt 头文件，可用普通 `g++` 单独编译测试。
- **可执行的架构门禁**：`tools/check_layering.py` 在测试与 CI 中检查层间 include；
  `tools/check_layering.py` 逐条核对。
- **并发与状态机可测试**：单代异步演化封装在 `SimulationEngine`；跳转重放在
  `LifeModel` 的后台任务中完成；快照历史使用 1 bit/cell 压缩；Model/View/Controller 各有 QtTest 覆盖。
- **错误码而非异常**：应用代码以 `-fno-exceptions` 编译；可恢复失败使用
  `model::CommandResult`、`service::FileError`、`service::SettingsError`、
  `domain::RleError` 等类型化错误码，Controller 负责映射成一条可操作的中文提示。
- **可运维日志**：内嵌 stacklog 直接写入 2 MiB × 3 份轮转文件，默认 Info，
  `--verbose` 才打开 Debug，`--log-file` 可覆盖路径；不会逐代/逐格刷日志。
- **单实例**：内嵌 singleproc 用文件锁保证同一用户只运行一个实例；重复启动直接退出。
- **工程化齐全**：qmake 工程、7 个用例（5 个 QTest + 2 个分层检查）、ASan/UBSan 选项、
  `.clang-format`、README 与架构文档。

## 功能

- **直接绘制**：左键点击切换细胞，按住拖动可连续绘制或擦除；快速拖动自动补齐中间格子
- **模拟控制**：开始 / 暂停、单步、随机填充、清空
- **代数跳转**：按指定代数回退或快进；跳转不逐代刷新界面，回退基于快照重放
- **可配置粒度**：跳转粒度、棋盘尺寸粒度、快照间隔，保存在 QSettings
- **速度调节**：0 – 120 代/秒（0 为暂停，正数对数刻度；大棋盘按屏幕刷新率自适应合并，屏幕足够快时逐代刷新）
- **并行演化**：QtConcurrent + QThreadPool 按行分块；计算不占用 UI 线程
- **图案库**：13 种经典图案，选择后显示半透明预览，点击棋盘直接放置
- **规则切换**：康威生命、高地生活、昼与夜、种子、迷宫；支持 RLE 头部自定义 B/S
- **边界模式**：死亡边界与环绕边界
- **棋盘尺寸**：宽度 20–1500，高度 20–1000（最大 150 万格，x 轴为宽度）
- **视图操作**：滚轮缩放（1 – 48 px/格）、中键/右键拖动平移、网格线开关
- **文件**：打开 / 保存 RLE，导出 PNG 棋盘图片
- **状态统计**：代数、存活细胞数、密度（Model 缓存人口，`stats()` O(1)）
- **自动暂停**：手动编辑、清空、载入时自动暂停；检测到静物或空棋盘时停止并提示
- **安全操作**：清空/调整尺寸前中文确认；对话框按钮为中文；Space 不会抢走
  已聚焦按钮的键盘激活
- **会话续接**：退出时保存棋盘、代数、规则、边界、速度、密度、网格与窗口位置

## 操作

| 操作 | 方式 |
|---|---|
| 绘制 / 擦除 | 左键点击；按住拖动连续绘制 |
| 放置图案 | 在侧边「图案」中选择，点击棋盘 |
| 平移 | 中键或右键拖动 |
| 缩放 | 滚轮；或菜单「编辑 / 视图」中的缩放项 |
| 开始 / 暂停 | 空格 / 工具栏按钮 / 侧边主按钮 |
| 单步 | `F6` / 工具栏「单步」 |
| 回退 / 快进 | 侧边「跳转」行填写代数后点「回退 / 快进」；或 `Ctrl+Shift+← / →` |
| 设置 | `Ctrl+,` / 菜单「编辑 → 设置」 |
| 随机填充 | `Ctrl+R` |
| 清空 | `Ctrl+Delete` |
| 载入示例 | `Ctrl+D` |
| 打开 / 保存 RLE | `Ctrl+O` / `Ctrl+S` |
| 导出 PNG | `Ctrl+Shift+S` |
| 重置缩放 | `Ctrl+0` |
| 显示 / 隐藏网格 | `Ctrl+G` |
| 全屏 | `F11` |

### 日志与单实例

- 同一用户同时只能运行一个实例；已有实例时，新进程直接退出（返回 0），
  不会把旧窗口带到前台。
- 日志写入 `QStandardPaths::AppLocalDataLocation/logs/life-game.log`（Linux 下通常是
  `~/.local/share/life-game/life-game/logs/life-game.log`），按 2 MiB × 3 份轮转。
- `--log-file <path>` 覆盖日志路径；`--verbose` 打开 Debug 级别。

## 图案库

| 分类 | 图案 |
|---|---|
| 静物 | 方块、蜂巢 |
| 振荡器 | 闪烁灯、蟾蜍、信标、脉冲星、五连珠 |
| 飞船 | 滑翔机、轻型飞船 |
| 增长 | 高斯帕滑翔机炮 |
| 长寿 | 橡果、R 五连体、死亡之翼 |

图案形状写在 `src/domain/pattern_library.cpp`，只使用稳定 ID + ASCII 艺术；
中文名称、分类与说明位于 `src/view/pattern_catalog.cpp`，两者通过 ID 关联。

## 规则

每个细胞只由周围 8 格中的存活数量决定。B/S 记号中，`B` 后是诞生所需的邻居数，
`S` 后是存活所需的邻居数。

| 名称 | 记号 | 备注 |
|---|---|---|
| 康威生命 | `B3/S23` | 默认规则 |
| 高地生活 | `B36/S23` | 具有自我复制行为的著名变体 |
| 昼与夜 | `B3678/S34678` | 对称性很强 |
| 种子 | `B2/S` | 所有活细胞都会死亡，靠新细胞扩散 |
| 迷宫 | `B3/S12345` | 容易生成迷宫状纹理 |

## 架构

完整契约见 **[docs/architecture.md](docs/architecture.md)**；
大棋盘渲染 profile 见 **[docs/rendering-profile.md](docs/rendering-profile.md)**；
独立审计（不引用文档作为证据）见 **docs/architecture-audit.md**。

### 分层

```
┌──────────────────────────── app ────────────────────────────┐
│ main.cpp · MainWindow（组合根）· 菜单 / 工具栏 / 状态栏       │
└───────────────────────────┬─────────────────────────────────┘
                            ▼
                       controller
                      ╱    │    ╲
                     ▼     ▼     ▼
                  view  model  service
                     ╲     │     ╱
                      ╲    ▼    ╱
                    domain  shared
```

| 层 | 目录 | 允许依赖 | 职责 |
|---|---|---|---|
| Domain | `src/domain` | 自身、C++ 标准库 | 棋盘、规则、图案、RLE、快照历史（纯业务） |
| Shared | `src/shared` | 自身、C++ 标准库 | `LifeStats`/`AppSettings`/`JumpInfo` 只读契约值对象 |
| Model | `src/model` | Domain、Shared、QtCore/QtConcurrent | 可观察状态、命令、异步演化；不引用 View/Controller |
| Service | `src/service` | Domain、Shared、QtCore/QtGui | QSettings、文件、PNG 编码；不引用 Model |
| View | `src/view` | Domain、Shared、QtWidgets | 被动绘制/控件；不引用 Model、Controller、Service |
| Controller | `src/controller` | Domain、Shared、Model、View、Service | 输入、Model→View 推送、View→Model 命令、定时器/对话框 |
| App | `src/app` | 全部 | 组合根与入口 |

### 分层 MVC 的数据流

用户绘制：

```
BoardView::cellPressed(row, col)
  → BoardController::onCellPressed
  → LifeModel::setCell(row, col, !alive)    // 命令
  → LifeModel 更新棋盘、人口、历史
  → LifeModel::boardChanged / statusChanged  // 广播
  → BoardPresenter 查询 Model
  → BoardView::applyFrame(BoardFrame)        // 原子帧
  → ControlPanel::applyState(PanelState)     // 原子面板状态
```

模拟定时器：

```
QTimer::timeout
  → SimulationController::onTick
  → LifeModel::requestStep()
  → SimulationEngine 在 Qt 线程池按行并行计算
  → QFutureWatcher::finished
  → LifeModel::onStepReady() → applyStep()
  → boardChanged / statusChanged
  → BoardPresenter 推送到 BoardView / ControlPanel
```

Controller **不**复制整块棋盘；它把 `const domain::Grid*` 交给 View，
因此每帧没有 160 KB 的棋盘拷贝，同时 View 也不知道 `LifeModel` 类型。

### 目录

```
life_game.pro                 主程序
life_game.pri                 模块源文件清单与编译选项（各 .pro 共用）
docs/architecture.md          架构契约与评审清单
docs/performance.md           数据结构与并发性能报告
src/domain/                   纯业务 C++，无 Qt（含 1 bit/cell 快照 PackedGrid）
src/shared/                   跨层只读契约值对象（无 Qt、无行为）
src/model/                    可观察 Model + CommandResult + 异步管线
src/service/                  QSettings / RLE / PNG + FileError/SettingsError
src/view/                     自绘棋盘、面板、对话框、确认/提示、目录、主题
src/controller/               输入、Presenter、会话/设置、错误文案映射
src/app/                      组合根、main（stacklog 初始化）
third_party/singleproc/       内嵌库：单实例锁
third_party/stacklog/         内嵌库：调用链日志
tests/                        domain / model / view / controller / ui
tools/check_layering.py       层间 include / 纯净度门禁
tools/layering_selftest.py    门禁负向 fixture
```

### 关键约束

- `BoardView` / `ControlPanel` 不包含任何 `model/` 头文件，不持有 `LifeModel*`；
  BoardPresenter 通过 `applyFrame/applyState` 原子推送显示状态。
- Controller 是除 App 组合根之外，唯一同时持有可写 Model 与 View 的层；
  View 的信号只连接到 Controller，Model 的信号也只由 Controller 消费。
- `LifeModel` 不包含 View/Controller/Service，也不出现 `QFile`、`QSettings`、`QFileDialog`
  或中文用户文案；Service 不依赖 Model/View/Controller。
- Domain/Shared 不包含 Qt；`LifeStats`/`AppSettings`/`JumpInfo` 位于 Shared，
  供 Model/Service/View/Controller 只读使用。
- 中文文案只出现在 View/Controller 和 App 外壳（菜单/标题/帮助），Model/Domain 不出现中文，
  UI 文案通过 `tr()` 提供翻译入口。

## 构建

用 **qmake**，需要 Qt 6（`qmake6`）、C++17 编译器；测试与架构门禁还需要 Python 3。

主程序与每个测试都是**独立工程**：各自 `qmake` + `make`，不依赖先构建库，也不用
先编主程序。公共的源文件清单与编译选项在 `life_game.pri`，由各 `.pro` include。

```bash
# 主程序
mkdir -p build/app && cd build/app
qmake6 ../../life_game.pro && make -j4
cd ../.. && ./build/app/lifegame

# 全部测试（各自构建、各自运行，最后跑两个分层检查）
./tests/run_all.sh

# 只跑一个
./tests/run_all.sh domain
```

单个测试也可以完全独立地构建：

```bash
mkdir -p build/tests/view && cd build/tests/view
qmake6 /路径/life_game/tests/view_tests.pro && make -j4
QT_QPA_PLATFORM=offscreen ./bin/view_tests
```

界面相关用例（`view_tests` / `controller_tests` / `ui_tests`）需要离屏平台插件，
`tests/run_all.sh` 会统一导出 `QT_QPA_PLATFORM=offscreen`；单独跑时自己带上。

### 一处环境限制：仓库路径含非 ASCII 字符

qmake 会把源码目录的**绝对路径**写进 Makefile 的若干规则里，`make` 执行这些规则
时对非 ASCII 路径处理不当（例如仓库在 `~/桌面/...` 下，路径会变成 `??`）。表现为
一条 `cd: can't cd to ...` 警告，构建通常仍能完成，但如果目标产物写在仓库内就会失败。

规避办法是经典的 shadow build —— 把构建目录放到纯 ASCII 路径下：

```bash
mkdir -p /tmp/lifegame-build && cd /tmp/lifegame-build
qmake6 /path/to/life_game/life_game.pro && make -j4
```

`tests/run_all.sh` 把产物放在仓库内的 `build/tests/` 下（能正常工作），若在非 ASCII
路径的仓库里遇到异常，改用上面的方式即可。

### 为什么是 Qt 6

代码用了 C++17 的 `std::optional` / `std::string_view`，以及 Qt 5.7 才引入的
`QOverload`。系统 Qt 5 是 5.6.3，两者都不满足，所以这一份挂 Qt 6。

### 编译选项

`life_game.pri` 里统一设：`-Wall -Wextra -Wpedantic` 与 `-fno-exceptions`
（与原来的 `LIFEGAME_NO_EXCEPTIONS=ON` 一致）。要开 ASan/UBSan 时在命令行上加：

```bash
qmake6 ../../life_game.pro "QMAKE_CXXFLAGS+=-fsanitize=address,undefined" \
       "QMAKE_LFLAGS+=-fsanitize=address,undefined"
```

## 测试

```bash
./tests/run_all.sh
```

每个测试是**独立工程**（各自的 `.pro`），脚本逐个 `qmake6` + `make` 并运行，最后
跑两个分层检查。七个用例：

| 用例 | 依赖 | 覆盖 |
|---|---|---|
| `domain_tests` | 无 Qt | 规则、Grid、边界、图案库、RLE（含恶意输入、行/列诊断、缺少 `!`）、PackedGrid、快照历史 |
| `model_tests` | QtCore | 演化、异步步进、停表、规则/边界、RLE 载入、跳转/取消、跳转中拒绝载入、错误码 |
| `view_tests` | QtWidgets | 速度刻度、滑块不回写、被动 setter、1500×1000 渲染/导出、设置对话框、进度框关闭 |
| `controller_tests` | QtWidgets | Bresenham、面板信号、绘制/图案、定时器、跳转方向/进度、Model→View 推送、文件路径、设置错误、会话恢复 |
| `ui_tests` | QtWidgets | 完整 MainWindow、确认/取消、Space、网格、1500×1000 渲染/模拟、截图 |
| `layering` | Python | include 方向、绕过、CJK、源码清单完整性 |
| `layering_selftest` | Python | 架构门禁自身的负向 fixture |

UI 测试输出 `/tmp/lifegame-ui-tests.png`。界面用例需要离屏平台插件：
`tests/run_all.sh` 统一导出 `QT_QPA_PLATFORM=offscreen`，无需 Xvfb。

### 质量门禁

- 编译开启 `-Wall -Wextra -Wpedantic`（在 `life_game.pri` 里统一设置）；默认
  `-fno-exceptions`，异常在编译期被拒绝。要当错误处理就加
  `"QMAKE_CXXFLAGS+=-Werror"`。
- `python3 tools/check_layering.py .` 检查 include 方向、`src/`/`..` 绕过、
  Domain/Shared/Model 的 Qt/中文纯净度、View/Service 的 Model 隔离、以及
  `life_game.pri` 的源文件清单是否完整。
- `tools/layering_selftest.py` 用负向 fixture 验证门禁确实会失败。
- CI：普通构建 + ASan/UBSan 两个 job，均执行 `tests/run_all.sh`。
- 新增源文件必须显式加入 `life_game.pri`（禁止 glob），并放入正确的层目录。

本仓库还提供两份外部工具配置，报告和日志都已加入 `.gitignore`：

```bash
# cxx_audit：clang-tidy + PMD CPD + Valgrind（ASan 视运行环境而定）
PYTHONPATH=../cxx_audit/src python3 -m cpp_audit -c audit.toml all
PYTHONPATH=../cxx_audit/src python3 -m cpp_audit -c audit.toml build --kind asan

# cxx_ci：clean -> configure -> build -> 全部测试 + 分层门禁
python3 ../cxx_ci/ci/ci.py -c ci.toml
```

结果在 `reports/`，CI 完整日志在 `ci-logs/`。当前 PRoot-Distro 环境无法运行
ASan（`sanitizer_allocator_primary64.h` CHECK failed），因此 `audit.toml`
默认不把 ASan 运行纳入 `all`，只用 `build --kind asan` 验证构建；普通 Linux
主机上把 `[asan].enabled` 改回 `true` 即可。

## 设计细节

- **定长 Grid**：`std::vector<Cell>` 只分配当前 `width × height` 的活动区域，每格 1 字节；
  同尺寸 `resize()` 不清零，避免每代重复清零。大棋盘（1500 × 1000 = 150 万格）
  约占 1.5 MB/棋盘，Grid 对象本身很小，不会压爆栈。
- **人口缓存**：Model 维护 `m_population`，`stats()` O(1)；View 不再逐帧扫描棋盘。
- **异步演化**：`SimulationEngine` 用 `QtConcurrent::mapped` 按行切片，
  `QFutureWatcher` 回 UI 线程；任务只读快照、只写自己的行区间。
- **背压与失效**：上一代仍在计算时忽略新的定时器请求；编辑/暂停/规则变化调用
  `discardPending()`，旧结果不会覆盖新状态。
- **快照回退**：生命游戏多对一、不可逆，因此按间隔存快照并正向重放；
  默认每 100 代一张，最多 64 张 + 锚点。快照使用 **1 bit/cell 的 `PackedGrid`**，
  1500 × 1000 最大棋盘约 187.5 KB/张，65 张约 12 MB（活棋盘仍是 1 byte/cell，
  保证模拟和渲染的热路径）。跳转在后台线程完成，
  UI 不逐代刷新，进度通过 queued invocation 回到 UI 线程。
- **View 无拷贝**：Controller 把 `const domain::Grid*` 传给 `BoardView`；演化中把每格
  压成一个像素后整块缩放贴图，暂停后恢复逐格矢量绘制。
- **导出上限**：PNG 使用固定 8 px/格，并把单边限制在 4096 px；不会因为缩放到
  1500 × 1000、48 px/格而申请数十 GiB 的 QImage；导出始终限制在 4096 边长内。
- **滑块不回写**：`ControlPanel` 只在外部载入设置时 `setSpeed`；Controller 在拖动
  过程中只重启定时器，不会把滑块"吸回"到近似位置。
- **大棋盘帧合并**：小棋盘立即刷新；运行中的大棋盘（>25 万格）读取屏幕刷新率，
  只有屏幕刷新率低于当前代速时才合并，屏幕足够快时逐代刷新。
- **随机填充**：Model 用高精度时钟与对象地址混合出种子（避免 `std::random_device`
  可能抛异常），因此每次结果不同；`Grid::randomize(density, seed)` 仍保留显式种子，
  便于测试与复现。
- **设置持久化**：`SettingsStore` 负责 QSettings 键；`SessionController` 负责启动恢复
  与退出保存；密度、速度、网格、规则、边界、代数、窗口几何都会往返。
- **错误分层**：Domain 用 `RleError` 枚举；Model 用 `CommandResult`；Service 用
  `FileError`/`SettingsError` 并附可选系统详情；Controller 统一映射为一条中文提示；
  Model 不产生用户文案。可恢复错误不使用异常；分配失败被视为不可恢复（进程终止）。
- **日志与单实例**：`main.cpp` 直接调用 stacklog；各层使用
  `STACKLOG_DEBUG/INFO/WARN/ERROR` 记录低频状态迁移与失败，不引入
  `QLoggingCategory`，也不逐代/逐格刷日志。`singleproc::Guard` 在事件循环前取得
  文件锁，重复启动直接退出，不把旧窗口带到前台。

## C++17 用法

项目不是"为了用 C++17 而用 C++17"，性能相关的用法只出现在有收益的地方：

- `std::string_view`：RLE/规则/图案查找全程借用，不复制字符串。
- `std::optional`：`RleDecodeResult` 用值语义表达"成功/失败"，不做异常控制流。
- `std::clamp` / `inline constexpr`：尺寸、密度、速度、快照范围共享同一组编译期常量。
- `constexpr`：`Rule` 的构造、`births/survives`、`Grid::width/height`、
  错误名、设置范围与 `shared::normalized` 可在编译期求值；测试中有 `static_assert`。
- 移动语义：`Grid` 使用 `std::vector` 活动存储并支持 `noexcept` move；
  `SimulationEngine` 把结果 move 给 Model，`LifeModel::applyStep` 再 move 进棋盘，
  避免 1.5 MB 棋盘的深拷贝。`static_assert` 验证 `Grid`/`Rule` 的 nothrow move。
- `std::from_chars`：RLE 头部整数解析不再走 `stoi`/手动乘加，且不分配。
- `std::to_chars`：RLE 编码的游程长度写入栈上缓冲区，避免每个 run 一次
  `std::to_string` 分配；大棋盘导出 RLE 时减少分配抖动。
- 没有强行使用 `if constexpr`：当前热路径没有类型相关的编译期分支，加它只会增加
  阅读成本；算法、缓存布局和 `QImage` 路径才是大棋盘性能的主要杠杆。

## 性能

详见 **[docs/performance.md](docs/performance.md)**。历史测量（8 核 aarch64 / Qt 5.15）：

| 项目 | 提升 |
|---|---:|
| `Grid` 串行演化 vs 旧 `LifeBoard` | 约 **6.8 – 7.9×** |
| 400 × 400 并行计算 vs 新串行 | **2.53×**（历史数据） |
| 400 × 400 异步管线 vs 旧同步 | 约 **19.7×** 吞吐（历史数据） |
| UI 线程每代阻塞（400 × 400） | 11.49 ms → **0.172 ms**（历史数据） |

100 × 70 等小棋盘上并行派发开销大于收益，因此只在面积 ≥ 20,000 格时多分块；
主要收益来自固定数组 Grid、人口缓存与异步解耦。

## 已知限制

- 棋盘为有限尺寸：宽度最大 1500，高度最大 1000；位压缩快照最高约 12 MB。
- 不提供撤销/重做；
- 单步按钮在 UI 线程同步计算一代；连续模拟完全异步；
- 并行计算使用 Qt 全局线程池，若应用其他部分占用大量池线程，模拟会自动排队；
- 回退范围受快照间隔与 64 张上限约束；
- 当前只提供中文文案；`tr()` 已就位，但未附带 `.ts` 翻译文件；
- 文件对话框不记忆上次打开目录；
- 分配失败/OOM 被视为不可恢复（`-fno-exceptions` 下进程终止），不会弹错误框；
- 未提供 `.desktop`/安装包集成；安装即拷贝 `build/app/bin/lifegame`。

## 第三方资源

- 工具栏与按钮图标来自 [Lucide](https://lucide.dev)（ISC License），
  许可证见 `resources/icons/LICENSE-lucide.txt`。
- `third_party/singleproc`、`third_party/stacklog` 是内嵌第三方源码，随主程序一起编译。

## 许可

MIT，见 [LICENSE](LICENSE)。
