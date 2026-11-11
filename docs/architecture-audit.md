# 标准分层 MVC 独立审计

本审计不使用 `docs/architecture.md` 作为证据，而是直接检查源码 include、`life_game.pri` 的模块清单、
测试依赖和运行行为。采用的验收基准是**分层 MVC + 被动视图**：

- View 不知道 Model，不 include `model/`，不连接 Model 信号；
- Controller 是唯一同时持有可写 Model 与 View 的层（App 组合根除外）；
- Model/Service/View 只依赖 Domain/Shared，不互相依赖；
- Domain/Shared 不依赖 Qt，Model 不依赖 UI/文件/设置；
- 模块清单的依赖方向与 include 方向一致；
- 有负向测试证明门禁会拒绝绕过。

## 审计结果

| 规则 | 证据 | 结果 |
|---|---|---|
| View 不 include Model | `grep -rn '#include "model/' src/view` 无输出 | 通过 |
| View 不持有 `LifeModel*`/不连 Model 信号 | `grep -rn 'LifeModel\|model::' src/view` 无输出 | 通过 |
| View 只依赖 Domain/Shared | `tests/view_tests.pro` 只加入 `LIFEGAME_VIEW_*` + `LIFEGAME_DOMAIN_*` + Shared 头文件 | 通过 |
| View 是被动 setter | `BoardView::applyFrame(BoardFrame)` 一次性推送棋盘/人口/运行态；`ControlPanel::applyState(PanelState)` 推送面板状态 | 通过 |
| Controller 是唯一 Model↔View 中介 | `BoardPresenter` 订阅 Model 信号后调用 View `applyFrame`/`applyState`；View 信号只连 Controller | 通过 |
| Service 不依赖 Model | `grep -rn '#include "model/' src/service` 无输出；Service 只 include Domain/Shared | 通过 |
| Model 不依赖 View/Controller/Service | `grep` 无输出；Model 只 include Domain/Shared 与 QtCore/QtConcurrent | 通过 |
| Domain 不依赖 Qt | 源码无 `<Q...>` include；`check_layering.py` 拒绝 Domain Qt 头 | 通过 |
| Shared 只依赖自身 | `src/shared` 无任何层 include；无 Qt | 通过 |
| Controller 不依赖 App | `grep -rn '#include "app/' src/controller` 无输出 | 通过 |
| App 不直接命令 Model | `grep -rn 'm_model->' src/app` 无输出；App 只组合/转发 Controller | 通过 |
| 测试能脱离 Model 测 View | `view_tests` 只加入 View/Domain 源码；用 `domain::Grid` 构造棋盘 | 通过 |
| 门禁是否只是“文档 OK” | `layering_selftest` 负向 fixture：View→Model、Service→Model、Shared→Qt、Manifest 分组错位全部失败 | 通过 |
| 目标图 | `model/service/view → domain/shared`；`controller → 三者`；`app → controller` | 通过 |

## 源码级证据

```text
src/view  → domain, shared, QtWidgets
src/model → domain, shared, QtCore, QtConcurrent
src/service → domain, shared, QtCore, QtGui
src/controller → model, view, service, domain, shared, QtWidgets
src/app → controller
```

`tools/check_layering.py` 对以下项返回非零：

- View 或 Service include `model/`
- `src/` 前缀、`..`/绝对路径绕过
- Domain/Shared/Model 的 Qt 或中文污染
- `life_game.pri` 未列文件
- `LIFEGAME_*_SOURCES/HEADERS` 分组与文件所在层不一致
- 同一个文件被列入多个层分组

`tools/layering_selftest.py` 为上述规则构造临时源码树，确认门禁会失败，而不是只会在
正确代码上打印 `Layering OK`。

## 与“经典 Observer MVC”的区别

原实现属于经典 Observer MVC：View 直接持有 `const LifeModel*`、连接 Model 信号并读取
`model.board()`。这是合法 MVC，但**不是**分层 MVC 中“Controller 中介、View 被动”的变体。
本次重构后：

- View 不再知道 `LifeModel` 类型；
- Model 信号只由 Controller 消费；
- `BoardPresenter` 用 `applyFrame(BoardFrame)`/`applyState(PanelState)` 推送完整状态；
- 棋盘仍不复制：`BoardView` 只持有 `const domain::Grid*`。

## 仍需说明的设计选择

