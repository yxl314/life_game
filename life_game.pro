# 生命游戏 —— 主程序
#
# 独立项目：本文件单独 qmake + make 就能出可执行文件，不依赖测试工程。
#     mkdir -p build/app && cd build/app
#     qmake6 ../../life_game.pro && make -j
#
# 用 Qt 6（代码用 C++17 的 std::optional / std::string_view，且用到 Qt 5.7 才
# 有的 QOverload；系统 Qt 5 是 5.6.3，编不过）。

QT += core gui widgets concurrent

TEMPLATE = app
TARGET   = lifegame
CONFIG  += release
CONFIG  -= app_bundle

LIFEGAME_ROOT = $$PWD
include($$LIFEGAME_ROOT/life_game.pri)

HEADERS += $$LIFEGAME_SHARED_HEADERS

HEADERS += $$LIFEGAME_APP_HEADERS
SOURCES += $$LIFEGAME_APP_SOURCES
HEADERS += $$LIFEGAME_CONTROLLER_HEADERS $$LIFEGAME_MODEL_HEADERS \
           $$LIFEGAME_VIEW_HEADERS $$LIFEGAME_SERVICE_HEADERS \
           $$LIFEGAME_DOMAIN_HEADERS
SOURCES += $$LIFEGAME_CONTROLLER_SOURCES $$LIFEGAME_MODEL_SOURCES \
           $$LIFEGAME_VIEW_SOURCES $$LIFEGAME_SERVICE_SOURCES \
           $$LIFEGAME_DOMAIN_SOURCES
RESOURCES += $$LIFEGAME_VIEW_RESOURCES
SOURCES += $$LIFEGAME_MAIN_SOURCES

HEADERS += $$LIFEGAME_SINGLEPROC_HEADERS $$LIFEGAME_STACKLOG_HEADERS
SOURCES += $$LIFEGAME_SINGLEPROC_SOURCES $$LIFEGAME_STACKLOG_SOURCES
unix: LIBS += -pthread

# 产物落在构建目录里，不写回仓库：仓库路径可能含非 ASCII 字符（例如"桌面"），
# 而 qmake/make 在规则里传绝对路径时会把它们写坏成 ??，链接直接失败。
# 这也正是 shadow build 的常规做法——源码目录保持干净。
DESTDIR     = $$OUT_PWD/bin
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR     = $$OUT_PWD/moc
RCC_DIR     = $$OUT_PWD/rcc
