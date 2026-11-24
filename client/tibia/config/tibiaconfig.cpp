#include "config/tibiaconfig.h"

#include <QVariant>

namespace tibia::config {

namespace {

const QString kUrlsGroup = QStringLiteral("URLS");
const QString kLoginWebService = QStringLiteral("loginWebService");
const QString kClientWebService = QStringLiteral("clientWebService");
const QString kAccountCreationClientServices = QStringLiteral("accountCreationClientServices");
const QString kHintsUrl = QStringLiteral("hintsUrl");

}

qint64 TTibiaConfig::clientSetting(const QString& Group, const QString& Key, const qint64 DefaultValue) const
{
    m_pSettings->beginGroup(Group);
    const qint64 value = m_pSettings->value(Key, DefaultValue).toLongLong();
    m_pSettings->endGroup();
    return value;
}

double TTibiaConfig::clientSettingReal(const QString& Group, const QString& Key, const double DefaultValue) const
{
    m_pSettings->beginGroup(Group);
    const double value = m_pSettings->value(Key, DefaultValue).toDouble();
    m_pSettings->endGroup();
    return value;
}

QString TTibiaConfig::clientSettingString(const QString& Group, const QString& Key, const QString& DefaultValue) const
{
    m_pSettings->beginGroup(Group);
    const QString value = m_pSettings->value(Key, DefaultValue).toString();
    m_pSettings->endGroup();
    return value;
}

bool TTibiaConfig::clientSettingBool(const QString& Group, const QString& Key, const bool DefaultValue) const
{
    m_pSettings->beginGroup(Group);
    const bool value = m_pSettings->value(Key, DefaultValue).toBool();
    m_pSettings->endGroup();
    return value;
}

qint64 TTibiaConfig::clientSettingAtMost(const QString& Group, const QString& Key, const qint64 DefaultValue,
                                         const qint64 Maximum) const
{
    const qint64 value = clientSetting(Group, Key, DefaultValue);
    if (value <= Maximum)
        return value;
    m_pSettings->beginGroup(Group);
    m_pSettings->setValue(Key, Maximum);
    m_pSettings->endGroup();
    return Maximum;
}

qint64 TTibiaConfig::clientSettingAtLeast(const QString& Group, const QString& Key, const qint64 DefaultValue,
                                          const qint64 Minimum) const
{
    const qint64 value = clientSetting(Group, Key, DefaultValue);
    if (value >= Minimum)
        return value;
    m_pSettings->beginGroup(Group);
    m_pSettings->setValue(Key, Minimum);
    m_pSettings->endGroup();
    return Minimum;
}

QUrl TTibiaConfig::readUrl(const QString& Key, const QUrl& DefaultValue) const
{
    m_pSettings->beginGroup(kUrlsGroup);
    const QUrl value = m_pSettings->value(Key, QVariant(DefaultValue)).toUrl();
    m_pSettings->endGroup();
    return value;
}

QUrl TTibiaConfig::loginWebServiceUrl() const
{
    return readUrl(kLoginWebService, QUrl());
}

QUrl TTibiaConfig::clientWebServiceUrl() const
{
    return readUrl(kClientWebService, QUrl());
}

QUrl TTibiaConfig::accountCreationClientServicesUrl() const
{
    return readUrl(kAccountCreationClientServices, QUrl());
}

QUrl TTibiaConfig::hintsUrl() const
{
    return readUrl(kHintsUrl, QUrl());
}

}
