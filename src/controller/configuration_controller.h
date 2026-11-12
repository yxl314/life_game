#pragma once

/**
 * @file configuration_controller.h
 * @brief 规则与边界配置控制器。
 */

#include <QObject>
#include <QString>

#include "domain/boundary.h"

class QActionGroup;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::view
{
class ControlPanel;
class MessageSink;
class RuleCatalog;
}

namespace lifegame::controller
{

/**
 * @brief 来自面板或菜单的规则/边界输入。
 *
 * 生效的规则与边界由模型持有；本类把面板和菜单的意图翻译成模型命令，并在
 * 每次配置变化后同步菜单的勾选状态。
 */
class ConfigurationController : public QObject
{
    Q_OBJECT

public:
    /// @brief 连接面板的规则与边界控件。
    /// @param model 可写模型。
    /// @param panel 规则/边界控件。
    /// @param catalog 规则展示目录。
    /// @param messages 状态栏输出端口。
    /// @param parent QObject 父对象。
    ConfigurationController(model::LifeModel* model,
                            view::ControlPanel* panel,
                            const view::RuleCatalog* catalog,
                            view::MessageSink* messages,
                            QObject* parent = nullptr);

public slots:
    /// @brief 按规则目录下标选择预设规则。
    ///
    /// 自定义（非预设）条目没有记法，直接忽略。
    /// @param index 预设下标。
    void setRuleIndex(int index);
    /// @brief 应用 B/S 记法给出的规则。
    ///
    /// 跳转进行中拒绝切换；记法未变化时不做任何事。
    /// @param notation B/S 记法。
    void setRuleByNotation(const QString& notation);
    /// @brief 应用边界策略。
    /// @param boundary 用户选择的边界策略。
    void setBoundary(domain::Boundary boundary);

    /// @brief 关联需要与模型同步的规则动作组。
    /// @param group 可勾选的规则动作组。
    void setRuleActionGroup(QActionGroup* group);

private:
    /// @brief 勾选数据等于模型记法的那个动作。
    void syncRuleActions();

    model::LifeModel* m_model = nullptr;          ///< 可写模型。
    const view::RuleCatalog* m_catalog = nullptr; ///< 规则展示目录。
    view::MessageSink* m_messages = nullptr;      ///< 状态栏输出端口。
    QActionGroup* m_ruleGroup = nullptr;          ///< 可勾选的规则动作组，可为空。
};

} // namespace lifegame::controller
