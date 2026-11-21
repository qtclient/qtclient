#pragma once

#include <memory>

#include "gamewindow/dialogmanager.h"
#include "highscores/highscoresciptypes.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QString>

#include <vector>

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace highscores {

/// CIP: THighscoresEntriesModel (roleNames, data).
class THighscoresEntriesModel final : public cip::tibia::highscores::THighscoresEntriesModel
{
    Q_OBJECT
public:
    enum ERole { RankRole = 0x101, CharacterRole, VocationRole, ScoreRole, WorldRole, LevelRole, LoyaltyTitleRole, IsOwnCharacterRole };

    explicit THighscoresEntriesModel(std::vector<THighscoreEntry> entries, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<THighscoreEntry> m_entries;
};

/// CIP: THighscoresData, the entries of one answer.
class THighscoresData final : public cip::tibia::highscores::THighscoresData
{
    Q_OBJECT
public:
    explicit THighscoresData(std::vector<THighscoreEntry> entries, QObject* parent = nullptr);

private:
    THighscoresEntriesModel m_model;
};

}
}

namespace tibia {
namespace gamewindow {

/// The world type check boxes of HighscorePage.qml.
class THighscoresWorldTypeModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum ERole { NameRole = Qt::UserRole + 1, CheckedRole, WorldTypeRole };

    struct Entry
    {
        int worldType = 0;
        QString name;
        bool checked = false;
    };

    using QAbstractListModel::QAbstractListModel;

    void setEntries(QList<Entry> entries);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<Entry> m_entries;
};

/// The highscores, HighscorePage.qml (CIP: THighscoresDialogController). It
/// shows the session's THighscoresStorage and asks the server for pages.
class THighscoresDialogController final : public TDialogController<cip::tibia::gamewindow::THighscoresDialogController>
{
    Q_OBJECT
public:
    explicit THighscoresDialogController(QObject* parent = nullptr);
    ~THighscoresDialogController() override;

    /// CIP connector: follows the session's storage: closes the dialog and listens to answers.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// OPEN_HIGHSCORES_DIALOG: shows the dialog and asks for the own character's page.
    void openDialog();

    QStringList availableWorlds() const override;
    QStringList availableBattlEyeTypes() const override;
    QStringList availableVocations() const override;
    QStringList availableCategories() const override;
    int selectedPage() const override;
    int totalPages() const override;
    int selectedIndex() const override;
    bool showLoyaltyTitle() const override;
    EValueType valueType() const override;
    QString dataActualisationText() const override;

    void setIndexOfSelectedWorld(int v) override;
    void setIndexOfSelectedBattlEyeType(int v) override;
    void setIndexOfSelectedVocation(int v) override;
    void setIndexOfSelectedCategory(int v) override;

    void showDialog() override;
    void closeDialog() override;
    void onClickedFirstPage() override;
    void onClickedPreviousPage() override;
    void onClickedNextPage() override;
    void onClickedLastPage() override;
    void onClickedShowMyPlaceButton() override;
    void applyFilters() override;
    void requestClose() override;
    void toggleSelectedWorldType(int RawWorldType) override;

private:
    struct BattlEyeType
    {
        int value;
        QString name;
    };
    static QList<BattlEyeType> battlEyeTypes();

    void onHighScoresError();
    void onHighScoresUpdated();
    void setShowLoadingComponent(bool show);
    void setDataAvailable(bool available);
    QString selectedWorldName() const;
    int selectedBattlEyeValue() const;
    int selectedVocationId() const;
    int selectedCategoryId() const;
    /// Asks for `page` with the selected filters.
    void requestPage(int page);
    /// Sends the highscores client message (action 0: a page, 1: the own rank).
    static void sendHighscoresRequest(int action, int page, int category, const QString& world, int vocation,
                                      const std::vector<int>& worldTypes, int battlEye);
    void updateIndexOfSelectedWorld();
    void updateIndexOfSelectedBattlEyeType();
    void updateIndexOfSelectedVocation();
    void updateIndexOfSelectedCategory();
    /// The five world types, checked (and selected) as the storage's answer was filtered.
    void updateWorldTypes();

    QPointer<highscores::THighscoresStorage> m_storage;
    std::vector<int> m_selectedWorldTypes;
    THighscoresWorldTypeModel m_worldTypes;
};

}
}
