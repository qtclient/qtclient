#include "containerslotinformation.h"

#include <client/item.h>
#include <tibia/assets/thingtype.h>

#include <QLocale>
#include <QStringList>

#include <iterator>

namespace tibia::container {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

/// Qt's multi-length string separator: Text shows the first variant that fits.
constexpr QChar LengthVariantSeparator(0x9c);

QString lengthVariants(const QString& longText, const QString& shortText)
{
    return longText + LengthVariantSeparator + shortText;
}

QString chargesShortText(const uint charges)
{
    return charges < 1000 ? QString::number(charges) : QStringLiteral(">999");
}

/// CIP 0x10095e350: minutes below a day.
QString minutesLongText(const qint64 minutes)
{
    if (minutes < 60)
        return qtTrId("time_minutes_short").arg(minutes % 60);
    return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
}

/// CIP 0x100d58a60: an object category's market name.
const char* categoryNameId(const int category)
{
    static constexpr const char* Names[] = {
        "market_category_unknown",        "market_category_armors",       "market_category_amulets",
        "market_category_boots",          "market_category_containers",   "market_category_decoration",
        "market_category_food",           "market_category_helmets",      "market_category_legs",
        "market_category_others",         "market_category_potions",      "market_category_rings",
        "market_category_runes",          "market_category_shields",      "market_category_tools",
        "market_category_valuables",      "market_category_ammo",         "market_category_axes",
        "market_category_clubs",          "market_category_distanceweapons", "market_category_swords",
        "market_category_wands",          "market_category_premiumscrolls", "market_category_tibiacoins",
        "market_category_creature_products", "market_category_quiver",    "market_category_soul_cores",
        "market_category_fist_weapons",   "market_category_unsorted",     "market_category_all_weapons",
        "market_category_gold",           "market_category_default",
    };
    return category >= 0 && category < static_cast<int>(std::size(Names)) ? Names[category] : "market_category_unknown";
}

/// Crystal sends a managed container's categories as bits (1 << category).
QString categoriesText(const char* captionId, const uint32_t flags)
{
    QStringList names;
    for (int category = 0; category < 32; ++category) {
        if (flags & (1u << category))
            names.append(qtTrId(categoryNameId(category)));
    }
    names.sort();
    return qtTrId(captionId) + names.join(QStringLiteral("<br>"));
}

}

QString managedContainerTooltip(const ItemPtr& item)
{
    if (!item)
        return {};
    QString tooltip;
    if (item->getLootFlags())
        tooltip = categoriesText("loot_categories_tooltip_caption", item->getLootFlags());
    if (item->getLootFlags() && item->getObtainFlags())
        tooltip += QStringLiteral("<br><br>");
    if (item->getObtainFlags())
        tooltip += categoriesText("obtain_categories_tooltip_caption", item->getObtainFlags());
    return tooltip;
}

bool hasClock(const ItemPtr& item)
{
    return item && (item->hasClockExpire() || item->hasExpire() || item->hasExpireStop());
}

QString chargesText(const ItemPtr& item, const bool whenUnused)
{
    if (item->isBrandNew() && !whenUnused)
        return {};
    const uint charges = item->getCharges();
    const QString longText = QLocale(QLocale::English).toString(charges);
    const QString shortText = chargesShortText(charges);
    return longText == shortText ? longText : lengthVariants(longText, shortText);
}

QString clockText(const ItemPtr& item, const bool whenUnused)
{
    const qint64 seconds = item->getDurationTime();
    if (seconds < 1 || (item->isBrandNew() && !whenUnused))
        return {};
    return lengthVariants(durationLongText(seconds), durationShortText(seconds));
}

QString ammoText(const uint total)
{
    if (total >= 65000)
        return QStringLiteral("65k+");
    if (total < 10000)
        return QString::number(total);
    return QStringLiteral("%1k").arg(total / 1000);
}

QString durationLongText(const qint64 seconds)
{
    if (seconds > 3600) {
        const qint64 minutes = seconds / 60;
        if (minutes <= 1440)
            return minutesLongText(minutes);
        return qtTrId("time_days_hours_and_minutes_short").arg(minutes / 1440).arg(minutes % 1440 / 60).arg(minutes % 60);
    }
    if (seconds < 60)
        return qtTrId("time_second_short").arg(seconds);
    if (seconds % 60 == 0)
        return qtTrId("time_minutes_short").arg(seconds / 60);
    return qtTrId("time_minutes_and_seconds_short").arg(seconds / 60).arg(seconds % 60);
}

