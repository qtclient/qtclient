#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include <string>
#include <tuple>
#include <vector>

namespace tibia::highscores {

/// One row of the highscore table (CIP: 0x78-byte entries of
/// THighscoresEntriesModel).
struct THighscoreEntry
{
    uint rank = 0;
    QString character;
    QString loyaltyTitle;
    QString vocation;
    QString world;
    quint16 level = 0;
    bool isOwnCharacter = false;
    qulonglong score = 0;
};

/// CIP: THighscoresEntriesModel (roleNames 0x100e49cb0, data 0x100e49fd0).
class THighscoresEntriesModel final : public cip::tibia::highscores::THighscoresEntriesModel
{
    Q_OBJECT
public:
    enum ERole { RankRole = 0x101, CharacterRole, VocationRole, ScoreRole, WorldRole, LevelRole, LoyaltyTitleRole, IsOwnCharacterRole };

    explicit THighscoresEntriesModel(QList<THighscoreEntry> entries, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<THighscoreEntry> m_entries;
};

/// CIP: THighscoresData (ctor 0x100e4a1b0), the entries of one answer.
class THighscoresData final : public cip::tibia::highscores::THighscoresData
{
    Q_OBJECT
public:
    explicit THighscoresData(QList<THighscoreEntry> entries, QObject* parent = nullptr);

private:
    THighscoresEntriesModel m_model;
};

}

namespace tibia::gamewindow {

/// The world type check boxes of HighscorePage.qml (CIP: the model at
/// controller +0xa0, vtable 0x1026616c0; types from 0x100e4ca30).
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

/// The highscores, HighscorePage.qml (CIP: THighscoresDialogController, ctor
/// 0x100d85690, static metacall 0x100823040). The server answer lives in the
/// controller (CIP: THighscoresStorage, filled by 0x100e4b8c0).
class THighscoresDialogController final : public TDialogController<cip::tibia::gamewindow::THighscoresDialogController>
{
    Q_OBJECT
public:
    using Vocations = std::vector<std::tuple<uint32_t, std::string>>;
    using Categories = std::vector<std::tuple<uint8_t, std::string>>;
    using Entries = std::vector<std::tuple<uint32_t, std::string, std::string, uint8_t, std::string, uint16_t, uint8_t, uint64_t>>;

    struct Answer
    {
        std::vector<std::string> worlds;
        std::string selectedWorld;
        uint8_t worldTypeCount = 0;
        uint8_t battlEye = 0;
        std::string allVocationsName;
        Vocations vocations;
        uint32_t selectedVocation = 0xFFFFFFFF;
        Categories categories;
        uint8_t selectedCategory = 0;
        uint16_t page = 0;
        uint16_t totalPages = 0;
        Entries entries;
        uint8_t selectedIndex = 0xFF;
        bool showLoyaltyTitle = false;
        uint8_t valueType = 1;
        uint32_t timestamp = 0;
    };

    explicit THighscoresDialogController(QObject* parent = nullptr);
    ~THighscoresDialogController() override;

    /// CIP 0x100d86c90 (OPEN_HIGHSCORES_DIALOG): shows the dialog and asks
    /// for the own character's page.
    void openDialog();
    /// CIP 0x100d85d50 (highScoresUpdated).
    void setAnswer(Answer answer);
    /// CIP 0x100d85c90 (highScoresError, error 1: no data).
    void setNoData();
    void clear();

    QStringList availableWorlds() const override;
    QStringList availableBattlEyeTypes() const override;
    QStringList availableVocations() const override;
    QStringList availableCategories() const override;
    int selectedPage() const override { return m_answer.page; }
    int totalPages() const override { return m_answer.totalPages; }
    int selectedIndex() const override;
    bool showLoyaltyTitle() const override { return m_answer.showLoyaltyTitle; }
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

    void setShowLoadingComponent(bool show);
    void setDataAvailable(bool available);
    /// CIP 0x100d85200.
    QString selectedWorldName() const;
    /// CIP 0x100d853b0.
    int selectedBattlEyeValue() const;
    int selectedVocationId() const;
    int selectedCategoryId() const;
    /// CIP 0x100e4a410 / 0x100e4a4b0 / 0x100e4a2e0 through 0x100e4a560.
    void requestPage(int page);
    void requestOwnRank();
    void updateWorldTypes();

    Answer m_answer;
    QList<int> m_selectedWorldTypes;
    THighscoresWorldTypeModel m_worldTypes;
};

}
