# 模块源文件清单与编译选项 —— 主程序和测试都 include 这一份。
#
# 每个 .pro 自己编译它需要的模块源码，不依赖预先构建的静态库：这样主程序与
# 各测试都是**独立项目**，单独 qmake + make 就能出可执行文件，不必先建库。
# 代价是同一份源码在多个构建里各编一次——换来的是"随便打开哪个 .pro 都能跑"。
#
# 用法（在 .pro 里，先定根目录再 include）：
#     LIFEGAME_ROOT = $$PWD
#     include($$LIFEGAME_ROOT/life_game.pri)

# 由包含它的 .pro 先给出：
#     LIFEGAME_ROOT = $$PWD        （主程序）
#     LIFEGAME_ROOT = $$PWD/..     （tests/ 下的测试）

# 各模块的源文件

LIFEGAME_DOMAIN_HEADERS = \
    $$LIFEGAME_ROOT/src/domain/grid.h \
    $$LIFEGAME_ROOT/src/domain/rule.h \
    $$LIFEGAME_ROOT/src/domain/pattern.h \
    $$LIFEGAME_ROOT/src/domain/pattern_library.h \
    $$LIFEGAME_ROOT/src/domain/rle.h \
    $$LIFEGAME_ROOT/src/domain/history.h \
    $$LIFEGAME_ROOT/src/domain/board_limits.h \
    $$LIFEGAME_ROOT/src/domain/boundary.h \
    $$LIFEGAME_ROOT/src/domain/packed_grid.h \
    $$LIFEGAME_ROOT/src/domain/text_util.h \
    $$LIFEGAME_ROOT/src/domain/cell.h

LIFEGAME_DOMAIN_SOURCES = \
    $$LIFEGAME_ROOT/src/domain/grid.cpp \
    $$LIFEGAME_ROOT/src/domain/rule.cpp \
    $$LIFEGAME_ROOT/src/domain/pattern.cpp \
    $$LIFEGAME_ROOT/src/domain/pattern_library.cpp \
    $$LIFEGAME_ROOT/src/domain/rle.cpp \
    $$LIFEGAME_ROOT/src/domain/history.cpp \
    $$LIFEGAME_ROOT/src/domain/packed_grid.cpp

LIFEGAME_MODEL_HEADERS = \
    $$LIFEGAME_ROOT/src/model/life_model.h \
    $$LIFEGAME_ROOT/src/model/simulation_engine.h \
    $$LIFEGAME_ROOT/src/model/jump_runner.h \
    $$LIFEGAME_ROOT/src/model/command_result.h \
    $$LIFEGAME_ROOT/src/model/stall_reason.h

LIFEGAME_MODEL_SOURCES = \
    $$LIFEGAME_ROOT/src/model/life_model.cpp \
    $$LIFEGAME_ROOT/src/model/simulation_engine.cpp \
    $$LIFEGAME_ROOT/src/model/jump_runner.cpp

LIFEGAME_VIEW_HEADERS = \
    $$LIFEGAME_ROOT/src/view/board_view.h \
    $$LIFEGAME_ROOT/src/view/control_panel.h \
    $$LIFEGAME_ROOT/src/view/settings_dialog.h \
    $$LIFEGAME_ROOT/src/view/jump_progress_dialog.h \
    $$LIFEGAME_ROOT/src/view/status_bar_view.h \
    $$LIFEGAME_ROOT/src/view/dialogs.h \
    $$LIFEGAME_ROOT/src/view/interfaces.h \
    $$LIFEGAME_ROOT/src/view/message_box_confirm_sink.h \
    $$LIFEGAME_ROOT/src/view/view_state.h \
    $$LIFEGAME_ROOT/src/view/theme.h \
    $$LIFEGAME_ROOT/src/view/rule_catalog.h \
    $$LIFEGAME_ROOT/src/view/pattern_catalog.h \
    $$LIFEGAME_ROOT/src/view/speed_scale.h

LIFEGAME_VIEW_SOURCES = \
    $$LIFEGAME_ROOT/src/view/board_view.cpp \
    $$LIFEGAME_ROOT/src/view/control_panel.cpp \
    $$LIFEGAME_ROOT/src/view/settings_dialog.cpp \
    $$LIFEGAME_ROOT/src/view/jump_progress_dialog.cpp \
    $$LIFEGAME_ROOT/src/view/status_bar_view.cpp \
    $$LIFEGAME_ROOT/src/view/dialogs.cpp \
    $$LIFEGAME_ROOT/src/view/message_box_confirm_sink.cpp \
    $$LIFEGAME_ROOT/src/view/theme.cpp \
    $$LIFEGAME_ROOT/src/view/rule_catalog.cpp \
    $$LIFEGAME_ROOT/src/view/pattern_catalog.cpp \
    $$LIFEGAME_ROOT/src/view/speed_scale.cpp

