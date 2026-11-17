#include "config/actionbaroptions.h"

#include <QJsonValue>

namespace tibia::config {

namespace {

const QString FirstVisibleButtonsKey = QStringLiteral("firstVisibleButtons");

}

TActionBarOptions::TActionBarOptions(QObject* parent)
    : cip::tibia::config::TActionBarOptions(parent)
{
}

QString TActionBarOptions::configurationName() const { return QStringLiteral("actionbars"); }

QJsonObject TActionBarOptions::configurationToJson() const
{
    QJsonObject buttons;
    for (const auto& [actionBarId, index] : m_firstVisibleButtons)
        buttons.insert(QStringLiteral("%1").arg(actionBarId), index);
    QJsonObject json;
    json.insert(FirstVisibleButtonsKey, buttons);
    return json;
}

void TActionBarOptions::configurationFromJson(const QJsonObject& json)
{
    if (!json.value(FirstVisibleButtonsKey).isObject())
        return;
    m_firstVisibleButtons.clear();
    const QJsonObject buttons = json.value(FirstVisibleButtonsKey).toObject();
    for (auto it = buttons.begin(); it != buttons.end(); ++it) {
        bool ok = false;
        const int actionBarId = it.key().toInt(&ok);
        m_firstVisibleButtons[static_cast<uint8_t>(ok ? actionBarId : 0)] = it.value().toInt(0);
    }
}

int TActionBarOptions::firstVisibleButton(const uint8_t actionBarId) const
{
    const auto it = m_firstVisibleButtons.find(actionBarId);
    return it != m_firstVisibleButtons.end() ? it->second : 0;
}

void TActionBarOptions::setFirstVisibleButton(const uint8_t actionBarId, const int index)
{
    int& current = m_firstVisibleButtons[actionBarId];
    if (current == index)
        return;
    current = index;
    Q_EMIT configurationChangedFromUserInput();
}

}
