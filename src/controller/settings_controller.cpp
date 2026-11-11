#include "controller/settings_controller.h"

#include <stacklog/stacklog.h>
#include "controller/error_text.h"
#include "controller/session_controller.h"

#include <QDialog>
#include <QMainWindow>

#include "model/life_model.h"
#include "view/interfaces.h"
#include "view/settings_dialog.h"

namespace lifegame::controller
{

SettingsController::SettingsController(model::LifeModel* model,
                                       SessionController* session,
                                       QMainWindow* window,
                                       view::MessageSink* messages,
                                       QObject* parent)
    : QObject(parent),
      m_model(model),
      m_session(session),
      m_window(window),
      m_messages(messages)
{
}

void SettingsController::openSettings()
{
    if (m_model->isJumping())
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("跳转进行中，请稍后再修改设置。"), 3000);
        }
        return;
    }

    m_session->refreshSettings();
    view::SettingsDialog dialog(m_window);
    dialog.setValues(m_session->settings());
    if (dialog.exec() != QDialog::Accepted)
    {
        STACKLOG_DEBUG() << "settings dialog cancelled";
        return;
    }

    m_session->applySettings(dialog.values());
    if (m_session->save() == service::SettingsError::None && m_messages != nullptr)
    {
        m_messages->showMessage(tr("设置已保存。"), 3000);
    }
}

} // namespace lifegame::controller
