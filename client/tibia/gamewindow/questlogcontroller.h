#pragma once

#include "gamewindow/dialogmanager.h"
#include "gamewindow/questtrackerwidgetcontroller.h"

#include <cip_gen.h>

#include <QPointer>
#include <QSet>
#include <QVariantList>

#include <functional>
#include <string>
#include <tuple>
#include <vector>

namespace tibia::mods {
class MissionBackend;
}

namespace tibia::gamewindow {

/// Whether a mission name carries the server's "(completed)" marker (CIP 0x100f701a0).
bool hasCompletedMarker(const QString& name);
/// The mission name without the "(completed)" marker (CIP 0x100f70250).
QString stripCompletedMarker(const QString& name);

/// Quest lines of the quest log (CIP: TQuestLinesModel, roleNames 0x100c22890).
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

    /// CIP 0x100c22e00: keeps the rows that stay.
    void setLines(QList<Line> lines);
    const QList<Line>& lines() const { return m_lines; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<Line> m_lines;
};

/// CIP: TQuestLinesSortFilterProxyModel (filterAcceptsRow 0x100c22060,
/// lessThan 0x100c22280).
class TQuestLinesSortFilterProxyModel final : public cip::TQuestLinesSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortingChanged FINAL)
public:
    explicit TQuestLinesSortFilterProxyModel(TQuestLinesModel* source, QObject* parent = nullptr);

    int sortOrder() const { return m_sortOrder; }
    void setSortOrder(int order);
    void setNameFilter(const QString& v) override;
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
/// 0x100c23800).
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

/// CIP: TQuestLinePartsSortFilterProxyModel (lessThan 0x100c23380).
class TQuestLinePartsSortFilterProxyModel final : public cip::TQuestLinePartsSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TQuestLinePartsSortFilterProxyModel(TQuestLinePartsModel* source, QObject* parent = nullptr);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

/// The quest log, QuestlogDialog.qml (CIP: TQuestLogController, metacall
/// 0x10083a1b0). Hidden and pinned lines are kept per character.
class TQuestLogController final : public TDialogController<cip::tibia::gamewindow::TQuestLogController>
{
    Q_OBJECT
public:
    explicit TQuestLogController(QObject* parent = nullptr);
    ~TQuestLogController() override;

    /// CIP 0x100c17b20: requests the quest lines and shows the dialog. With
    /// the 0x10000 bit, the low 16 bits are a quest flag to select along with
    /// its quest line.
    void showDialog(int parameter = 0);
    /// The server's quest lines (CIP 0x100c15f90 through the storage).
    void setQuestLines(const std::vector<std::tuple<uint16_t, std::string, bool>>& lines);
    /// The server's missions of one quest line (CIP 0x100c16ed0).
    void setQuestLineParts(uint16_t questLineId, const std::vector<std::tuple<std::string, std::string, uint16_t>>& missions);
    /// The tracked missions, shared with the quest tracker widget.
    TQuestTrackerStorage* tracker() { return &m_tracker; }
    /// Loads the per-character file when the logged-in character changed.
    void ensureStorage();
    void clear();

    void closeButtonPressed() override;
    void requestQuestLineDetails(int QuestLineID) override;
    void requestQuestLineDetailsFirstInList() override;
    void selectedQuestFlag(int QuestFlagID) override;
    void questTrackerButtonPressed() override;
    void setShowCompleted(bool Show) override;
    void setShowHidden(bool Show) override;

    /// CIP 0x100c18ad0.
    Q_INVOKABLE void setHidden(int QuestLineId, bool IsHidden);
    /// CIP 0x100c18e00.
    Q_INVOKABLE void setPinned(int QuestLineId, bool IsPinned);
    /// CIP 0x100c18ea0.
    Q_INVOKABLE void setTrackingOfQuestFlag(int QuestFlagId, bool IsTracked);

    /// Product mission for the selected quest line (empty when that line has none).
    Q_PROPERTY(QString missionCta READ missionCta NOTIFY missionChanged)
    Q_PROPERTY(QString missionProgress READ missionProgress NOTIFY missionChanged)
    Q_PROPERTY(QVariantList missionChoices READ missionChoices NOTIFY missionChanged)
    void setExpeditions(mods::MissionBackend* storage);
    void setBrowseOpener(std::function<void(const QString& filter)> opener);
    Q_INVOKABLE void startMission(const QString& taskEntryId = {});

Q_SIGNALS:
    void missionChanged();

private:
    QString missionCta() const;
    QString missionProgress() const;
    QVariantList missionChoices() const;
    QString selectedPartName() const;
    void rebuildLines();
    void updateTracking();
    void loadStorage();
    void saveStorage();

    TQuestLinesModel m_lines;
    TQuestLinesSortFilterProxyModel m_linesProxy;
    TQuestLinePartsModel m_parts;
    TQuestLinePartsSortFilterProxyModel m_partsProxy;
    std::vector<std::tuple<uint16_t, std::string, bool>> m_serverLines;
    QSet<int> m_hiddenLines;
    QSet<int> m_pinnedLines;
    TQuestTrackerStorage m_tracker;
    QPointer<mods::MissionBackend> m_expeditions;
    std::function<void(const QString&)> m_browseOpener;
    quint32 m_characterId = 0;
};

}
