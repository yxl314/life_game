# 架构说明：标准分层 MVC（被动视图）

本文档按**外部可验证的架构规则**描述 life-game，而不是反过来用代码给文档找理由。
验收标准是：依赖图、工程模块、include 约束和测试行为共同成立；文档只是解释。

## 1. 采用的 MVC 变体

MVC 有多个合法变体。本项目明确采用**分层 MVC + 被动视图（Passive View）**：

- **Model** 持有状态、规则和用例，通过信号广播变化，但不知道 View/Controller 的存在；
- **View** 只负责绘制和控件，不读取 Model、不连接 Model 信号；它接收 Controller 推送的
  只读数据和 `const domain::Grid*`，只发出用户意图信号；
- **Controller** 是唯一同时持有 Model 与 View 的协调层，负责：
  1. 把 View 的用户意图翻译成 Model 命令；
  2. 订阅 Model 信号，把最新状态推送给 View；
  3. 把错误码映射为用户提示，并记录低频日志。

这不是“View 直接观察 Model”的经典 Observer MVC，而是分层架构中更严格的中介式 MVC。
两种都叫 MVC，但本项目的验收按前者（View 不知道 Model）执行。

## 2. 依赖规则

```
                    app
                     │
                     ▼
                 controller
                ╱     │      ╲
               ▼      ▼       ▼
             view   model   service
               ╲      │      ╱
                ╲     ▼     ╱
              domain  shared
```

| 层 | 目录 | 允许依赖 | 禁止依赖 | 职责 |
|---|---|---|---|---|
| Domain | `src/domain` | 自身、C++ 标准库 | Qt、Shared、Model、Service、View、Controller、App | 棋盘、规则、图案、RLE、快照历史 |
| Shared | `src/shared` | 自身、C++ 标准库 | Qt、Domain、Model、Service、View、Controller、App | `LifeStats`/`AppSettings`/`JumpInfo` 只读契约 |
| Model | `src/model` | Domain、Shared、QtCore/QtConcurrent | Service、View、Controller、App | 可观察的应用状态、命令、异步演化 |
| Service | `src/service` | Domain、Shared、QtCore/QtGui | Model、View、Controller、App | QSettings、RLE 文件、PNG 编码 |
| View | `src/view` | Domain、Shared、QtWidgets | Model、Service、Controller、App | 被动绘制、控件、确认/进度接口 |
| Controller | `src/controller` | Domain、Shared、Model、Service、View | App | 输入编排、Model→View 推送、错误文案、会话协调 |
| App | `src/app` | 全部 | — | `main()`、组合根、菜单/工具栏/状态栏装配 |

**`life_game.pri` 里的模块清单必须与上表一致**：

```
LIFEGAME_DOMAIN_*      → 无业务依赖
LIFEGAME_SHARED_*      → header-only 契约，无业务依赖
LIFEGAME_MODEL_*       → Domain + Shared（QT += core concurrent）
LIFEGAME_SERVICE_*     → Domain + Shared（QT += core gui）
LIFEGAME_VIEW_*        → Domain + Shared（QT += widgets），含 resources.qrc
LIFEGAME_CONTROLLER_*  → Model + Service + View + Shared（QT += widgets）
LIFEGAME_APP_*         → Controller
LIFEGAME_MAIN_SOURCES  → 只有主程序用
```

每个 `.pro` 只把**自己需要的**那几组 `*_SOURCES`/`*_HEADERS` 加进来：主程序全要，
测试按覆盖范围取子集。这样"依赖方向"在构建文件上也看得见——某个测试多加了 Model
的文件，评审时一眼就能发现。

`tools/check_layering.py` 会拒绝 View/Service 对 `model/` 的 include，也会检查
`src/`/`..` 绕过、Domain/Model 的 Qt/中文污染、源文件是否列入 `life_game.pri`、以及
`LIFEGAME_*_SOURCES/HEADERS` 分组与文件所在层是否一致。`layering_selftest` 用负向
fixture 证明门禁会失败，而不是只会在正确代码上打印 OK。

## 3. 各层职责

### Domain

- 纯 C++17，不包含任何 Qt 头文件；可脱离 Qt 单独编译测试。
- `Grid`、`Rule`、`Pattern`、`PatternLibrary`、`Rle`、`History`、`Boundary`。
- Domain 不产生中文文案，只返回 `RleError`/`RuleParseError` 等技术错误码。

### Shared

