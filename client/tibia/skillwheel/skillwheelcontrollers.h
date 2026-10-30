#pragma once

#include "gamewindow/messagedialogcontroller.h"
#include "skillwheel/skillwheelmodels.h"

#include <cip_gen.h>

namespace tibia::skillwheel {

/// One slice of SkillWheel.qml's skillParameters (CIP 0x100de5150).
class TSkillWheelTileQMLInfo final : public cip::TSkillWheelTileQMLInfo
{
    Q_OBJECT
public:
    TSkillWheelTileQMLInfo(int slot, double fill, bool unlocked, int mediumPerk, int smallPerk, const QUrl& image, int modType, QObject* parent);
};

/// One corner of SkillWheel.qml's cornerParameters (CIP 0x100de5150).
class TSkillWheelCornerQMLInfo final : public cip::TSkillWheelCornerQMLInfo
{
    Q_OBJECT
public:
    TSkillWheelCornerQMLInfo(int domain, double fill, int level, int largePerk, const QUrl& gem, const QUrl& bezel, QObject* parent);
};

/// A vessel of the gem atelier (CIP 0x100f9a160).
class TQmlSocketedGem final : public cip::tibia::skillwheel::TQmlSocketedGem
{
    Q_OBJECT
public:
    TQmlSocketedGem(int gemId, const QUrl& gem, const QUrl& bezel, QObject* parent);
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
    int spentSkillPoints() const override { return m_storage->spentPoints(); }
    int huntingTaskSkillPoints() const override { return m_storage->huntingTaskPoints(); }
    QString bonusPointItemNames() const override;
    bool hasShrinesOfInsightBonusPoints() const override { return m_storage->hasShrinesOfInsightPoints(); }
    cip::tibia::qmlcomponents::enums::EVocation vocation() const override;
    bool isWheelDirty() const override { return !m_storage->isPreview() && m_storage->isWheelDirty(); }
    bool canBeDecreased() const override { return m_storage->changeState() == 1; }
    bool canBeIncreased() const override { return m_storage->changeState() == 1 || m_storage->changeState() == 2; }
    bool isLocalPlayer() const override { return m_storage->isLocalPlayer(); }
    bool isPreview() const override { return m_storage->isPreview(); }
    QString playerName() const override { return m_storage->playerName(); }
    bool canManagePresets() const override { return m_storage->canManagePresets(); }
    bool isPresetDirty() const override { return m_storage->canManagePresets() && m_storage->isPresetDirty(); }
    QString currentPresetName() const override;
    bool morePresetsPossible() const override { return m_storage->presets().size() < MaxPresets; }
    bool deletePresetsPossible() const override { return m_storage->presets().size() > 1; }

    void requestClose() override;
    void requestSaveCurrentPreset() override;
    void onGridTileRightClicked(int GridTileQML, int KeyboardModifiers) override;
    void fillSkill(int GridTileQML) override;
    void addToSkill(int GridTileQML, int KeyboardModifiers) override;
    void clearSkill(int GridTileQML) override;
    void removeFromSkill(int GridTileQML, int KeyboardModifiers) override;
    void changedGem(int QuarterQML) override;
    void onGridTileSelected(int GridTileQML) override;
    void onGridTileHovered(int GridTileQML) override;
    void onLargePerkSelected(int QuarterQML) override;
    void onLargePerkHovered(int QuarterQML) override;
    void onVesselSelected(int QuarterQML) override;
    void onVesselHovered(int QuarterQML) override;
    void restSkillGrid() override;
    void requestCopyExportCodeToClipboard() override;
    void selectPresetByName(const QString& PresetName, bool IgnoreChanges) override;
    void requestDeletePresetClicked() override;
    void requestRenamePresetClicked() override;
    void requestNewPresetClicked() override;
    void requestImportPresetClicked() override;
    void requestExportPresetClicked() override;
    void setShowFullSummary(bool Show) override;
    void useServerSkillWheelAsCurrent() override;
    void clearSelection() override;
    void onNewPresetName(const QString& PresetName) override;
    void clearData() override;

