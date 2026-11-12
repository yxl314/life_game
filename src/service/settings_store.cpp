#include "service/settings_store.h"

#include <stacklog/stacklog.hpp>

#include <QMetaType>
#include <QSettings>

#include <utility>

namespace lifegame::service
{

SettingsStore::SettingsStore(std::unique_ptr<QSettings> settings)
    : m_settings(std::move(settings))
{
}

SettingsStore::~SettingsStore() = default;

SettingsLoadResult SettingsStore::load() const
{
    SettingsLoadResult result;
    if (!m_settings)
    {
        result.error = SettingsError::AccessError;
        result.detail = QStringLiteral("no QSettings instance");
        return result;
    }

    bool corrupt = false;
    const auto readInt = [this, &corrupt](const char* key, int fallback) {
        bool ok = false;
        const int value = m_settings->value(QString::fromLatin1(key), fallback).toInt(&ok);
        if (!ok)
        {
            corrupt = true;
            return fallback;
        }
        return value;
    };
    const auto readBool = [this, &corrupt](const char* key, bool fallback) {
        bool ok = false;
        // QVariant::toBool() has no error flag; check the stored type instead.
        const QVariant raw = m_settings->value(QString::fromLatin1(key), fallback);
        const bool value = raw.toBool();
        if (raw.metaType().id() == QMetaType::QString)
        {
            const QString text = raw.toString().trimmed().toLower();
            ok = text == QStringLiteral("true") || text == QStringLiteral("false") ||
                 text == QStringLiteral("1") || text == QStringLiteral("0");
        }
        else
        {
            ok = true;
        }
        if (!ok)
        {
            corrupt = true;
            return fallback;
        }
        return value;
    };

    SessionState& state = result.state;
    state.settings.jumpStep = readInt("settings/jump_step", 100);
    state.settings.sizeStep = readInt("settings/size_step", 5);
    state.settings.snapshotInterval = readInt("settings/snapshot_interval", 100);
    state.settings.density = readInt("simulation/density", shared::limits::kDefaultDensity);
    state.settings.speed = readInt("simulation/speed", 10);
    state.settings.gridVisible = readBool("view/grid", true);
    state.settings = shared::normalized(state.settings);

    state.boardRle = m_settings->value(QStringLiteral("board/rle")).toString();
    state.ruleNotation =
        m_settings->value(QStringLiteral("simulation/rule"), QStringLiteral("B3/S23")).toString();

    bool boundaryOk = false;
    const int boundaryValue =
        m_settings->value(QStringLiteral("simulation/boundary"), 0).toInt(&boundaryOk);
    if (!boundaryOk || (boundaryValue != 0 && boundaryValue != 1))
    {
        state.boundary = domain::Boundary::Dead;
        corrupt = true;
    }
    else
    {
        state.boundary =
            boundaryValue == 1 ? domain::Boundary::Wrap : domain::Boundary::Dead;
    }

    bool generationOk = false;
    state.generation = m_settings->value(QStringLiteral("board/generation"), 0).toInt(&generationOk);
    if (!generationOk || state.generation < 0)
    {
        state.generation = 0;
        corrupt = true;
    }
    state.windowGeometry = m_settings->value(QStringLiteral("window/geometry")).toByteArray();

    switch (m_settings->status())
    {
    case QSettings::NoError:
        break;
    case QSettings::AccessError:
        result.error = SettingsError::AccessError;
        result.detail = m_settings->fileName();
        break;
    case QSettings::FormatError:
        result.error = SettingsError::FormatError;
        result.detail = m_settings->fileName();
        break;
    }
    if (result.error == SettingsError::None && corrupt)
    {
        result.error = SettingsError::CorruptData;
        result.detail = m_settings->fileName();
    }
    if (result.error == SettingsError::None)
    {
        STACKLOG_DEBUG() << "settings loaded file=" << m_settings->fileName().toStdString();
    }
    else
    {
        STACKLOG_WARN() << "settings load failed error=" << settingsErrorName(result.error)
                        << " file=" << m_settings->fileName().toStdString();
    }
    return result;
}

SettingsError SettingsStore::save(const SessionState& state, QString* detail)
{
    if (!m_settings)
    {
        if (detail != nullptr)
        {
            *detail = QStringLiteral("no QSettings instance");
        }
        return SettingsError::AccessError;
    }

    m_settings->setValue(QStringLiteral("settings/jump_step"), state.settings.jumpStep);
    m_settings->setValue(QStringLiteral("settings/size_step"), state.settings.sizeStep);
    m_settings->setValue(QStringLiteral("settings/snapshot_interval"), state.settings.snapshotInterval);
    m_settings->setValue(QStringLiteral("simulation/density"), state.settings.density);
    m_settings->setValue(QStringLiteral("simulation/speed"), state.settings.speed);
    m_settings->setValue(QStringLiteral("simulation/rule"), state.ruleNotation);
    m_settings->setValue(QStringLiteral("simulation/boundary"), static_cast<int>(state.boundary));
    m_settings->setValue(QStringLiteral("board/rle"), state.boardRle);
    m_settings->setValue(QStringLiteral("board/generation"), state.generation);
    m_settings->setValue(QStringLiteral("view/grid"), state.settings.gridVisible);
    m_settings->setValue(QStringLiteral("window/geometry"), state.windowGeometry);
    m_settings->sync();

    switch (m_settings->status())
    {
    case QSettings::NoError:
        STACKLOG_DEBUG() << "settings saved file=" << m_settings->fileName().toStdString();
        return SettingsError::None;
    case QSettings::AccessError:
        if (detail != nullptr)
        {
            *detail = m_settings->fileName();
        }
        STACKLOG_WARN() << "settings save failed error=AccessError file="
                        << m_settings->fileName().toStdString();
        return SettingsError::AccessError;
    case QSettings::FormatError:
        if (detail != nullptr)
        {
            *detail = m_settings->fileName();
        }
        STACKLOG_WARN() << "settings save failed error=FormatError file="
                        << m_settings->fileName().toStdString();
        return SettingsError::FormatError;
    }
    STACKLOG_WARN() << "settings save failed error=unknown file=" << m_settings->fileName().toStdString();
    return SettingsError::AccessError;
}

} // namespace lifegame::service
