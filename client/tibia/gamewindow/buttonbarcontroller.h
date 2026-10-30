#pragma once

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::mods {
class AttentionBackend;
}

namespace tibia::gamewindow {

class TMessageDialogController;

/// buttonsModel: the buttons clientoptions.json's
/// controlButtonsOptions.enabledButtons lists, in its order (CIP:
/// TButtonBarModel, static meta object 0x100b99360; role buttonId).
class TButtonBarModel final : public cip::TButtonBarModel
{
    Q_OBJECT
public:
    explicit TButtonBarModel(QObject* parent = nullptr);

    /// Re-reads the enabled buttons.
    void reload();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<int> m_buttons;
};

/// The control buttons panel, ButtonBarPanel.qml (CIP: TButtonBarController,
/// ctor 0x100b96090). Its buttons publish game actions (widgets toggle through
/// TOGGLE_WIDGET); the checked state follows the sidebar widgets manager's
/// widgetVisible / widgetHidden. The store, news (Compendium), social,
/// cyclopedia and bosstiary button controllers belong to their dialogs, which
/// set them here.
class TButtonBarController final : public cip::tibia::gamewindow::TButtonBarController
{
    Q_OBJECT
public:
    explicit TButtonBarController(QObject* parent = nullptr);
    ~TButtonBarController() override;

    /// sidebarPanelsOptions.controlButtonsVisible.
    void setSidebarOptions(config::TSidebarOptions* options);
    /// The button controller properties are constant: each setter emits
    /// buttonControllersChanged, on which the panel item rebinds
    /// panelController to re-read them.
    void setStoreButtonController(QObject* controller);
    void setNewsButtonController(QObject* controller);
    void setSocialButtonController(QObject* controller);
    void setCyclopediaButtonController(QObject* controller);
    void setBosstiaryButtonController(QObject* controller);
    void setAttentionStorage(mods::AttentionBackend* storage);

    /// CIP: set by DAILY_REWARD_READY_TO_PICK_UP, cleared by
    /// DAILY_REWARD_NOT_COLLECTABLE (handleGameAction 0x100b98e00).
    bool highlightRewardWallButton() const override;
    /// CIP: read from a session storage (connectToGameSession 0x100b98310).
    bool highlightWeaponProficiencyButton() const override;

    /// controlButtonsOptions.enabledButtons names, in order (product side rail).
    QStringList enabledShortcutIds() const;
    /// Same gold highlight ButtonBarPanel draws for this controlButtonsOptions id.
    bool shortcutHighlighted(const QString& name) const;
    /// Fire the same slot ButtonBarPanel's Loader would for `buttonId` (EButtons).
    void activateShortcut(int buttonId);
    /// Same as activateShortcut, keyed by controlButtonsOptions name.
    void activateShortcutByName(const QString& name);

public Q_SLOTS:
    /// CIP 0x100b965b0.
    void onDropdownButtonClicked() override;
    void onSkillButtonClicked() override;
    void onBattleListButtonClicked() override;
    void onPartyListButtonClicked() override;
    void onVipWidgetButtonClicked() override;
    void onQuestLogButtonClicked() override;
    void onQuestTrackerButtonClicked() override;
    void onBestiaryTrackerButtonClicked() override;
    void onBosstiaryTrackerButtonClicked() override;
    void onUnjustifiedPointsButtonClicked() override;
    void onKillTrackerWidgetButtonClicked() override;
    void onPreyDialogButtonClicked() override;
    void onSpellListWidgetButtonClicked() override;
    /// CIP 0x100b96df0: closes the open multi-action buttons, then opens the
    /// options' first page.
    void onOptionsButtonClicked() override;
    /// product: open CIP Change Character (same as Ctrl+G / HUD top bar).
    void onSwitchCharacterButtonClicked();
    /// CIP 0x100b96fb0: asks first.
    void onLogoutButtonClicked() override;
    void onRewardWallButtonClicked() override;
    void onAnalyticsSelectorWidgetButtonClicked() override;
    void onLenshelpButtonClicked() override;
    void onHighscoresDialogButtonClicked() override;
    void onExaltationForgeDialogButtonClicked() override;
    void onBossSlotsDialogButtonClicked() override;
    void onSkillWheelDialogButtonClicked() override;
    void onImbuementTrackerWidgetClicked() override;
    /// CIP 0x100b977f0: the dialog at the weapon in the left hand slot (6),
    /// if any (CIP sends its type id).
    void onWeaponProficiencyDialogButtonClicked() override;
    /// CIP 0x100b97a80.
    void onTaskboardDialogButtonClicked() override;
    /// CIP 0x100b97c80.
    void onPlayerGuideWidgetClicked() override;
    /// CIP 0x100b97d90.
    void onManageShortcutsClicked() override;
    /// CIP 0x100b97e90.
    void onShowContextMenu() override;

    /// CIP slots 38 / 39 (EWidgetType).
    void onWidgetVisible(int widgetType);
    void onWidgetHidden(int widgetType);

Q_SIGNALS:
    /// The player confirmed the logout question.
    void logoutRequested();
    void buttonControllersChanged();

private:
    void setButtonController(QObject*& member, QObject* controller);

    /// CIP 0x100b96630: TOGGLE_WIDGET.
    void toggleWidget(int widgetType);
    void setWidgetVisible(int widgetType, bool visible);

    TButtonBarModel m_model;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<TMessageDialogController> m_logoutQuestion;
    QPointer<mods::AttentionBackend> m_attention;
};

}
