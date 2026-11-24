#pragma once

#include "gamewindow/messagedialogcontroller.h"
#include "qtconnectionmanager.h"
#include "skillwheel/gematelierreshapemoddialogcontroller.h"
#include "skillwheel/skillwheelmodels.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <memory>
#include <optional>

class QQmlEngine;
class QQuickItem;

namespace tibia::game {
class IGameSession;
class TPremiumTimeStorage;
}

namespace tibia {
namespace skillwheel {

/// One slice of SkillWheel.qml's skillParameters (CIP).
class TSkillWheelTileQMLInfo final : public cip::TSkillWheelTileQMLInfo
{
    Q_OBJECT
public:
    TSkillWheelTileQMLInfo(int slot, double fill, bool unlocked, int mediumPerk, int smallPerk, const QUrl& image, QObject* parent);

    /// CIP moc property getters.
    cip::tibia::qmlcomponents::enums::ESkillGridTile id() const override;
    double fillPercent() const override;
    bool unlocked() const override;
    int mediumPerkId() const override;
    int smallPerkId() const override;
    QUrl largeImage() const override;
};

/// One corner of SkillWheel.qml's cornerParameters (CIP).
class TSkillWheelCornerQMLInfo final : public cip::TSkillWheelCornerQMLInfo
{
    Q_OBJECT
public:
    TSkillWheelCornerQMLInfo(int domain, double fill, int level, int largePerk, QObject* parent);

    /// CIP moc property getters.
    cip::tibia::qmlcomponents::enums::EPosition id() const override;
    double fillPercent() const override;
    int level() const override;
    int largePerkId() const override;
};

/// A vessel of the gem atelier (CIP): the vessel index, the configuration's vocation index and the
/// gem socketed in the vessel, if any.
class TQmlSocketedGem final : public cip::tibia::skillwheel::TQmlSocketedGem
{
    Q_OBJECT
public:
    /// Throws std::invalid_argument for a vessel index outside 0..7.
    TQmlSocketedGem(int vesselIndex, int vocationIndex, const std::optional<TGem>& gem, QObject* parent);