    /// CIP onApplyClicked: saves the preset and sends the wheel.
    void apply();
    /// The close prompt (CIP mode 1); false when nothing to ask.
    bool askToSaveBeforeClose();
    void closeMessages();

Q_SIGNALS:
    /// The vessel's "Change Gem" button: show the gem atelier for a domain.
    void showGemAtelier(int domain, int gemId);
    /// The management dialog for a mode (TSkillWheelPresetManagmentDialogController::EManageMode).
    void managePresets(int mode);

private:
    void onWheelChanged();
    void onInitialData();
    void updateParameters();
    void updateSummaries();
    void updateInformation();
    void fillTile(cip::tibia::gamewindow::TTileInformation* info, int slot) const;
    void fillLargePerk(cip::tibia::gamewindow::TLargePerkInformation* info, int domain) const;
    void fillVessel(cip::tibia::gamewindow::TVesselInformation* info, int domain) const;
    /// CIP 0x1011225d0: adds `delta` points to a slice if the grid stays valid.
    void changePoints(int slot, int delta);
    static int stepSize(int modifiers);

    TSkillWheelStorage* m_storage;
    TSummaryModel* m_small;
    TSummaryModel* m_large;
    TSummaryModel* m_medium;
    TSummaryModel* m_vessels;
    TSummaryModel* m_full;
    TPresetInfoModel* m_presets;
    cip::tibia::gamewindow::TTileInformation* m_selectedTile;
    cip::tibia::gamewindow::TTileInformation* m_hoveredTile;
    cip::tibia::gamewindow::TLargePerkInformation* m_selectedLarge;
    cip::tibia::gamewindow::TLargePerkInformation* m_hoveredLarge;
    cip::tibia::gamewindow::TVesselInformation* m_selectedVessel;
    cip::tibia::gamewindow::TVesselInformation* m_hoveredVessel;
    int m_selectedSlot = -1;
    int m_hoveredSlot = -1;
    int m_selectedLargeDomain = -1;
    int m_hoveredLargeDomain = -1;
    int m_selectedVesselDomain = -1;
    int m_hoveredVesselDomain = -1;
    gamewindow::TMessageDialogController* m_switchPreset;
    gamewindow::TMessageDialogController* m_deletePreset;
    gamewindow::TMessageDialogController* m_info;
    QString m_pendingPreset;
};

/// The Gem Atelier tab, SkillWheelGemAtelierPage.qml (CIP: TGemAtelierPageController).
class TGemAtelierPageController final : public cip::tibia::gamewindow::TGemAtelierPageController
{
    Q_OBJECT
public:
    explicit TGemAtelierPageController(TSkillWheelStorage* storage, QObject* parent = nullptr);

    int gemsInInventory() const override { return static_cast<int>(m_storage->gems().size()); }
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
    int changeAffinityCost() const override;

    void socketOrUnsocketGem(int GemId) override;
    void dismantleGem(int GemId) override;
    void revealGem(int GemQuality) override;
    void selectGem(int GemId, bool NarrowDownFilters) override;
    void selectGem(int GemId) override;
    void changeAffinityOfGem(int GemId) override;
    void toggleGemLock(int GemId) override;
    void selectModInFragmentWorkshop(int ModId, bool IsSupremeMod) override;
    QString getModGradeImageSource(int ModGrade) override { return modGradeImage(ModGrade); }
    QString getModGradePotentialSource(int ModGradePotential) override { return modGradePotentialImage(ModGradePotential); }
    QString getModGradeHighlightImageSource(int ModGrade) override { return modGradeHighlightImage(ModGrade); }

    void onGemsChanged();
    void onWheelChanged();
    void onResourcesChanged() { Q_EMIT unrevealedGemsChanged(); }
    /// Shows a domain's vessel gem, or the domain's gems.
    void showDomain(int domain, int gemId);

Q_SIGNALS:
    void requestWorkshopMod(bool isBasic, int modId);

private:
    void updateSockets();
    void gemAction(int type, int param, int pos = 0);

    TSkillWheelStorage* m_storage;
    TInventoryGemListModel* m_list;
    TInventoryGemSortFilterProxyModel* m_filter;
    TPaginationProxyModel* m_pages;
    int m_selectedGemId = -1;
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
    TModFilterProxyModel* m_filter;
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
    TSkillWheelPresetManagmentDialogController(TSkillWheelStorage* storage, EManageMode mode, QObject* parent = nullptr);

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
    gamewindow::TMessageDialogController* m_closePrompt;
    TSkillWheelPresetManagmentDialogController* m_presetDialog = nullptr;
};

}