QString durationShortText(const qint64 seconds)
{
    const auto twoDigits = [](const qint64 value) { return QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0')); };
    if (seconds >= 864000)
        return QStringLiteral("%1d").arg(seconds / 86400);
    if (seconds >= 86400) // CIP shows the day's minutes after the days
        return QStringLiteral("%1d").arg(seconds / 86400) + twoDigits(seconds % 86400 / 60);
    if (seconds >= 36000)
        return QStringLiteral("%1h").arg(seconds / 3600);
    if (seconds >= 3600)
        return QStringLiteral("%1h").arg(seconds / 3600) + twoDigits(seconds % 3600 / 60);
    if (seconds >= 600)
        return QStringLiteral("%1m").arg(seconds / 60);
    if (seconds >= 60)
        return QStringLiteral("%1m").arg(seconds / 60) + twoDigits(seconds % 60);
    return QStringLiteral("%1s").arg(seconds);
}

QString slotText(const ItemPtr& item, const bool showExpire, const bool whenUnused)
{
    if (!item)
        return {};
    QString text;
    if (showExpire) {
        if (item->hasWearOut())
            text = chargesText(item, whenUnused);
        if (hasClock(item))
            text = clockText(item, whenUnused);
    }
    if (item->isStackable() && item->getCountOrSubType() > 1)
        text = QString::number(item->getCountOrSubType());
    if (item->hasContainerAmmoTotal())
        text = ammoText(item->getContainerAmmoTotal());
    return text;
}

TContainerSlotQmlInformation::TContainerSlotQmlInformation(QObject* parent)
    : cip::tibia::container::TContainerSlotQmlInformation(parent)
{
    m_acceptsDrop = true;
    m_isUpdatable = true;
}

void TContainerSlotQmlInformation::setBackgroundImage(const QString& image)
{
    if (image == m_backgroundImage)
        return;
    m_backgroundImage = image;
    Q_EMIT backgroundImageChanged();
}

void TContainerSlotQmlInformation::setText(const QString& text)
{
    if (text == m_text)
        return;
    m_text = text;
    Q_EMIT textChanged();
}

void TContainerSlotQmlInformation::assign(const TSlotInfo& info)
{
    const uint previousSlot = m_slotID;
    m_slotID = info.slotId;
    setText(info.text);
    if (previousSlot != m_slotID)
        Q_EMIT slotIDChanged();

    const ItemPtr& item = info.object;
    const uint objectId = item ? item->getId() : 0;
    const bool liquid = item && (item->isFluidContainer() || item->isSplash());
    const uint count = item && item->isStackable() ? item->getCountOrSubType() : 0;
    const auto liquidType = static_cast<Info::ELiquidType>(liquid ? item->getCountOrSubType() : 0);
    const uint tier = item ? item->getTier() : 0;
    const uint decoItem = item ? item->getDecoItemId() : 0;
    if (item != m_object || objectId != m_objectID || count != m_objectCount || liquidType != m_liquidType || tier != m_upgradeTier
        || decoItem != m_decoItemObjectID) {
        m_object = item;
        m_objectID = objectId;
        m_objectCount = count;
        m_liquidType = liquidType;
        m_hookDirection = Info::EHookDirection::NONE;
        m_upgradeTier = tier;
        m_decoItemObjectID = decoItem;
        Q_EMIT appearanceChanged();
    }
    if (info.acceptsDrop != m_acceptsDrop) {
        m_acceptsDrop = info.acceptsDrop;
        Q_EMIT acceptsDropChanged();
    }
    if (info.isUpdatable != m_isUpdatable) {
        m_isUpdatable = info.isUpdatable;
        Q_EMIT isUpdatableChanged();
    }
    if (info.tutorialMarkerId != m_tutorialMarkerID) {
        m_tutorialMarkerID = info.tutorialMarkerId;
        Q_EMIT tutorialMarkerIDChanged();
    }
    if (info.managedContainerTooltip != m_managedContainerTooltip) {
        m_managedContainerTooltip = info.managedContainerTooltip;
        Q_EMIT managedContainerTooltipChanged();
    }
}

}
