#pragma once

#include "gamewindow/messagedialogcontroller.h"
#include "skillwheel/skillwheelmodels.h"

#include <cip_gen.h>

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

/// The Wheel tab, SkillWheelPage.qml (CIP: TSkillWheelPageController).
class TSkillWheelPageController final : public cip::tibia::gamewindow::TSkillWheelPageController
{
    Q_OBJECT
public:
    explicit TSkillWheelPageController(TSkillWheelStorage* storage, QObject* parent = nullptr);

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
    bool canBeDecreased() const override { return m_storage->changeState() == 1; }
    bool canBeIncreased() const override { return m_storage->changeState() == 1 || m_storage->changeState() == 2; }
    bool isLocalPlayer() const override { return m_storage->isOwnCharacter(); }
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
    /// The close prompt (CIP mode 1); false when nothing to ask.
    void closeMessages();
    /// Domain/gem for the selected vessel tile when opening the gem atelier.
    int gemAtelierDomain() const;
    int gemAtelierGemId() const;

Q_SIGNALS:
    /// The management dialog for a mode (TSkillWheelPresetManagmentDialogController::EManageMode).
    void managePresets(int mode);

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

    TSkillWheelStorage* m_storage;
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
    explicit TGemAtelierPageController(TSkillWheelStorage* storage, QObject* parent = nullptr);

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
    /// CIP: closes the dismantle question (the skill wheel dialog closing).
    void closeMessageDialog();
    void revealGem(int GemQuality) override;
    void reshapeGem(int GemId, int ModIdx) override;
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
    /// Shows a domain's vessel gem, or the domain's gems.
    void showDomain(int domain, int gemId);

Q_SIGNALS:
    void requestWorkshopMod(bool isBasic, int modId);

private:
    /// CIP: Yes/No before a gem is dismantled.
    gamewindow::TMessageDialogController* m_dismantleConfirm;
    int m_dismantleGemId = 0;
    void updateSockets();

    TSkillWheelStorage* m_storage;
    TInventoryGemListModel* m_list;
    TInventoryGemSortFilterProxyModel* m_filter;
    TPaginationProxyModel* m_pages;
    QObject* m_sockets[VesselCount] = {};
    /// The gem socketed or unsocketed last: selected again once the gem inventory changes
    /// (dropped by a new selection or when the gem is gone).
    std::optional<int> m_gemToReselect;
};

/// The Fragment Workshop tab, SkillWheelFragmentWorkshopPage.qml
/// (CIP: TFragmentWorkshopPageController).
class TFragmentWorkshopPageController final : public cip::tibia::gamewindow::TFragmentWorkshopPageController
{
    Q_OBJECT
public:
    explicit TFragmentWorkshopPageController(TSkillWheelStorage* storage, QObject* parent = nullptr);

    int modCount() const override { return m_list->rowCount(); }
    bool canBeChanged() const override { return m_storage->changeState() == 1; }
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

private:
    TSkillWheelStorage* m_storage;
    TModListModel* m_list;
    TModListSortFilterProxyModel* m_filter;
    TPaginationProxyModel* m_pages;
    bool m_pendingBasic = true;
    int m_pendingMod = -1;
};

/// SkillWheelPresetManagmentDialog.qml (CIP: TSkillWheelPresetManagmentDialogController).
class TSkillWheelPresetManagmentDialogController final
    : public gamewindow::TDialogController<cip::tibia::gamewindow::TSkillWheelPresetManagmentDialogController>
{
    Q_OBJECT
public:
    TSkillWheelPresetManagmentDialogController(TSkillWheelStorage* storage, EManageMode mode, const QString& currentPresetName,
                                               QObject* parent = nullptr);

    void onOkClicked(EManageMode ManageMod) override;
    void onCloseClicked() override;
    void setNewName(const QString& NewName) override;
    void setImportString(const QString& ImportString) override;

private:
    bool isValidName(const QString& name) const;
    /// The import error text; empty: `wheel` (if given) receives the imported wheel.
    QString importError(TWheel* wheel) const;

    TSkillWheelStorage* m_storage;
    QString m_name;
    QString m_importString;
};

/// SkillWheelDialog.qml (CIP: TSkillWheelDialogController).
class TSkillWheelDialogController final : public gamewindow::TDialogController<cip::tibia::gamewindow::TSkillWheelDialogController>
{
    Q_OBJECT
public:
    explicit TSkillWheelDialogController(TSkillWheelStorage* storage, QObject* parent = nullptr);
    ~TSkillWheelDialogController() override;

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

    void showMessage(const QString& caption, const QString& text, bool offerPreview);
    void openDialogTab(EDialogTab tab);
    void openGemAtelier(int domain, int gemId);
    void openWorkshopMod(bool isBasic, int modId);
    void onPremiumChanged() { Q_EMIT premiumStatusChanged(); }
    void clear();

private:
    void managePresets(int mode);

    TSkillWheelStorage* m_storage;
    TSkillWheelPageController* m_page;
    TGemAtelierPageController* m_atelier;
    TFragmentWorkshopPageController* m_workshop;
    gamewindow::TMessageDialogController* m_message;
    TSkillWheelPresetManagmentDialogController* m_presetDialog = nullptr;
};

}
}