- 跨层只读契约：`shared::LifeStats`、`shared::AppSettings`、`shared::JumpInfo`。
- 纯 C++，不依赖 Qt/Domain/其他层；只有值对象、范围常量和无副作用的规范化函数。
- Model/Service/View/Controller 都可以依赖 Shared；Shared 不反向依赖任何层。

### Model

- `LifeModel` 是可观察的应用状态机：棋盘、规则、边界、代数、人口、快照历史、运行/跳转状态。
- 活棋盘保持 1 byte/cell；`History` 快照使用 1 bit/cell 的 `PackedGrid`，
  只在记录和回退时 pack/unpack。
- 对外只有两类接口：
  - **只读查询**：`board()`、`stats()`、`rule()`、`boundary()`、`isRunning()` 等；
  - **命令**：`setCell()`、`step()`、`loadDocument()`、`setRule()`、`startJump()` 等。
- 命令失败返回 `CommandResult`，不弹窗、不拼接用户文案、不做文件 I/O。
- 状态变化通过信号广播；**信号的下游消费者是 Controller，不是 View**。
- 单代并行计算封装在 `SimulationEngine`；跳转重放使用 `QtConcurrent::run`，结果回 UI 线程。
- Model 不 include `service/`、`view/`、`controller/`、`app/`。

### Service

- `SettingsStore`：QSettings 键、损坏值检测、保存状态。
- `RleFileService`：文件读取/写入（`QSaveFile`）、8 MiB 上限、把字节交给 Domain RLE。
- `ImageFileService`：`QImageWriter` PNG 导出与错误详情。
- 错误返回 `FileError`/`SettingsError`；不弹对话框、不包含 Model 状态机。
- Service 只 include Domain、Shared 和 QtCore/QtGui，不 include Model/View/Controller/App。

### View

- `BoardView`：自绘棋盘、缩放/平移、导出图像；通过
  `applyFrame(const BoardFrame&)` 一次性接收棋盘/人口/运行态；不连接 Model 信号、
  不持有 `LifeModel*`，也没有分离的 board/population/running setter。
- `ControlPanel`：侧边面板；通过 `applyState(const PanelState&)` 接收完整显示状态，
  少量 granular setter 只用于控制器设置单项输入；只发出 `playPauseRequested`、
  `clearRequested`、`resizeRequested` 等意图信号。
- `SettingsDialog`：只编辑 `shared::AppSettings` 的三个字段，并保留未编辑字段。
- `JumpProgressDialog`：实现 `JumpProgressView`；程序化关闭时先断开取消回调，
  避免 Controller/Model 销毁顺序造成的 use-after-free。
- `MessageBoxConfirmSink`、`ConfirmSink`、`MessageSink`：Controller 可替换的输出接口。
- View 不 include `model/`、`service/`、`controller/`、`app/`。

### Controller

| Controller | 职责 |
|---|---|
| `BoardController` | 鼠标/图案输入、清空/随机/尺寸/网格/缩放命令；持有密度、尺寸粒度和网格输入状态 |
| `BoardPresenter` | Model→View 推送、屏幕刷新率与帧合并、原子 ViewState；只通过 `FrameTimingSource` 读取绘制耗时 |
| `SimulationController` | 播放/暂停/单步/速度、定时器、停滞与后台失败提示；持有规范速度状态 |
| `JumpController` | 回退/快进、进度视图、取消；持有规范跳转粒度 |
| `FileController` | 打开/保存 RLE、导出 PNG、错误弹窗；通过 `BoardImageSource` 取图 |
| `ConfigurationController` | 规则/边界命令；把规则/边界推送给 `ControlPanel` 和菜单 ActionGroup |
| `SessionController` | 启动恢复、设置应用、退出保存；只通过 `SessionSettingsPort` 访问输入状态 |
| `ControllerSettingsBridge` | 组合适配器：在 `SessionSettingsPort` 与具体 Controller 之间转发 `AppSettings` |
| `SessionService` | 无 UI 的会话读写（Model + SettingsStore） |
| `SettingsController` | 设置对话框输入与应用 |

Controller 是唯一同时持有可写 `LifeModel*` 和具体 View 指针的层（App 组合根除外）。
`MainWindow` 只创建对象、注入依赖、连接菜单；不直接调用 Model 命令。

