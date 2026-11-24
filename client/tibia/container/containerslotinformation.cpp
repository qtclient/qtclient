#include "containerslotinformation.h"
#include "config/clientoptions.h"

#include "qmlcomponents/appearanceinstancerenderer.h"
#include "qmlcomponents/texthelper.h"

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

/// CIP : minutes below a day.
QString minutesLongText(const qint64 minutes)
{
    if (minutes < 60)
        return qtTrId("time_minutes_short").arg(minutes % 60);
    return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
}

/// CIP : an object category's market name.
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
    names.sort(Qt::CaseInsensitive);
    return qtTrId(captionId) + names.join(QStringLiteral("<br>"));
}

}

QString managedContainerTooltip(const TInventorySlot& slot)
{
    if (slot.empty())
        return {};
    QString tooltip;
    if (slot.lootFlags)
        tooltip = categoriesText("loot_categories_tooltip_caption", slot.lootFlags);
    if (slot.lootFlags && slot.obtainFlags)
        tooltip += QStringLiteral("<br><br>");
    if (slot.obtainFlags)
        tooltip += categoriesText("obtain_categories_tooltip_caption", slot.obtainFlags);
    return tooltip;
}

bool hasClock(const TInventorySlot& slot)
{
    return !slot.empty() && slot.hasClock;
}

QString chargesText(const TInventorySlot& slot, const bool whenUnused)
{
    if (slot.brandNew && !whenUnused)
        return {};
    const uint charges = slot.charges;
    const QString longText = QLocale(QLocale::English).toString(charges);
    const QString shortText = chargesShortText(charges);
    return longText == shortText ? longText : lengthVariants(longText, shortText);
}

QString clockText(const TInventorySlot& slot, const bool whenUnused)
{
    const qint64 milliseconds = slot.durationRemainingMilliseconds();
    if (milliseconds < 1 || (slot.brandNew && !whenUnused))
        return {};
    const qint64 seconds = milliseconds / 1000;
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
    return qmlcomponents::ultraShortDurationString(seconds);
}

QString slotText(const TInventorySlot& slot, const bool showExpire, const bool whenUnused)
{
    if (slot.empty())
        return {};
    QString text;
    if (showExpire) {
        if (slot.hasWearOut)
            text = chargesText(slot, whenUnused);
        if (hasClock(slot))
            text = clockText(slot, whenUnused);
    }
    if (slot.stackable && slot.count > 1)
        text = QString::number(slot.count);
    if (slot.hasContainerAmmo)
        text = ammoText(slot.containerAmmoTotal);
    return text;
}

TContainerSlotQmlInformation::TContainerSlotQmlInformation(QObject* parent)
    : cip::tibia::container::TContainerSlotQmlInformation(parent)
{
    // Everything starts empty: no slot, no object, no text; drops are not
    // accepted and the slot does not update until the first assign.
}

uint TContainerSlotQmlInformation::objectID() const
{
    return qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getQmlTypeId(m_appearance.appearance);
}

uint TContainerSlotQmlInformation::objectCount() const
{
    return qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getQmlCumulativeCount(m_appearance.appearance);
}

/// CIP: the slot's object type can be wielded in both hands; false for an empty slot.
bool TContainerSlotQmlInformation::hasDualWielding() const
{
    return m_appearance.appearance && m_appearance.dualWield;
}

uint TContainerSlotQmlInformation::upgradeTier() const
{
    return qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getQmlUpgradeTier(m_appearance.appearance);
}

Info::ELiquidType TContainerSlotQmlInformation::liquidType() const
{
    return qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getQmlLiquidType(m_appearance.appearance);
}

Info::EHookDirection TContainerSlotQmlInformation::hookDirection() const
{
    return qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getQmlHookDirection(m_appearance.appearance);
}

uint TContainerSlotQmlInformation::decoItemObjectID() const
{
    return qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getQmlDecoItemObjectID(m_appearance.appearance);
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

    const TInventorySlot& slot = info.appearance;
    const uint objectId = slot.typeId;
    const uint count = slot.stackable ? slot.count : 0;
    const auto liquidType = static_cast<Info::ELiquidType>(slot.fluidOrSplash ? slot.fluid : 0);
    const uint tier = slot.tier;
    const uint decoItem = slot.decoItemId;
    if (slot.typeId != m_appearance.typeId || objectId != m_objectID || count != m_objectCount || liquidType != m_liquidType
        || tier != m_upgradeTier || decoItem != m_decoItemObjectID || slot.count != m_appearance.count
        || slot.fluid != m_appearance.fluid || slot.charges != m_appearance.charges
        || slot.durationEndMs != m_appearance.durationEndMs) {
        m_appearance = slot;
        m_objectID = objectId;
        m_objectCount = count;
        m_liquidType = liquidType;
        m_hookDirection = Info::EHookDirection::NONE;
        m_upgradeTier = tier;
        m_decoItemObjectID = decoItem;
        Q_EMIT appearanceChanged();
    } else {
        m_appearance = slot;
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

TSlotInfo TContainerRenderProvider::inventorySlotInfo(const int slotId, const bool showExpire,
                                                     const bool showExpireWhenUnused) const
{
    TSlotInfo info;
    info.slotId = static_cast<uint>(slotId);
    const std::shared_ptr<TInventoryContainer> inventory = m_containerStorage ? m_containerStorage->inventoryContainer() : nullptr;
    if (inventory) {
        info.appearance = inventory->slotAt(slotId);
        info.tutorialMarkerId = inventory->tutorialMarker(slotId);
    }
    info.text = slotText(info.appearance, showExpire, showExpireWhenUnused);
    info.acceptsDrop = true;
    info.isUpdatable = !info.appearance.empty();
    info.managedContainerTooltip = managedContainerTooltip(info.appearance);
    return info;
}

std::map<uint32_t, TSlotInfo> TContainerRenderProvider::inventorySlotInfos() const
{
    const bool showExpire = m_clientOptions && m_clientOptions->boolOption(config::EClientOption::showExpireInInventory);
    const bool showExpireWhenUnused = m_clientOptions && m_clientOptions->boolOption(config::EClientOption::showExpireWhenUnused);
    std::map<uint32_t, TSlotInfo> infos;
    for (int slotId = 0; slotId < TInventoryContainer::SlotCount; ++slotId)
        infos[static_cast<uint32_t>(slotId)] = inventorySlotInfo(slotId, showExpire, showExpireWhenUnused);
    return infos;
}

}
