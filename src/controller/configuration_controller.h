#pragma once

/**
 * @file configuration_controller.h
 * @brief Rule and boundary configuration controller.
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

/// @brief Rule/boundary input from the panel or the menu.
class ConfigurationController : public QObject
{
    Q_OBJECT

public:
    /// @param model Writable model.
    /// @param panel Rule/boundary controls.
    /// @param catalog Rule presentation catalog.
    /// @param messages Status output port.
    /// @param parent QObject parent.
    ConfigurationController(model::LifeModel* model,
                            view::ControlPanel* panel,
                            const view::RuleCatalog* catalog,
                            view::MessageSink* messages,
                            QObject* parent = nullptr);

public slots:
    /// @param index Preset index.
    void setRuleIndex(int index);
    /// @param notation B/S notation.
    void setRuleByNotation(const QString& notation);
    /// @param boundary Boundary policy selected by the user.
    void setBoundary(domain::Boundary boundary);

    /// @param group Checkable rule action group.
    void setRuleActionGroup(QActionGroup* group);

private:
    void syncRuleActions();

    model::LifeModel* m_model = nullptr;
    const view::RuleCatalog* m_catalog = nullptr;
    view::MessageSink* m_messages = nullptr;
    QActionGroup* m_ruleGroup = nullptr;
};

} // namespace lifegame::controller