输入状态（速度、跳转粒度、密度、尺寸粒度、网格开关）由 Controller 持有，`ControlPanel`
只负责显示和发出带值意图信号。`SessionController` 不直接认识具体 Controller，而是通过
`SessionSettingsPort` 的 `apply/capture` 交换完整 `AppSettings`；`ControllerSettingsBridge`
在组合层完成适配。

### App

- `main.cpp`：`QApplication`、`QCommandLineParser`、stacklog 初始化/关闭、singleproc
  单实例锁、创建窗口、事件循环。
- `MainWindow`：组合根；构造 Model/View/Service/Controller，连接菜单/工具栏/状态栏。
- App 可以 include 全部层；它不实现演化规则，也不直接读写 QSettings 键。

## 4. 典型数据流

### 用户绘制（View → Controller → Model）

```
BoardView::cellPressed(row, col)
  → BoardController::onCellPressed
  → LifeModel::setCell(row, col, !alive)
  → LifeModel::boardChanged / statusChanged
  → BoardPresenter（订阅 Model 信号）
  → BoardView::applyFrame
  → BoardView::applyFrame({&model.board(), stats.population, running})
  → ControlPanel::applyState({stats, ...})
```

View 不知道 `LifeModel`；它只收到 `const domain::Grid*` 和值对象。

### 模拟定时器（Controller 驱动）

```
QTimer::timeout
  → SimulationController::onTick
  → LifeModel::requestStep()
  → SimulationEngine（QtConcurrent::mapped，按行分块）
  → QFutureWatcher::finished
  → LifeModel::onStepReady() → applyStep()
  → boardChanged / statusChanged
  → BoardPresenter 查询 Model，推送到 BoardView / ControlPanel
```

### 打开 RLE（Controller 协调）

```
FileController::openRle()
  → QFileDialog（View 输入）
  → RleFileService::read()          // Service，只依赖 Domain
  → domain::decodeRle()             // Domain
  → LifeModel::loadDocument()       // Model
  → boardChanged / configurationChanged / statusChanged
  → Controller 推送到 View
```

## 5. 状态所有权与“无棋盘拷贝”

- `LifeModel` 以值成员 `domain::Grid m_board` 拥有棋盘；`board()` 返回 `const Grid&`。
- `BoardPresenter` 在 `boardChanged`/`boardResized` 时把 `&model.board()` 作为 `const Grid*`
  放进 `BoardFrame`，`BoardView::applyFrame()` 一次性应用；View 不复制棋盘。
- `BoardView::applyFrame()` 在棋盘指针或宽高变化时把缩放重置为自动适配并清空平移；
  平时只 update。
- `shared::LifeStats`、`shared::AppSettings` 是按值传递的小型值对象，允许拷贝。
- 棋盘尺寸变化时 Model 原地更新值成员 `Grid`；指针通常不变，因此 View 以宽高
  变化判断是否重置视图。BoardPresenter 下一帧通过 `applyFrame()` 更新指针，
  View 不缓存网格内容。
- Model 的 `stats()` 是 O(1) 缓存；View 不逐帧扫描棋盘。

### 5.1 大棋盘（宽度 1500 × 高度 1000）

- 上限为宽 1500、高 1000，共 150 万格；x 轴为宽度，y 轴为高度。`Grid` 只分配活动区域
  （`std::vector<Cell>`），Grid 对象本身很小，不会占满线程栈。
- 每一步的 `Grid` 输出通过 vector move 交给 Model；不再使用 1.5 MB 的内联数组。
- 大棋盘在 View 中超过 25 万格时强制走 QImage 缩放路径，避免 150 万次
  `drawRect`；低缩放时绘制粗网格（约每 3 屏幕像素一条），网格开关仍然可见。
- View 只遍历与窗口相交的可见 cell 范围；缩放/平移后不再重建整张棋盘的图像。
- 大棋盘刷新读取 `QScreen::refreshRate()`：屏幕刷新率 ≥ 当前代速时逐代刷新，
  只有屏幕刷新率不足时才合并。
- PNG 导出仍限制单边 4096 px；1500 × 1000 导出为约 3032 × 2032，
  不会按 48 px/格申请数十 GiB。
- 快照是 1 bit/cell 的 `PackedGrid`；默认 64 张在 1500 × 1000 下约 12 MB。

## 6. 错误、日志与并发

### 错误