LIFEGAME_VIEW_RESOURCES = \
    $$LIFEGAME_ROOT/resources/resources.qrc

LIFEGAME_SERVICE_HEADERS = \
    $$LIFEGAME_ROOT/src/service/session_state.h \
    $$LIFEGAME_ROOT/src/service/file_error.h \
    $$LIFEGAME_ROOT/src/service/settings_error.h \
    $$LIFEGAME_ROOT/src/service/settings_store.h \
    $$LIFEGAME_ROOT/src/service/rle_file_service.h \
    $$LIFEGAME_ROOT/src/service/image_file_service.h

LIFEGAME_SERVICE_SOURCES = \
    $$LIFEGAME_ROOT/src/service/settings_store.cpp \
    $$LIFEGAME_ROOT/src/service/rle_file_service.cpp \
    $$LIFEGAME_ROOT/src/service/image_file_service.cpp

LIFEGAME_CONTROLLER_HEADERS = \
    $$LIFEGAME_ROOT/src/controller/line_raster.h \
    $$LIFEGAME_ROOT/src/controller/error_text.h \
    $$LIFEGAME_ROOT/src/controller/board_presenter.h \
    $$LIFEGAME_ROOT/src/controller/board_controller.h \
    $$LIFEGAME_ROOT/src/controller/simulation_controller.h \
    $$LIFEGAME_ROOT/src/controller/jump_controller.h \
    $$LIFEGAME_ROOT/src/controller/file_controller.h \
    $$LIFEGAME_ROOT/src/controller/configuration_controller.h \
    $$LIFEGAME_ROOT/src/controller/session_controller.h \
    $$LIFEGAME_ROOT/src/controller/session_service.h \
    $$LIFEGAME_ROOT/src/controller/session_settings.h \
    $$LIFEGAME_ROOT/src/controller/controller_settings_bridge.h \
    $$LIFEGAME_ROOT/src/controller/settings_controller.h

LIFEGAME_CONTROLLER_SOURCES = \
    $$LIFEGAME_ROOT/src/controller/line_raster.cpp \
    $$LIFEGAME_ROOT/src/controller/error_text.cpp \
    $$LIFEGAME_ROOT/src/controller/board_presenter.cpp \
    $$LIFEGAME_ROOT/src/controller/board_controller.cpp \
    $$LIFEGAME_ROOT/src/controller/simulation_controller.cpp \
    $$LIFEGAME_ROOT/src/controller/jump_controller.cpp \
    $$LIFEGAME_ROOT/src/controller/file_controller.cpp \
    $$LIFEGAME_ROOT/src/controller/configuration_controller.cpp \
    $$LIFEGAME_ROOT/src/controller/session_controller.cpp \
    $$LIFEGAME_ROOT/src/controller/session_service.cpp \
    $$LIFEGAME_ROOT/src/controller/controller_settings_bridge.cpp \
    $$LIFEGAME_ROOT/src/controller/settings_controller.cpp

LIFEGAME_APP_HEADERS = \
    $$LIFEGAME_ROOT/src/app/main_window.h

LIFEGAME_APP_SOURCES = \
    $$LIFEGAME_ROOT/src/app/main_window.cpp

# shared：只有头文件的共享类型与纯函数
LIFEGAME_SHARED_HEADERS = \
    $$LIFEGAME_ROOT/src/shared/app_settings.h \
    $$LIFEGAME_ROOT/src/shared/jump_info.h \
    $$LIFEGAME_ROOT/src/shared/life_stats.h

# 入口：只有主程序需要
LIFEGAME_MAIN_SOURCES = \
    $$LIFEGAME_ROOT/src/app/main.cpp

# 内嵌第三方库：单实例锁与日志。它们不属于 src 分层，只由需要它们的工程加入。
LIFEGAME_SINGLEPROC_HEADERS = \
    $$LIFEGAME_ROOT/third_party/singleproc/include/singleproc/singleproc.hpp

LIFEGAME_SINGLEPROC_SOURCES = \
    $$LIFEGAME_ROOT/third_party/singleproc/src/singleproc.cpp

LIFEGAME_STACKLOG_HEADERS = \
    $$LIFEGAME_ROOT/third_party/stacklog/include/stacklog/stacklog.hpp \
    $$LIFEGAME_ROOT/third_party/stacklog/include/stacklog/version.hpp

LIFEGAME_STACKLOG_SOURCES = \
    $$LIFEGAME_ROOT/third_party/stacklog/src/stacklog.cpp

# 编译选项：-Wall -Wextra -Wpedantic，并关掉异常
CONFIG += c++17
QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
QMAKE_CXXFLAGS += -fno-exceptions

INCLUDEPATH += \
    $$LIFEGAME_ROOT/src \
    $$LIFEGAME_ROOT/third_party/singleproc/include \
    $$LIFEGAME_ROOT/third_party/stacklog/include