    /// The socketed gem's id, -1 for an empty vessel.
    int gemId() const override;
    /// The socketed gem's mod count; an empty vessel's quality decides it (lesser 1, regular 2, greater 3).
    int modSlotCount() const override;
    int vesselQuality() const override;
    /// The bezel of the socketed gem's vocation and mod count, or the empty bezel of the mod count.
    QUrl socketBezelSource() const override;

private:
    int m_vesselIndex = 0;
    int m_vocationIndex = 0;
    std::optional<TGem> m_gem;
};

/// SkillWheelPresetManagmentDialog.qml (CIP: TSkillWheelPresetManagmentDialogController), owned by the
/// wheel page: opened for the current preset in a mode, it renames the preset, adds an empty preset,
/// copies the preset or imports a wheel code into the session's presets.
class TSkillWheelPresetManagmentDialogController final
    : public gamewindow::TDialogController<cip::tibia::gamewindow::TSkillWheelPresetManagmentDialogController>
{
    Q_OBJECT
public:
    TSkillWheelPresetManagmentDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
    ~TSkillWheelPresetManagmentDialogController() override;

    /// CIP: the game session the presets belong to (null without one); the presets are its skill
    /// wheel storage's.
    void setGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// The dialog opens again for `presetName` in `mode`: the new name starts as the trimmed preset
    /// name, the import string empty.
    void open(const QString& presetName, EManageMode mode);

    int maxNameLength() const override { return 29; }

    void onOkClicked(EManageMode ManageMod) override;
    void onCloseClicked() override;
    void setNewName(const QString& NewName) override;
    void setImportString(const QString& ImportString) override;

private:
    /// Mode, names, import string and error go back to their initial values.
    void clear();
    /// A name that is not empty and no preset of the session has.
    bool isValidName(const QString& name) const;
    /// Empty for a wheel code of the player's vocation with a valid grid.
    QString importError(const QString& importString) const;
    void renamePreset();
    void createEmptyPreset();
    void copyPreset();
    void importPreset();

    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The session's skill wheel storage (null without a session).
    TSkillWheelStorage* m_storage = nullptr;
    /// CIP: the info that an imported code is of an older format.
    gamewindow::TMessageDialogController* m_message;
    QString m_newName;
    QString m_importString;
};

/// The Wheel tab, SkillWheelPage.qml (CIP: TSkillWheelPageController).
class TSkillWheelPageController final : public cip::tibia::gamewindow::TSkillWheelPageController
{
    Q_OBJECT
public:
    /// CIP: `plannerUrl` is the planner page the export code is appended to (the client's
    /// wheelOfDestinyPlannerDirectUrl setting); the message boxes and the preset management dialog
    /// are made with `engine` in `dialogParentItem`.
    TSkillWheelPageController(const QString& plannerUrl, QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
    ~TSkillWheelPageController() override;

    /// CIP: the page follows the session's skill wheel storage (its configuration manager's); the
    /// preset management dialog gets the session; the page is refreshed.
    void connectToGameSession(std::shared_ptr<game::IGameSession> GameSession);

    int totalSkillPoints() const override { return m_storage->totalPoints(); }
    int regularSkillPoints() const override { return m_storage->regularPoints(); }
    int specialSkillPoints() const override { return m_storage->extraPoints(); }
    int modGradesSkillPoints() const override { return m_storage->modGradesPoints(); }
    int totalModGradesSkillPoints() const override;
    int spentSkillPoints() const override;
    int huntingTaskSkillPoints() const override { return m_storage->huntingTaskPoints(); }
    QString bonusPointItemNames() const override;
    bool hasShrinesOfInsightBonusPoints() const override { return m_storage->hasShrinesOfInsightPoints(); }
    cip::tibia::qmlcomponents::enums::EVocation vocation() const override;
    bool isWheelDirty() const override { return m_storage->isCurrentSkillWheelChanged(); }
    bool canBeDecreased() const override { return m_storage && m_storage->changeState() == 1; }
    bool canBeIncreased() const override { return m_storage->changeState() == 1 || m_storage->changeState() == 2; }
    bool isLocalPlayer() const override { return m_storage && m_storage->isOwnCharacter(); }
    /// A wheel without any points (the preview of the own vocation).
    bool isPreview() const override { return totalSkillPoints() == 0; }
    QString playerName() const override { return m_storage->playerName(); }
    bool canManagePresets() const override;
    bool isPresetDirty() const override;
    QString currentPresetName() const override;
    bool morePresetsPossible() const override { return m_storage->presets().size() < MaxPresets; }
    bool deletePresetsPossible() const override { return m_storage->presets().size() > 1; }

    void showDialogTab() override;
    void hideDialogTab() override;
    void requestClose() override;
    void requestSaveCurrentPreset() override;
    void onGridTileRightClicked(int GridTileQML, int KeyboardModifiers) override;
    void fillSkill(int GridTileQML) override;
    void addToSkill(int GridTileQML, int KeyboardModifiers) override;
    void clearSkill(int GridTileQML) override;
    void removeFromSkill(int GridTileQML, int KeyboardModifiers) override;
    void onGridTileSelected(int GridTileQML) override;
    void onGridTileHovered(int GridTileQML) override;
    void onLargePerkSelected(int QuarterQML) override;
    void onLargePerkHovered(int QuarterQML) override;
    void resetSkillGrid() override;
    void requestCopyExportCodeToClipboard() override;
    void selectPresetByName(const QString& PresetName, bool IgnoreChanges) override;
    void requestDeletePresetClicked() override;
    void requestRenamePresetClicked() override;
    void requestNewPresetClicked() override;
    void requestImportPresetClicked() override;
    void requestExportPresetClicked() override;
    void setShowFullSummary(bool Show) override;
    bool rendererSupportsGraphicEffects() const override;
    void useServerSkillWheelAsCurrent() override;
    void clearSelection() override;
    void onNewPresetName(const QString& PresetName) override;
    void clearData() override;
    void requestCopyExportCodeOrUrlToClipboardClicked(cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier,
                                                      bool CheckBoxIsChecked) override;
    void onMessageDialogAcceptClicked();
    void onMessageDialogDiscardClicked();
    void savePresetChangesThanSwapPreset();
    void discardPresetChangesThanSwapPreset();

    /// CIP: saves the preset and sends the wheel when it changed.
    void apply();
    /// The page's message boxes and the preset management dialog close.
    void closeMessages();
    /// The next showDialogTab keeps the presets as they are (the dialog switches back to the wheel).
    void skipNextShowDialogTabUpdate() { m_skipNextShowUpdate = true; }

Q_SIGNALS:
    /// CIP: the fragment workshop tab, at a mod when both are given.
    void requestShowFragmentWorkshop(std::optional<int> ModId, std::optional<bool> IsBasicMod);

private:
    /// What the page's message box asks (CIP: the box's mode).
    enum EMessageMode { NoMessage = 0, CloseMessage = 1, PresetsAdjustedMessage = 2, SwitchPresetMessage = 3, DeletePresetMessage = 4 };

    void onWheelChanged();
    /// The own character with points and a current wheel (the preset handling needs them).
    bool hasOwnWheel() const;
    void setCurrentPresetName(const QString& name);
    void validatePresets();
    void updatePresetsInfoModelIfNeeded();
    void ensureDefaultPreset();
    void selectMatchingPreset();
    void selectPendingPreset();
    void showPresetsAdjustedMessage();
    void updateParameters();
    void clearParameters();
    void showMessage(EMessageMode mode, const QString& caption, const QString& text);
    void deleteCurrentPreset();
    void updateSummaries();
    void updateInformation();
    void fillTile(cip::tibia::gamewindow::TTileInformation* info, int slot) const;
    void fillLargePerk(cip::tibia::gamewindow::TLargePerkInformation* info, int domain) const;
    static void clearTile(cip::tibia::gamewindow::TTileInformation* info);
    static void clearLargePerk(cip::tibia::gamewindow::TLargePerkInformation* info);
    /// CIP : adds `delta` points to a slice if the grid stays valid.
    void changePoints(int slot, int delta);
    static int stepSize(int modifiers);

    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The session's skill wheel storage (null without a session).
    TSkillWheelStorage* m_storage = nullptr;
    TPerkInfoModel* m_small;
    TPerkInfoModel* m_large;
    TPerkInfoModel* m_medium;
    TPerkInfoModel* m_vessels;
    TPerkInfoModel* m_full;
    TPresetInfoModel* m_presets;
    cip::tibia::gamewindow::TTileInformation* m_selectedTile;
    cip::tibia::gamewindow::TTileInformation* m_hoveredTile;
    cip::tibia::gamewindow::TLargePerkInformation* m_selectedLarge;
    cip::tibia::gamewindow::TLargePerkInformation* m_hoveredLarge;
    int m_selectedSlot = -1;
    int m_hoveredSlot = -1;
    int m_selectedLargeDomain = -1;
    int m_hoveredLargeDomain = -1;
    gamewindow::TMessageDialogController* m_message;
    gamewindow::TMessageDialogController* m_export;
    TSkillWheelPresetManagmentDialogController* m_presetManagmentDialog;
    EMessageMode m_messageMode = NoMessage;
    /// The next showDialogTab keeps the presets as they are.
    bool m_skipNextShowUpdate = false;
    QString m_pendingPreset;
    /// The selected preset and its wheel when it was selected.
    QString m_currentPresetName;
    std::optional<TWheel> m_presetWheel;
    /// CIP: the planner page the export code is appended to.
    QString m_plannerUrl;
};

/// The Gem Atelier tab, SkillWheelGemAtelierPage.qml (CIP: TGemAtelierPageController).
class TGemAtelierPageController final : public cip::tibia::gamewindow::TGemAtelierPageController
{
    Q_OBJECT
public:
    /// CIP: the dismantle question and the reshape dialog are made with `engine` in `dialogParentItem`.
    TGemAtelierPageController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);

