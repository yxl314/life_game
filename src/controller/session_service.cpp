#include "controller/session_service.h"

#include "domain/rle.h"
#include "model/life_model.h"
#include "service/session_state.h"
#include "service/settings_store.h"

namespace lifegame::controller
{

SessionService::SessionService(model::LifeModel* model, service::SettingsStore* store)
    : m_model(model), m_store(store)
{
}

SessionRestoreResult SessionService::restore()
{
    SessionRestoreResult result;
    const service::SettingsLoadResult loadedState = m_store->load();
    const service::SessionState state = loadedState.state;
    result.settings = shared::normalized(state.settings);
    result.error = loadedState.error;
    result.detail = loadedState.detail;
    result.windowGeometry = state.windowGeometry;

    (void)m_model->setSnapshotInterval(result.settings.snapshotInterval);
    (void)m_model->setBoundary(state.boundary);
    const model::CommandResult ruleResult =
        m_model->setRuleByNotation(state.ruleNotation.toStdString());
    if (model::failed(ruleResult) && ruleResult != model::CommandResult::NoChange)
    {
        // A corrupt stored rule must not silently change the effective rule.
        (void)m_model->setRuleByNotation("B3/S23");
        if (result.error == service::SettingsError::None)
        {
            result.error = service::SettingsError::CorruptData;
            result.detail = QStringLiteral("invalid stored rule notation");
        }
    }

    if (!state.boardRle.isEmpty())
    {
        const domain::RleDecodeResult decoded = domain::decodeRle(state.boardRle.toStdString());
        if (!decoded.document.has_value())
        {
            result.status = SessionRestoreStatus::DecodeFailed;
        }
        else
        {
            result.modelResult = m_model->loadDocument(*decoded.document);
            if (model::failed(result.modelResult))
            {
                result.status = SessionRestoreStatus::LoadFailed;
            }
            else if (state.generation > 0)
            {
                const model::CommandResult generationResult =
                    m_model->setGeneration(state.generation);
                if (model::succeeded(generationResult))
                {
                    result.status = SessionRestoreStatus::Restored;
                }
                else
                {
                    result.status = SessionRestoreStatus::PartialRestore;
                    result.modelResult = generationResult;
                }
            }
            else
            {
                result.status = SessionRestoreStatus::Restored;
            }
        }
    }

    if (!result.restored())
    {
        const model::CommandResult resizeResult =
            m_model->resizeBoard(domain::kDefaultBoardWidth, domain::kDefaultBoardHeight);
        if (model::failed(resizeResult))
        {
            result.status = SessionRestoreStatus::LoadFailed;
            result.modelResult = resizeResult;
        }
    }
    return result;
}

service::SettingsError SessionService::save(const shared::AppSettings& settings,
                                             const QByteArray& windowGeometry,
                                             QString* detail)
{
    service::SessionState state;
    state.settings = settings;
    state.boardRle = QString::fromStdString(m_model->toRle("life-game"));
    state.ruleNotation = QString::fromStdString(m_model->rule().notation());
    state.boundary = m_model->boundary();
    state.generation = m_model->stats().generation;
    state.windowGeometry = windowGeometry;
    return m_store->save(state, detail);
}

} // namespace lifegame::controller
