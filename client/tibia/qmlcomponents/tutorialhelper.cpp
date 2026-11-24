#include "tutorialhelper.h"

#include "spells/spellinfostorage.h"

#include <algorithm>

#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>

namespace tibia::qmlcomponents {

namespace {

TTutorialHelperQmlService* g_instance = nullptr;

}

TTutorialMarkerRegistry& TTutorialMarkerRegistry::instance()
{
    static TTutorialMarkerRegistry registry;
    return registry;
}

int TTutorialMarkerRegistry::nextUniqueMarkerID() { return s_nextMarkerID++; }

void TTutorialMarkerRegistry::registerMarker(cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item) { m_markers.push_back(item); }

void TTutorialMarkerRegistry::unregisterMarker(cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item)
{
    m_markers.erase(std::remove(m_markers.begin(), m_markers.end(), item), m_markers.end());
}

cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* TTutorialMarkerRegistry::find(const int markerID) const
{
    for (cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item : m_markers) {
        if (item->markerID() == markerID)
            return item;
    }
    return nullptr;
}

void TTutorialMarkerRegistry::setDebugOverlayVisible(const bool visible)
{
    if (s_debugOverlayVisible == visible)
        return;
    s_debugOverlayVisible = visible;
    for (cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item : m_markers)
        item->setDebugOverlayVisible(s_debugOverlayVisible);
}

bool TTutorialMarkerRegistry::debugOverlayVisible() { return s_debugOverlayVisible; }

TTutorialHelperQmlService::TTutorialHelperQmlService(QObject* parent)
    : cip::tibia::qmlcomponents::TTutorialHelperQmlService(parent)
{
    g_instance = this;
}

TTutorialHelperQmlService* TTutorialHelperQmlService::instance() { return g_instance; }

int TTutorialHelperQmlService::nextUniqueTutorialMarkerID() { return TTutorialMarkerRegistry::instance().nextUniqueMarkerID(); }

void TTutorialHelperQmlService::setDebugOverlayVisible(const bool Visible)
{
    TTutorialMarkerRegistry::instance().setDebugOverlayVisible(Visible);
}

cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* TTutorialHelperQmlService::findTutorialMarkerQuickItemWithID(const int ID)
{
    return TTutorialMarkerRegistry::instance().find(ID);
}

/// The first spell of the table whose name matches case-insensitively, as
/// {name, icon, cooldown (ms), formula}; undefined for none.
QJSValue TTutorialHelperQmlService::findSpellInfoForSpell(const QString& SpellName)
{
    const QString wanted = SpellName.toLower();
    for (const auto& [id, spell] : spells::TSpellInfoStorage::instance()->spells()) {
        Q_UNUSED(id);
        if (spell.name.toLower() != wanted)
            continue;
        QJsonObject info;
        info[QStringLiteral("name")] = spell.name;
        info[QStringLiteral("icon")] = static_cast<int>(static_cast<uint16_t>(spell.iconIndex));
        info[QStringLiteral("cooldown")] = static_cast<int>(spell.cast.cooldownSelf);
        info[QStringLiteral("formula")] = spell.words;
        const QVariant variant = QJsonDocument(info).toVariant();
        if (QQmlEngine* engine = qmlEngine(this))
            return engine->toScriptValue(variant);
        break;
    }
    return QJSValue(QJSValue::UndefinedValue);
}

TTutorialMarkerQuickItem::TTutorialMarkerQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TTutorialMarkerQuickItem(parent)
{
    TTutorialMarkerRegistry::instance().registerMarker(this);
}

TTutorialMarkerQuickItem::~TTutorialMarkerQuickItem() { TTutorialMarkerRegistry::instance().unregisterMarker(this); }

void TTutorialMarkerQuickItem::setMarkerID(const int v)
{
    cip::tibia::qmlcomponents::TTutorialMarkerQuickItem::setMarkerID(v);
}

void TTutorialMarkerQuickItem::setDebugOverlayVisible(const bool v)
{
    if (debugOverlayVisible() == v)
        return;
    // The item has contents only while the debug overlay shows (it paints none).
    setFlag(ItemHasContents, v);
    cip::tibia::qmlcomponents::TTutorialMarkerQuickItem::setDebugOverlayVisible(v);
}

void TTutorialMarkerQuickItem::componentComplete()
{
    cip::tibia::qmlcomponents::TTutorialMarkerQuickItem::componentComplete();
}

QSGNode* TTutorialMarkerQuickItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    return oldNode;
}

}
