#pragma once

#include "cyclopedia/creaturetrackerstorage.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>

#include <vector>

namespace tibia::bossbestiary {
class TBossTrackerStorage;
}

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {

/// Rows for CreatureTrackerWidget.qml (CIP: TCreatureTrackerModel).
class TCreatureTrackerListModel final : public cip::TCreatureTrackerModel
{
    Q_OBJECT
public:
    enum Role {
        RaceIdRole = Qt::UserRole + 1,
        NameRole,
        CreatureHasCooldownRole,
        CooldownIsExpiredRole,
        CooldownEndTextRole,
        CurrentKillsRole,
        KillsForDetails1Role,
        KillsForDetails2Role,
        KillsForDetails3Role,
        IsFullyUnlockedRole
    };

    struct Row {
        cyclopedia::TCreatureTrackerEntry entry;
        bool creatureHasCooldown = false;
        bool cooldownIsExpired = false;
        QString cooldownEndText;
    };

    explicit TCreatureTrackerListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(std::vector<Row> rows);

private:
    std::vector<Row> m_rows;
};

/// CreatureTrackerWidget.qml — one instance per EWidgetMode (Bestiary / Bosstiary).
class TCreatureTrackerWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TCreatureTrackerWidgetController>
{
    Q_OBJECT
public:
    using EWidgetMode = cip::tibia::gamewindow::TCreatureTrackerWidgetController::EWidgetMode;

    TCreatureTrackerWidgetController(EWidgetMode mode, cyclopedia::TCreatureTrackerStorage* creatures,
                                     bossbestiary::TBossTrackerStorage* bosses, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                     QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

public Q_SLOTS:
    void saveWidgetOptions() override;
    void requestOpenBestiary() override;
    void requestContextMenu(uint RaceID, const QString& RaceName) override;
    void requestWidgetConfigurationMenu() override;
    void requestOpenBestiaryEntry(uint RaceID) override;

private:
    void rebuild();
    void persistOptions();

    EWidgetMode m_mode;
    QPointer<cyclopedia::TCreatureTrackerStorage> m_creatures;
    QPointer<bossbestiary::TBossTrackerStorage> m_bosses;
    TCreatureTrackerListModel* m_model = nullptr;
    QPointer<config::TSidebarOptions> m_options;
};

class TCreatureTrackerWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TCreatureTrackerWidgetManager(TCreatureTrackerWidgetController::EWidgetMode mode, cyclopedia::TCreatureTrackerStorage* creatures,
                                  bossbestiary::TBossTrackerStorage* bosses, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                  QObject* parent = nullptr);
    ~TCreatureTrackerWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    TCreatureTrackerWidgetController::EWidgetMode m_mode;
    QPointer<cyclopedia::TCreatureTrackerStorage> m_creatures;
    QPointer<bossbestiary::TBossTrackerStorage> m_bosses;
    QQuickItem* m_gameWindowItem = nullptr;
    QQmlEngine* m_engine = nullptr;
    TCreatureTrackerWidgetController* m_controller = nullptr;
};

} // namespace tibia::gamewindow
