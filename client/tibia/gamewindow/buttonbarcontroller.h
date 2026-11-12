#pragma once

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace mods {
class AttentionBackend;
}
}

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// buttonsModel: the buttons clientoptions.json's
/// controlButtonsOptions.enabledButtons lists, in its order (CIP:
/// TButtonBarModel, static meta object ; role buttonId).
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
/// ctor). Its buttons publish game actions (widgets toggle through
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
    /// DAILY_REWARD_NOT_COLLECTABLE (handleGameAction).
    bool highlightRewardWallButton() const override;
    /// CIP: read from a session storage (connectToGameSession).
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
    /// CIP .
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
    /// CIP : closes the open multi-action buttons, then opens the
    /// options' first page.
    void onOptionsButtonClicked() override;
    /// product: open CIP Change Character (same as Ctrl+G / HUD top bar).
    void onSwitchCharacterButtonClicked();
    /// CIP : asks first.
    void onLogoutButtonClicked() override;
    void onRewardWallButtonClicked() override;
    void onAnalyticsSelectorWidgetButtonClicked() override;
    void onLenshelpButtonClicked() override;
    void onHighscoresDialogButtonClicked() override;
    void onExaltationForgeDialogButtonClicked() override;
    void onBossSlotsDialogButtonClicked() override;
    void onSkillWheelDialogButtonClicked() override;
    void onImbuementTrackerWidgetClicked() override;
    /// CIP : the dialog at the weapon in the left hand slot (6),
    /// if any (CIP sends its type id).
    void onWeaponProficiencyDialogButtonClicked() override;
    /// CIP .
    void onTaskboardDialogButtonClicked() override;
    /// CIP .
    void onPlayerGuideWidgetClicked() override;
    /// CIP .
    void onManageShortcutsClicked() override;
    /// CIP .
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

    /// CIP : TOGGLE_WIDGET.
    void toggleWidget(int widgetType);
    void setWidgetVisible(int widgetType, bool visible);

    TButtonBarModel m_model;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<TMessageDialogController> m_logoutQuestion;
    QPointer<mods::AttentionBackend> m_attention;
};

}
}
