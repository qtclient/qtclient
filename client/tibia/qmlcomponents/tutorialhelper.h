#pragma once

#include <cip_gen.h>

#include <QHash>
#include <QPointer>

namespace tibia::qmlcomponents {

/// QML singleton TutorialHelper (CIP: TTutorialHelperQmlService).
/// Not `final`: QML registration subclasses via QQmlElement.
class TTutorialHelperQmlService : public cip::tibia::qmlcomponents::TTutorialHelperQmlService
{
    Q_OBJECT
public:
    explicit TTutorialHelperQmlService(QObject* parent = nullptr);

    static TTutorialHelperQmlService* instance();

    Q_INVOKABLE int nextUniqueTutorialMarkerID() override;
    Q_INVOKABLE void setDebugOverlayVisible(bool Visible) override;
    Q_INVOKABLE cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* findTutorialMarkerQuickItemWithID(int ID) override;
    Q_INVOKABLE QJSValue findSpellInfoForSpell(const QString& SpellName) override;

    void registerMarker(cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item);
    void unregisterMarker(cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item);

private:
    int m_nextId = 1;
    bool m_debugOverlay = false;
    QHash<int, QPointer<cip::tibia::qmlcomponents::TTutorialMarkerQuickItem>> m_markers;
};

/// Registers / unregisters with TutorialHelper when markerID is set.
/// Not `final`: QML registration subclasses via QQmlElement.
class TTutorialMarkerQuickItem : public cip::tibia::qmlcomponents::TTutorialMarkerQuickItem
{
    Q_OBJECT
public:
    explicit TTutorialMarkerQuickItem(QQuickItem* parent = nullptr);
    ~TTutorialMarkerQuickItem() override;

    void setMarkerID(int v) override;
    void setDebugOverlayVisible(bool v) override;
};

}
