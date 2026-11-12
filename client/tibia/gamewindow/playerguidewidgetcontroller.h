#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

/// PlayerGuideWidget.qml CipSoft shell (CIP: TPlayerGuideWidgetController).
/// Newhaven / tutorial player-guide entries are not published on CipSoft today —
/// the widget stays an empty entries pane. The manager still listens for
/// onUnlockedSpells so CipSoft can later surface level-up auto-spell rows.
class TPlayerGuideWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TPlayerGuideWidgetController>
{
    Q_OBJECT
public:
    TPlayerGuideWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    void onUnlockedSpells(const std::vector<uint16_t>& spellIds);

    /// CIP moc property getters (live members; empty shell keeps entries off).
    bool isShowLevelUp() const override;
    bool isShowAutoSpellList() const override;
    bool isShowManualSpellList() const override;
    int playerLevelToShow() const override;
    QObject* autoSpellList() const override;
    QObject* manualSpellList() const override;
    bool isShowPlayerGuideEntries() const override;
    bool hasLatestUnlockedPlayerGuideEntry() const override;
    QString latestUnlockedPlayerGuideEntry() const override;
    bool hasPreviousUnlockedPlayerGuideEntries() const override;
    QObject* previousUnlockedPlayerGuideEntries() const override;
    bool isShowLevelSpecialPopup() const override;
    QString levelSpecialPopupName() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    void saveWidgetOptions() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

public:
    Q_INVOKABLE void showLatestUnlockedPlayerGuide() override;
    Q_INVOKABLE void showPreviousUnlockedPlayerGuide(int newsEntryId) override;
    Q_INVOKABLE void showLevelSpecialPopup() override;

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — tip NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP sidebar chrome (drag/move) — tip redeclare; forwarded from chrome slots.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); tip name parity.
    void publishGameAction();
    /// CIP class NOTIFY — tip already emits.
    void playerGuideEntriesChanged();
    /// CIP empty-shell NOTIFYs — tip redeclare; Newhaven catalogue absent so no live emit yet.
    void autoSpellListChanged();
    void manualSpellListChanged();
    void showAutoSpellListChanged();
    void showManualSpellListChanged();
    void showLevelUpChanged();
    void playerLevelToShowChanged();
    void levelSpecialPopupLevelChanged();

private Q_SLOTS:
    /// CIP private slots — options persist + news-driven entry refresh hook.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();
    void onNewsStorageContentsChanged();

private:
    QPointer<config::TSidebarOptions> m_options;
};

class TPlayerGuideWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TPlayerGuideWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TPlayerGuideWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TPlayerGuideWidgetController> m_controller;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