    /// CIP: the reshape dialog and the dismantle question close; the page follows the session's
    /// skill wheel storage.
    void connectToGameSession(std::shared_ptr<game::IGameSession> GameSession);

    int totalVesselCount() const override { return VesselCount; }
    int activeVesselCount() const override;
    int gemsInInventory() const override { return static_cast<int>(m_storage->gemInventory().size()); }
    int unrevealedLesserGems() const override { return m_storage->unrevealedGems(0); }
    int unrevealedRegularGems() const override { return m_storage->unrevealedGems(1); }
    int unrevealedGreaterGems() const override { return m_storage->unrevealedGems(2); }
    QString unrevealedLesserGemName() const override;
    QString unrevealedRegularGemName() const override;
    QString unrevealedGreaterGemName() const override;
    int unrevealedLesserGemAppearanceType() const override;
    int unrevealedRegularGemAppearanceType() const override;
    int unrevealedGreaterGemAppearanceType() const override;
    bool canBeChanged() const override { return m_storage->changeState() == 1; }
    bool showUnusedVesselsNotification() const override;

    void socketOrUnsocketGem(int GemId, int VesselIndex) override;
    void dismantleGem(int GemId) override;
    /// CIP: closes the reshape dialog and the dismantle question (the skill wheel dialog closing).
    void closeMessageDialog();
    /// CIP (the dialog's reset on this tab): the current wheel's vessels are emptied.
    void resetVessels();
    /// The skill wheel dialog closed: back to the first page, filters and selection cleared.
    void clearData();
    void revealGem(int GemQuality) override;
    /// The reshape the reshape dialog confirmed: the storage forgets the attempt, the server applies it.
    void reshapeGem(int GemId, int ModIdx) override;
    /// The storage remembers the gem, the mod and the gem's mods; the server rolls the new mod.
    void attemptReshapeGem(int GemId, int ModIdx) override;
    void selectGem(int GemId, bool NarrowDownFilters) override;
    void selectGem(int GemId) override;
    void selectVessel(int Index) override;
    void toggleGemLock(int GemId) override;
    void selectModInFragmentWorkshop(int ModId, bool IsSupremeMod) override;
    QString getModGradeImageSource(int ModGrade, bool IsFilled) override;
    QString getModGradeImageSource(int ModGrade) override { return modGradeImage(ModGrade); }
    QString getModGradeHighlightImageSource(int ModGrade) override { return modGradeHighlightImage(ModGrade); }
    Q_INVOKABLE QObject* socketAt(int Index) override;
    Q_INVOKABLE int reshapeGemCost(cip::tibia::qmlcomponents::enums::EGemQuality Quality, bool GoldCost) override;
    Q_INVOKABLE int unrevealedGemAppearanceType(cip::tibia::qmlcomponents::enums::EGemQuality Quality) override;
    Q_INVOKABLE QVariantMap gemDataForId(int GemId) override;
    /// A new selection drops the gem waiting to be selected again.
    void setSelectedGemIndex(const QPersistentModelIndex& Index) override;

