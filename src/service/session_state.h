#pragma once

/**
 * @file session_state.h
 * @brief 跨次运行持久化的会话值对象。
 */

#include <QByteArray>
#include <QString>

#include "domain/boundary.h"
#include "shared/app_settings.h"

namespace lifegame::service
{

/// @brief 跨次运行持久化的全部内容。
struct SessionState
{
    shared::AppSettings settings;                       ///< 用户设置。
    QString boardRle;                                   ///< 编码后的棋盘。
    QString ruleNotation = QStringLiteral("B3/S23");    ///< 规范规则记法。
    domain::Boundary boundary = domain::Boundary::Dead; ///< 边界策略。
    int generation = 0;                                 ///< 当前代数。
    QByteArray windowGeometry;                          ///< 保存的窗口几何。
};

} // namespace lifegame::service
