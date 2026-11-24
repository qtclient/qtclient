#pragma once

#include <cip_gen.h>

#include <QSGNode>

#include <vector>

namespace tibia {
namespace qmlcomponents {

/// Every live tutorial marker item, the shared debug-overlay flag and the
/// marker id counter (process-wide, like CipSoft's static).
class TTutorialMarkerRegistry
{
public:
    static TTutorialMarkerRegistry& instance();

    /// Ids start at 1.
    int nextUniqueMarkerID();
    void registerMarker(cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item);
    void unregisterMarker(cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* item);
    /// The first registered marker with `markerID`, else null.
    cip::tibia::qmlcomponents::TTutorialMarkerQuickItem* find(int markerID) const;
    /// Pushes a changed flag to every registered marker.
    void setDebugOverlayVisible(bool visible);
    static bool debugOverlayVisible();
    /// Every registered marker, in registration order.
    const std::vector<cip::tibia::qmlcomponents::TTutorialMarkerQuickItem*>& markers() const { return m_markers; }

private:
    std::vector<cip::tibia::qmlcomponents::TTutorialMarkerQuickItem*> m_markers;
    static inline int s_nextMarkerID = 1;
    static inline bool s_debugOverlayVisible = false;
};

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
};

/// Registers itself with TTutorialMarkerRegistry for its lifetime. It paints
/// nothing: the tutorial animations find it and draw at its place.
/// Not `final`: QML registration subclasses via QQmlElement.
class TTutorialMarkerQuickItem : public cip::tibia::qmlcomponents::TTutorialMarkerQuickItem
{
    Q_OBJECT
public:
    explicit TTutorialMarkerQuickItem(QQuickItem* parent = nullptr);
    ~TTutorialMarkerQuickItem() override;

    void setMarkerID(int v) override;
    void setDebugOverlayVisible(bool v) override;

protected:
    void componentComplete() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
};

}
}
