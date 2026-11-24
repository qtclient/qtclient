#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/sidebarpanelcontrollerbase.h"
#include "qtconnectionmanager.h"
#include <cip_gen.h>

#include <QPointer>
#include <QStringList>

#include <functional>
#include <memory>
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
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
namespace skillwheel {
class TSkillWheelStorage;
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
class TButtonBarController final : public TSidebarPanelController<cip::tibia::gamewindow::TButtonBarController>
{
    Q_OBJECT
public:
    /// Pack Display Shortcuts (controlButtonsOptions) entries. Numeric ids start
    /// at ProductShortcutIdBase so they never collide with CIP EButtons.
    static constexpr int ProductShortcutIdBase = 1000;

    explicit TButtonBarController(QObject* parent = nullptr);
    ~TButtonBarController() override;

    /// Register a product shortcut for Options → Display Shortcuts and the
    /// button bar grid. `id` is stored in controlButtonsOptions; `label` is
    /// shown in the options lists and as the button tooltip. When
    /// `defaultEnabled`, reconcile appends it to enabledButtons if the user
    /// has never seen the id. `imageSource` is a `/images/…` (or pack qrc)
    /// path for ButtonBarPanel; empty keeps the text label on the button.
    static void addProductShortcut(const QString& id, const QString& label, std::function<void()> run,
                                   bool defaultEnabled = true, const QString& imageSource = {});
    static QStringList productShortcutIds();
    static QStringList defaultEnabledProductShortcutIds();
    static QString productShortcutLabel(const QString& id);
    static QString productShortcutImage(const QString& id);
    static QString productShortcutIdFromLabel(const QString& label);
    static int productShortcutButtonId(const QString& id);
    static QString productShortcutIdFromButtonId(int buttonId);
    static bool isProductShortcutButtonId(int buttonId);

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
    /// Relays the storage's premiumTimeChanged as premiumTimeChanged (a
    /// previous storage's relay is dropped), then announces it once.
    void connectToPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage);
    /// Closes the logout question and follows the session: its sidebar
    /// options (the shortcuts' visibility), and the skill wheel, forge,
    /// proficiency and taskboard storages behind the button highlights. The
    /// button controllers follow the session too; the highlights are
    /// announced at the end.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// CIP: set by DAILY_REWARD_READY_TO_PICK_UP, cleared by
    /// DAILY_REWARD_NOT_COLLECTABLE (handleGameAction).
    bool highlightRewardWallButton() const override;
    /// The proficiency storage has an unspent perk (or the pack's attention
    /// signal is up).
    bool highlightWeaponProficiencyButton() const override;
    /// The skill wheel has unspent points.
    bool highlightSkillWheelButton() const override;
    /// The forge's dust balance has reached the dust limit.
    bool highlightExaltationForgeDialogButton() const override;
    /// This week's results are in, or a bounty task can be claimed.
    bool taskboardButtonHighlight() const override;

    /// controlButtonsOptions.enabledButtons names, in order (product side rail).
    QStringList enabledShortcutIds() const;
    /// Same gold highlight ButtonBarPanel draws for this controlButtonsOptions id.
    bool shortcutHighlighted(const QString& name) const;
    /// Fire the same slot ButtonBarPanel's Loader would for `buttonId` (EButtons).
    void activateShortcut(int buttonId);
    /// Same as activateShortcut, keyed by controlButtonsOptions name.
    void activateShortcutByName(const QString& name);

    /// ButtonBarPanel product delegate: label for a ProductShortcutIdBase id.
    Q_INVOKABLE QString productShortcutLabelForButtonId(int buttonId) const;
    /// ButtonBarPanel product delegate: icon path (empty → show label as text).
    Q_INVOKABLE QString productShortcutImageForButtonId(int buttonId) const;
    /// ButtonBarPanel product delegate click.
    Q_INVOKABLE void onProductShortcutClicked(int buttonId);

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
    /// Asks first; the answer goes to onLogoutRequestConfirmedOrCanceled.
    void onLogoutButtonClicked() override;
    /// "Yes" quits the game.
    void onLogoutRequestConfirmedOrCanceled(cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier Button);
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

    /// Closes the logout question.
    void onGamesessionChanged() override;
    /// The model takes the enabled buttons of the button options.
    void onButtonOptionsChanged();
    /// The shortcuts follow the sidebar panels options' button visibility
    /// (written back to the options, then announced).
    void onSidebarPanelsOptionsChanged();

    /// CIP slots 38 / 39 (EWidgetType).
    void onWidgetVisible(int widgetType);
    void onWidgetHidden(int widgetType);

Q_SIGNALS:
    void buttonControllersChanged();

private:
    void setButtonController(QObject*& member, QObject* controller);

    /// CIP : TOGGLE_WIDGET.
    void toggleWidget(int widgetType);
    void setWidgetVisible(int widgetType, bool visible);

    TButtonBarModel m_model;
    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<skillwheel::TSkillWheelStorage> m_skillWheelStorage;
    shared::TQtConnectionManager m_gameSessionConnections;
    QPointer<TMessageDialogController> m_logoutQuestion;
    QPointer<mods::AttentionBackend> m_attention;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;

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