    void onGemsChanged();
    void onWheelChanged();
    void onResourcesChanged() { Q_EMIT unrevealedGemsChanged(); }
    /// The server rolled a reshape: the dialog shows the mod before and after.
    void onTemporaryReshapedGemChanged();
    /// The reshape dialog's new mod: the remembered gem and mod are reshaped.
    void onConfirmReshape();
    /// Shows a domain's vessel gem, or the domain's gems.
    void showDomain(int domain, int gemId);

Q_SIGNALS:
    void requestWorkshopMod(bool isBasic, int modId);

private:
    /// CIP: Yes/No before a gem is dismantled.
    gamewindow::TMessageDialogController* m_dismantleConfirm;
    int m_dismantleGemId = 0;
    void updateSockets();

    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The session's skill wheel storage (null without a session).
    TSkillWheelStorage* m_storage = nullptr;
    TInventoryGemListModel* m_list;
    TInventoryGemSortFilterProxyModel* m_filter;
    TPaginationProxyModel* m_pages;
    QObject* m_sockets[VesselCount] = {};
    /// The gem socketed or unsocketed last: selected again once the gem inventory changes
    /// (dropped by a new selection or when the gem is gone).
    std::optional<int> m_gemToReselect;
    TGemAtelierReshapeModDialogController* m_reshapeModDialog;
};

/// The Fragment Workshop tab, SkillWheelFragmentWorkshopPage.qml
/// (CIP: TFragmentWorkshopPageController).
class TFragmentWorkshopPageController final : public cip::tibia::gamewindow::TFragmentWorkshopPageController
{
    Q_OBJECT
public:
    /// CIP: the page's message box is made with `engine` in `dialogParentItem` (CipSoft's workshop
    /// only ever closes it).
    TFragmentWorkshopPageController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);

    /// CIP: the message box closes; the page follows the session's skill wheel storage.
    void connectToGameSession(std::shared_ptr<game::IGameSession> GameSession);

