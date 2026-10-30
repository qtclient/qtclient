#include "tutorialhelper.h"

#include <QJSEngine>
#include <QQmlEngine>

namespace tibia::qmlcomponents {

namespace {

TTutorialHelperQmlService* g_instance = nullptr;

}

TTutorialHelperQmlService::TTutorialHelperQmlService(QObject* parent)
    : cip::tibia::qmlcomponents::TTutorialHelperQmlService(parent)
{
    g_instance = this;
}

TTutorialHelperQmlService* TTutorialHelperQmlService::instance() { return g_instance; }

int TTutorialHelperQmlService::nextUniqueTutorialMarkerID() { return m_nextId++; }

void TTutorialHelperQmlService::setDebugOverlayVisible(const bool Visible)
{
    m_debugOverlay = Visible;
    for (auto it = m_markers.begin(); it != m_markers.end(); ++it) {
        if (cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item = it.value())
            item->setDebugOverlayVisible(Visible);
    }
}

cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* TTutorialHelperQmlService::findTutorialMarkerQuickItemWithID(const int ID)
{
    return m_markers.value(ID);
}

QJSValue TTutorialHelperQmlService::findSpellInfoForSpell(const QString& SpellName)
{
    Q_UNUSED(SpellName);
    return {};
}

void TTutorialHelperQmlService::registerMarker(cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item)
{
    if (!item)
        return;
    const int id = item->markerID();
    if (id != 0)
        m_markers.insert(id, item);
}

void TTutorialHelperQmlService::unregisterMarker(cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item)
{
    if (!item)
        return;
    for (auto it = m_markers.begin(); it != m_markers.end();) {
        if (it.value() == item)
            it = m_markers.erase(it);
        else
            ++it;
    }
}

TTutorialMarkerQuickItem::TTutorialMarkerQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TTutorialMarkerQuickItem(parent)
{
}

TTutorialMarkerQuickItem::~TTutorialMarkerQuickItem()
{
    if (TTutorialHelperQmlService* helper = TTutorialHelperQmlService::instance())
        helper->unregisterMarker(this);
}

void TTutorialMarkerQuickItem::setMarkerID(const int v)
{
    if (TTutorialHelperQmlService* helper = TTutorialHelperQmlService::instance())
        helper->unregisterMarker(this);
    cip::tibia::qmlcomponents::TTutorialMarkerQuickItem::setMarkerID(v);
    if (TTutorialHelperQmlService* helper = TTutorialHelperQmlService::instance())
        helper->registerMarker(this);
}

void TTutorialMarkerQuickItem::setDebugOverlayVisible(const bool v)
{
    cip::tibia::qmlcomponents::TTutorialMarkerQuickItem::setDebugOverlayVisible(v);
}

}
