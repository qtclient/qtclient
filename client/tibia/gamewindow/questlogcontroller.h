#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/questtrackerwidgetcontroller.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>
#include <QSet>
#include <QVariantList>

#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tibia {
namespace mods {
class MissionBackend;
}
}

namespace tibia {
namespace gamewindow {

using questlog::hasCompletedMarker;
using questlog::stripCompletedMarker;

/// Quest lines of the quest log (CIP: TQuestLinesModel, roleNames).
class TQuestLinesModel final : public cip::TQuestLinesModel
{
    Q_OBJECT
public:
    enum ERole { IdRole = Qt::UserRole + 1, NameRole, IsCompletedRole, IsHiddenRole, IsPinnedRole };

    struct Line
    {
        int id = 0;
        QString name;
        bool completed = false;
        bool hidden = false;
        bool pinned = false;
    };

    using cip::TQuestLinesModel::TQuestLinesModel;

    /// CIP : keeps the rows that stay.
    void setLines(QList<Line> lines);
    const QList<Line>& lines() const { return m_lines; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<Line> m_lines;
};

/// CIP: TQuestLinesSortFilterProxyModel (filterAcceptsRow ,
/// lessThan). `sortOrder` is cip_skipped — redeclared here.
class TQuestLinesSortFilterProxyModel final : public cip::TQuestLinesSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortingChanged FINAL)
public:
    explicit TQuestLinesSortFilterProxyModel(TQuestLinesModel* source, QObject* parent = nullptr);

    int questLineIdEnumValue() const override;
    int questLineNameEnumValue() const override;
    QString nameFilter() const override;
    void setNameFilter(const QString& v) override;
    int sortOrder() const;
    void setSortOrder(int order);
    void resetFilter() override;
    void setFilterOptions(bool showCompleted, bool showHidden);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    int m_sortOrder = int(cip::tibia::qmlcomponents::enums::ESortOrder::AtoZ);
    bool m_showCompleted = true;
    bool m_showHidden = false;
};

/// Missions of the selected quest line (CIP: TQuestLinePartsModel, roleNames
/// ).
class TQuestLinePartsModel final : public cip::TQuestLinePartsModel
{
    Q_OBJECT
public:
    enum ERole { IdRole = Qt::UserRole + 1, NameRole, DescriptionRole, IsCompletedRole };

    struct Part
    {
        int id = 0;
        QString name;
        QString description;
        bool completed = false;
    };

    using cip::TQuestLinePartsModel::TQuestLinePartsModel;

    void setParts(QList<Part> parts);
    const QList<Part>& parts() const { return m_parts; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<Part> m_parts;
};

/// CIP: TQuestLinePartsSortFilterProxyModel (lessThan).
class TQuestLinePartsSortFilterProxyModel final : public cip::TQuestLinePartsSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TQuestLinePartsSortFilterProxyModel(TQuestLinePartsModel* source, QObject* parent = nullptr);

    int questPartIdEnumValue() const override;
    int questPartDescriptionEnumValue() const override;

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

/// The quest log, QuestlogDialog.qml (CIP: TQuestLogController). Its state is
/// the character's questlog::TQuestLogStorage.
class TQuestLogController final : public TDialogController<cip::tibia::gamewindow::TQuestLogController>
{
    Q_OBJECT
public:
    explicit TQuestLogController(QObject* parent = nullptr);
    ~TQuestLogController() override;

    /// CIP showDialog(optional<TQuestFlagID>): with the 0x10000 bit, the low 16
    /// bits are a quest flag to select along with its quest line.
    void showDialog(int parameter = 0);
    questlog::TQuestLogStorage* questLogStorage() { return &m_storage; }
    /// The tracked missions, shared with the quest tracker widget.
    TQuestTrackerStorage* tracker() { return &m_tracker; }
    /// Loads the per-character file when the logged-in character changed.
    void clear();
    /// The character's questtracking configuration was loaded: the tracker reads it and the
    /// tracked flags go to the server.
    void onCharacterConfigurationLoaded();

    bool canTrackAdditionalQuestFlags() const override;
    bool isTracked() const override;
    qulonglong completedQuestLinesCount() const override;
    bool showCompleted() const override;
    qulonglong hiddenQuestLinesCount() const override;
    bool showHidden() const override;

    void closeButtonPressed() override;
    void requestQuestLineDetails(int QuestLineID) override;
    void requestQuestLineDetailsFirstInList() override;
    void selectedQuestFlag(int QuestFlagID) override;
    void questTrackerButtonPressed() override;
    void setShowCompleted(bool Show) override;
    void setShowHidden(bool Show) override;

    Q_INVOKABLE void setHidden(int QuestLineId, bool IsHidden);
    Q_INVOKABLE void setPinned(int QuestLineId, bool IsPinned);
    Q_INVOKABLE void setTrackingOfQuestFlag(int QuestFlagId, bool IsTracked);

    /// Product mission for the selected quest line (empty when that line has none).
    Q_PROPERTY(QString missionCta READ missionCta NOTIFY missionChanged)
    Q_PROPERTY(QString missionProgress READ missionProgress NOTIFY missionChanged)
    Q_PROPERTY(QVariantList missionChoices READ missionChoices NOTIFY missionChanged)
    void setMissionBackend(mods::MissionBackend* storage);
    void setBrowseOpener(std::function<void(const QString& filter)> opener);
    Q_INVOKABLE void startMission(const QString& taskEntryId = {});

Q_SIGNALS:
    void missionChanged();

private:
    /// CIP slots.
    void onQuestLinesChanged();
    void onQuestLinePartsChanged(int questLineId);
    void onFilterOptionChanged();
    /// CIP: closing forgets the selection and resets the list's filter and sorting.
    void resetSelection();
    std::set<questlog::TQuestFlagID> trackedQuestFlagIds() const;

    QString missionCta() const;
    QString missionProgress() const;
    QVariantList missionChoices() const;
    QString selectedPartName() const;

    QTimer m_questLinesChangedTimer;
    questlog::TQuestLogStorage m_storage;
    TQuestTrackerStorage m_tracker;
    TQuestLinesModel m_lines;
    TQuestLinesSortFilterProxyModel m_linesProxy;
    TQuestLinePartsModel m_parts;
    TQuestLinePartsSortFilterProxyModel m_partsProxy;
    QPointer<mods::MissionBackend> m_missionBackend;
    std::function<void(const QString&)> m_browseOpener;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