- 可恢复错误不使用异常；应用默认以 `-fno-exceptions` 编译。
- Domain：`RleError`、`RuleParseError`（值返回 `RuleParseResult`）。
- Model：`CommandResult`（`Success/Adjusted/Clipped/Busy/.../ComputationFailed`）。
- Service：`FileError`、`SettingsError`，可附可选系统详情。
- Controller：`error_text.h` 把错误码映射为一条中文提示；View 不处理技术错误码。
- 普通容器分配失败在 `-fno-exceptions` 下会终止进程；只有显式使用
  `new (std::nothrow)` 的路径返回
  `OutOfMemory`。

### 日志与单实例

- `main.cpp` 直接调用 stacklog；各层使用 `STACKLOG_DEBUG/INFO/WARN/ERROR`。
- 文件 sink 2 MiB × 3 轮转，默认 Info；`--verbose` 打开 Debug，`--log-file` 覆盖路径。
- 只记录低频状态迁移、文件/设置失败、模拟/跳转生命周期；不记录每代、每帧、每格、
  RLE/棋盘正文或窗口几何全文。
- Domain 不引入日志；RLE 错误在 Service/Controller 边界记录。
- `singleproc::Guard("life-game")` 在事件循环前取得文件锁；已有实例时直接退出，
  不把旧窗口带到前台。

### 并发

- `SimulationEngine` 是单代异步管线；`LifeModel` 的跳转重放使用 `QtConcurrent::run`。
- 后台任务只读快照、只写自己的输出；UI 线程不等待计算（析构时的 `waitForFinished` 除外）。
- 编辑/暂停/规则变化使旧结果失效；结果回到 UI 线程后才应用。
- 跳转进度通过 queued invocation 回 UI 线程；取消使用原子标志。

## 7. 测试与门禁

### 测试

| 用例 | 覆盖 |
|---|---|
| `domain_tests` | 规则、Grid、边界、图案、RLE 恶意输入/行诊断、PackedGrid、History |
| `model_tests` | 演化、异步步进、停表、规则/边界、RLE 载入、跳转/取消、错误码 |
| `view_tests` | 被动 setter、速度滑块、导出上限、目录、设置对话框、进度框关闭 |
| `controller_tests` | Model→View 推送、面板信号、绘制/图案、定时器、跳转、文件/设置、会话 |
| `ui_tests` | 完整 MainWindow、确认/取消、Space 焦点、网格同步、截图 |
| `layering` | include 方向、View/Service↔Model 隔离、CJK/Qt 纯净度、`life_game.pri` 清单 |
| `layering_selftest` | 门禁的负向 fixture；覆盖模块路径 Qt 头、非层 include、清单注释等绕过 |

`view_tests` 的 `.pro` 只加入 View 与 Domain 的源码；它使用
`domain::Grid` 直接构造棋盘，不链接 Model，证明 View 可以脱离 Model 测试。

### 门禁

- `tools/check_layering.py`：include 方向、`src/`/`..`、View/Service 的 Model 隔离、
  Domain/Model 的 Qt/中文、`life_game.pri` 的源文件清单是否完整。
- `tools/layering_selftest.py`：对每个绕过/隔离规则构造负向 fixture。
- 模块清单按层拆分；`view`/`service` 只依赖 `domain`（+ `shared`）。
- 新增文件必须放入正确层目录并显式加入 `life_game.pri`（禁止 glob）。

## 8. 评审清单（按标准 MVC 验收）

- [ ] View 是否包含任何 `model/` 头文件？应为否。
- [ ] View 是否直接持有 `LifeModel*` 或连接 Model 信号？应为否。
- [ ] View 是否只通过 Controller 调用的 setter 接收显示状态？应为是。
- [ ] Service 是否包含 `model/` 头文件？应为否。
- [ ] Model 是否包含 View/Controller/Service 头文件？应为否。
- [ ] Controller 是否是唯一同时持有可写 Model 与 View 的层（App 组合根除外）？应为是。
- [ ] Domain 是否仍然不包含任何 Qt 头文件/中文文案？应为是。
- [ ] 用户输入是否只经 Controller 变成 Model 命令？应为是。
- [ ] Model 变化是否只经 Controller 推送到 View？应为是。
- [ ] `life_game.pri` 的模块清单是否与依赖表一致？`view`/`service` 是否只依赖 Domain + Shared？
- [ ] 新增状态变化是否有对应测试，尤其是 Model→View 推送和 View→Model 命令？
- [ ] 可恢复错误是否返回错误码，而不是异常/静默 bool？
- [ ] 是否只在低频、可行动路径记录日志，且没有逐代/逐格/正文日志？
