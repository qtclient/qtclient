#pragma once

#include "gamewindow/dialogmanager.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QJsonArray>

#include <memory>
#include <unordered_set>

namespace tibia {
namespace game {
class IGameSession;
class TOutfitDialogStorage;
class TSelectOutfitDialogData;
}
}

namespace tibia {
namespace gamewindow {

class TGenericEnterTextDialogController;

/// Outfits, mounts or summons offered by the outfit window (CIP:
/// TCustomiseCharacterModel).
class TCustomiseCharacterModel final : public cip::TCustomiseCharacterModel
{
    Q_OBJECT
public:
    enum ERole {
        OutfitIDRole = Qt::UserRole + 1,
        OutfitNameRole,
        OutfitSourceRole,
        OutfitTypeRole,
        FirstAddOnRole,
        SecondAddOnRole,
        CanBeColorizedRole,
        OfferIDRole,
        OutfitGenderRole,
        OtherGenderOutfitIDRole,
    };

    struct Entry
    {
        int id = 0;
        QString name;
        bool firstAddOn = false;
        bool secondAddOn = false;
        /// TibiaEnums.EOutfitType; the server's mode byte.
        int type = 0;
        bool canBeColorized = false;
        int offerId = 0;
        /// TSelectOutfitDialogController::EOutfitGender.
        int gender = 2;
        int otherGenderId = 0;
    };

    explicit TCustomiseCharacterModel(QObject* parent = nullptr);

    void setEntries(QList<Entry> entries);
    const QList<Entry>& entries() const { return m_entries; }
    /// Owned entry with `id`, or nullptr.
    const Entry* ownedEntry(int id) const;

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<Entry> m_entries;
};

/// An outfit or mount appearance with colour layers.
bool outfitCanBeColorized(int lookType);

/// CIP: TCustomizeCharacterSortProxyModel (filterAcceptsRow).
class TCustomizeCharacterSortProxyModel final : public cip::TCustomizeCharacterSortProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int numberOfOwnedOutfits READ numberOfOwnedOutfits CONSTANT)
public:
    explicit TCustomizeCharacterSortProxyModel(TCustomiseCharacterModel* source, QObject* parent = nullptr);

    bool showOnlyOwnOutfits() const override;
    void setShowOnlyOwnOutfits(bool v) override;
    QString outfitNameFilter() const override;
    void setOutfitNameFilter(const QString& v) override;
    int numberOfOwnedOutfits() const override;
    void resetFilter() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

/// The character's saved outfit presets (CIP: TOutfitPresetsModel).
class TOutfitPresetsModel final : public cip::TOutfitPresetsModel
{
    Q_OBJECT
public:
    enum ERole { JsonDataRole = Qt::UserRole + 1, EverythingOwnedRole };

    explicit TOutfitPresetsModel(QObject* parent = nullptr);

    void setPresets(QList<QJsonObject> presets, QList<bool> owned);

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<QJsonObject> m_presets;
    QList<bool> m_owned;
};

/// The outfit window, outfitdialog/OutfitDialog.qml (CIP:
/// TSelectOutfitDialogController). Shows the session's latest select outfit
/// dialog data whenever the player handler asks for the dialog; the mode of
/// that data decides what the dialog edits and what a confirm sends.
class TSelectOutfitDialogController final : public TDialogController<cip::tibia::gamewindow::TSelectOutfitDialogController>
{
    Q_OBJECT
public:
    explicit TSelectOutfitDialogController(QObject* parent = nullptr);
    ~TSelectOutfitDialogController() override;

    /// Follows the session's player handler and character configuration: closes
    /// the rename dialog, opens the dialog on showSelectOutfitDialog and reloads
    /// the presets when either preset list changes.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Shows the outfit dialog storage's current data: the outfit, mount and
    /// summon lists (with the owned ids), the random mount flag, then creates
    /// and shows the dialog and loads the presets.
    void onShowSelectOutfitDialog();

    void onCancelClicked() override;
    void onClickedOpenStoreOffer(int offerID) override;
    void onControllerFinished() override;
    void onCharacterPresetsChanged() override;

    void setCharacterConfiguration(cip::tibia::qmlcomponents::TOutfitPresetQmlType* pOutfitPreset, int LookDirectionRaw, bool ShowSocket,
                                   bool MountCharacter) override;
    void outfitPresetsSaveIndex(const QJsonObject& JsonObject, int Index) override;
    void outfitPresetsDeleteIndex(int Index) override;
    void outfitPresetsRenameIndex(int Index) override;
    void copyToClipboard(QObject* pData) override;
    QObject* createFromClipboard() override;
    void onPresetNameEntered(const QString& PlayerName);
    EDialogMode dialogMode() const override;
    bool showOutfit() const override;
    bool showSocket() const override;
    cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::ECreatureLookDirection direction() const override;
    bool isMounted() const override;
    cip::tibia::qmlcomponents::TOutfitPresetQmlType* selectedOutfitPreset() const override;
    QVariantList hsvColors() const override;
    QVariantMap initialDialogProperties() const override;

private:
    /// The presets the dialog mode edits, null when it edits none.
    QJsonArray* presetList();
    const QJsonArray* presetList() const;
    /// The character configuration storage announces the edited presets.
    void savePresets();
    /// Clears the add-ons of each offered outfit its appearance cannot show.
    void removeUnsupportedAddOns();

    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    int m_renameIndex = -1;
    /// The owned outfits (and their other-gender twins), mounts and summons.
    std::unordered_set<uint16_t> m_ownedIds;
    TCustomiseCharacterModel m_outfits;
    TCustomiseCharacterModel m_mounts;
    TCustomiseCharacterModel m_summons;
    TCustomizeCharacterSortProxyModel m_outfitsProxy;
    TCustomizeCharacterSortProxyModel m_mountsProxy;
    TCustomizeCharacterSortProxyModel m_summonsProxy;
    TOutfitPresetsModel m_presets;
    std::shared_ptr<game::TSelectOutfitDialogData> m_dialogData;
    std::shared_ptr<game::TOutfitDialogStorage> m_outfitDialogStorage;
    TGenericEnterTextDialogController* m_renameDialog;
};

}
}