    int modCount() const override { return m_storage ? static_cast<int>(m_storage->modList().size()) : 0; }
    bool canBeChanged() const override { return m_storage && m_storage->changeState() == 1; }
    int bonusBaseStats() const override;
    int bonusPromotionPoints() const override;
    void setFilterMods(int v) override;

    void selectMod(bool isBasicMod, int ModID) override;
    void enhanceModGrade(bool isBasicMod, int ModID) override;
    QString getModGradeImageSource(int ModGrade) override { return modGradeImage(ModGrade); }
    int getGoldCostsForBasicModWithGrade(int modGrade) override { return basicGoldCost(modGrade); }
    int getGoldCostsForSupremeModWithGrade(int modGrade) override { return supremeGoldCost(modGrade); }
    int getFragmentCostsForGrade(int modGrade) override { return fragmentCost(modGrade); }

    void onModsChanged();
    /// CIP: the page's message box closes (the skill wheel dialog showing or closing).
    void closeMessageDialog();
    /// The list learns the current wheel and the grades (with a current wheel).
    void updateConfiguration();
    /// The skill wheel dialog closed: back to the first page, filters, selection and pending
    /// selection cleared.
    void clearData();

private:
    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The session's skill wheel storage (null without a session).
    TSkillWheelStorage* m_storage = nullptr;
    gamewindow::TMessageDialogController* m_message;
    TModListModel* m_list;
    TModListSortFilterProxyModel* m_filter;
    TPaginationProxyModel* m_pages;
    bool m_pendingBasic = true;
    int m_pendingMod = -1;
};

/// SkillWheelDialog.qml (CIP: TSkillWheelDialogController): the wheel, gem atelier and fragment
/// workshop tabs, and the skill wheel game actions.
class TSkillWheelDialogController final : public gamewindow::TDialogController<cip::tibia::gamewindow::TSkillWheelDialogController>
{
    Q_OBJECT
public:
    /// CIP: the wheel page (with the planner url), the gem atelier, the fragment workshop and the
    /// message box are made with `engine` in `dialogParentItem`.
    TSkillWheelDialogController(const QString& plannerUrl, QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
    ~TSkillWheelDialogController() override;

    /// CIP: the dialog closes, each page follows the session, the dialog keeps it.
    void connectToGameSession(std::shared_ptr<game::IGameSession> GameSession);
    /// CIP: the skill wheel actions open the dialog at a tab, ask about a missing wheel, or select a
    /// gem or mod; asking for the own wheel ends the skipped open after a save.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

    bool isPremium() const override;

    void requestClose() override;
    void requestTabSwitch(EDialogTab NewTab) override;
    void onOkClicked() override;
    void onApplyClicked() override;
    void onResetClicked() override;
    void getPremiumClicked() override;
    void requestCopyExportCodeToClipboarClicked() override;
    void showDialog() override;
    void closeDialog() override;
    void requestSkillWheelPreview() override;

    /// CIP: the client's premium time; a change of it is the dialog's
    /// premium status change.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage);

private:
    /// What the dialog's message box asks (CIP: the box's mode).
    enum EMessageMode { NoMessage = 0, PreviewPossibleMessage = 1, NoWheelMessage = 2 };

    /// Shows the dialog unless it is open (or a save of the own wheel skips it), then the tab.
    void openDialogTab(EDialogTab tab);
    /// The tab's page shows; the previous one hides.
    void switchTab(EDialogTab tab);
    void onMessageDialogAcceptClicked();
    void onMessageDialogDiscardClicked();
    void onRequestShowGemAtelier();
    void onRequestShowFragmentWorkshop(std::optional<int> ModId, std::optional<bool> IsBasicMod);
    void skipNextOpenForOwnPlayer();
    void clearSkipNextOpenForOwnPlayer();
    /// The session's skill wheel storage (null without a session).
    TSkillWheelStorage* skillWheelStorage() const;

    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    TSkillWheelPageController* m_page;
    TGemAtelierPageController* m_atelier;
    TFragmentWorkshopPageController* m_workshop;
    gamewindow::TMessageDialogController* m_message;
    EMessageMode m_messageMode = NoMessage;
    /// CIP: after a save of the own wheel, its open action does not show the dialog (for a second).
    QTimer m_skipNextOpenForOwnPlayerTimer;
    bool m_skipNextOpenForOwnPlayer = false;
    std::shared_ptr<game::IGameSession> m_gameSession;
};

}
}