1. **Shared 层**：`LifeStats`/`AppSettings`/`JumpInfo` 是跨层 DTO，不是业务实体，因此放在
   `src/shared`，避免污染 Domain；`shared` 不依赖任何其他层。
2. **Model 使用 Qt signal/QtConcurrent**：这是 Qt 应用模型，不是纯领域实体；纯业务规则在
   `domain`，可脱离 Qt 测试。
3. **View 是具体类，不是接口**：Controller 直接调用具体 View 的 setter；这是桌面 Qt MVC
   的正常做法。`MessageSink`/`ConfirmSink`/`JumpProgressView` 三个输出端口保留接口化，
   便于测试确认/取消/进度。
4. **App 组合根**持有 Model/View 指针用于注入，但不直接调用 Model 命令；这是组合根例外，
   不是 Controller 职责泄漏。
5. **`SessionController` 承担启动恢复/退出保存**：它是 Controller 层的应用协调器；不是
   业务规则，也不绕过 Model/Service。

## 复核命令

```bash
./tests/run_all.sh
python3 tools/check_layering.py .
python3 tools/layering_selftest.py
```

当前结果：主程序和 6 个测试工程通过；门禁 99 个源文件通过；负向 fixture 全部按预期失败。

## 复审改进落实

以下问题已在后续修订中处理：

1. **输入状态所有权**：`SimulationController`、`JumpController`、`BoardController`
   分别持有速度、跳转粒度、密度/尺寸粒度/网格状态；`ControlPanel` 只显示并发出
   带值信号。Controller 不再从 View 回读输入状态。
2. **命令结果语义**：`setCell`/`setCells`/`toggleCell` 统一返回 `CommandResult`，
   可区分 `Busy`/`OutOfBounds`/`NoChange`/`Success`。`clear`/`randomize`/`step`
   是总是执行的命令，保留 `Success` 语义。
3. **会话恢复状态**：`SessionRestoreResult` 用 `SessionRestoreStatus` 枚举表达
   `NoStoredBoard`/`Restored`/`PartialRestore`/`DecodeFailed`/`LoadFailed`，
   不再依赖多个互相组合的 bool；规则、代数和 fallback 分配结果都有显式检查。
4. **SessionController 解耦**：通过 `SessionSettingsPort` 只暴露
   `apply/capture`；`ControllerSettingsBridge` 在组合层适配具体 Controller。
5. **App 的 UI 意图**：缩放菜单改连 `BoardController`；帮助/关于/Qt 对话框文本和
   构造移到 `view/dialogs`。`MainWindow` 只保留组合、菜单和转发。
6. **Domain 不变量**：`Pattern` 的原始构造函数改为私有，外部只能通过
   `Pattern::fromAscii` 或返回 `std::optional` 的 `Pattern::fromCells` 构造。
7. **常量单一来源**：密度范围移入 `shared::limits`，Model 使用共享范围；
   `domain::board_limits.h` 只保留棋盘尺寸。`Grid::kMax*` 作为棋盘常量的别名保留。
8. **热路径统计**：`Grid::computeRows()` 在演化时顺带统计人口和 `changed`；
   `SimulationEngine` 汇总各 chunk 的 `GridStats`；`LifeModel::applyStep()` 不再每代
   额外调用 `Grid::equals()` 和 `Grid::population()`。
9. **OOM 策略**：`PackedGrid` 使用 `new (std::nothrow)` 分配字存储并提供 `valid()`；
   `History` 返回放快照失败；Jump 快照失败返回 `OutOfMemory`。大 Grid 和快照路径
   不再依赖异常。
10. **测试清单**：`logging_tests.pro` 只链 `app/logging` + `model/model_log`；
    新增的 Controller bridge/接口都列入 `LIFEGAME_CONTROLLER_*`，由分层门禁检查。

## 剩余设计取舍

- `SessionService` 对 `setSnapshotInterval`/`setBoundary` 的返回值不做错误展示：
  两者只可能返回 `Success`/`NoChange`（参数已规范化），Board 加载失败才有可操作
  错误。
- `Pattern::isConsistent()` 保留为防御性查询；正常构造路径已经保证不变量。
- `RuleCatalog`/`PatternCatalog` 仍继承 `QObject` 以获得 `tr()` 上下文和父对象
  生命周期，虽然当前没有信号；这不影响分层依赖。
- View 的 `BoardView` 仍通过 `const domain::Grid*` 读取棋盘；这是性能约束下的
  只读契约，不是 Model 依赖。
