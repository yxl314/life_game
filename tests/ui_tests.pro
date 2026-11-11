# ui_tests —— 独立测试工程
#
# 独立项目：本文件单独 qmake6 + make 就能出可执行文件，不需要先构建主程序或
# 其它测试。它自己编译所需的模块源码（清单在 ../life_game.pri）。
#
#     mkdir -p build/tests/ui_tests && cd build/tests/ui_tests
#     qmake6 <仓库>/tests/ui_tests.pro && make -j
#     QT_QPA_PLATFORM=offscreen ./bin/ui_tests
#
# 界面相关用例需要离屏平台插件；tests/run_all.sh 会统一导出。

QT += testlib core gui widgets concurrent

TEMPLATE = app
TARGET   = ui_tests
CONFIG  += console testcase
CONFIG  -= app_bundle

LIFEGAME_ROOT = $$PWD/..
include($$LIFEGAME_ROOT/life_game.pri)

HEADERS += $$LIFEGAME_SHARED_HEADERS

HEADERS += \
    $$LIFEGAME_APP_HEADERS \
    $$LIFEGAME_CONTROLLER_HEADERS \
    $$LIFEGAME_VIEW_HEADERS \
    $$LIFEGAME_SERVICE_HEADERS \
    $$LIFEGAME_MODEL_HEADERS \
    $$LIFEGAME_DOMAIN_HEADERS

SOURCES = \
    $$LIFEGAME_APP_SOURCES \
    $$LIFEGAME_CONTROLLER_SOURCES \
    $$LIFEGAME_VIEW_SOURCES \
    $$LIFEGAME_SERVICE_SOURCES \
    $$LIFEGAME_MODEL_SOURCES \
    $$LIFEGAME_DOMAIN_SOURCES
RESOURCES += $$LIFEGAME_VIEW_RESOURCES

SOURCES += $$PWD/ui_tests.cpp

DESTDIR     = $$OUT_PWD/bin
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR     = $$OUT_PWD/moc
RCC_DIR     = $$OUT_PWD/rcc
