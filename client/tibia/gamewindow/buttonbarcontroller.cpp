#include "buttonbarcontroller.h"

#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "config/sidebaroptions.h"
#include "config/characterconfigurationmanager.h"
#include "container/containerciptypes.h"
#include "exaltation/exaltationstorage.h"
#include "game/currentgamesession.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/cyclopediadialogcontroller.h"
#include "gamewindow/newsdialogcontroller.h"
#include "gamewindow/socialbuttoncontroller.h"
#include "gamewindow/storedialogcontroller.h"
#include "proficiency/proficiencystorage.h"
#include "skillwheel/skillwheelstorage.h"
#include "taskboard/taskboardbountytasksstorage.h"
#include "taskboard/taskboardweeklytasksstorage.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/sidebarwidgetsmanager.h"
#include "mods/attention_backend.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <client/const.h>

#include <QJsonArray>
#include <QMetaObject>
#include <QStringList>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Buttons = cip::tibia::gamewindow::TButtonBarController;
using W = TSidebarWidgetsManager;

constexpr int ButtonIdRole = Qt::UserRole + 1;

/// controlButtonsOptions' button names (CIP's spelling) by EButtons.
constexpr std::pair<const char*, int> ButtonNames[] = {
    { "skillsWidget", Buttons::SKILLS_WIDGET },
    { "battleListWidget", Buttons::BATTLE_WIDGET },
    { "partyWidget", Buttons::PARTY_WIDGET },
    { "vipWidget", Buttons::VIP_WIDGET },
    { "spellListWidget", Buttons::SPELLS_WIDGET },
    { "skillWheelDialog", Buttons::SKILL_WHEEL_DIALOG },
    { "questDialog", Buttons::QUEST_DIALOG },
    { "questTrackerWidget", Buttons::QUEST_TRACKER_WIDGET },
    { "unjustifiedPoinsWidget", Buttons::UNJUSTIFIED_WIDGET },
    { "preyDialog", Buttons::PREY_DIALOG },
    { "preyWidget", Buttons::KILL_TRACKER_WIDGET },
    { "rewardWallDialog", Buttons::DAILY_REWARD_DIALOG },
    { "analyticsSelectorWidget", Buttons::ANALYTICS_WIDGET },
    { "compendiumDialog", Buttons::COMPENDIUM_DIALOG },
    { "cyclopediaDialog", Buttons::CYCLOPEDIA_DIALOG },
    { "bosstiaryDialog", Buttons::BOSSTIARY_DIALOG },
    { "bossslotsDialog", Buttons::BOSSSLOTS_DIALOG },
    { "bosstiaryTrackerWidget", Buttons::BOSSTIARY_TRACKER_WIDGET },
    { "bestiaryTrackerWidget", Buttons::BESTIARY_TRACKER_WIDGET },
    { "imbuementTrackerWidget", Buttons::IMBUEMENT_TRACKER_WIDGET },
    { "weaponProficiency", Buttons::WEAPON_PROFICIENCY_WIDGET },
    { "exaltationForgeDialog", Buttons::EXALTATION_FORGE_DIALOG },
    { "friendsDialog", Buttons::SOCIAL_DIALOG },
    { "lenshelpFunction", Buttons::LENSHELP },
    { "highscoresDialog", Buttons::HIGHSCORES_DIALOG },
    { "playerGuide", Buttons::PLAYER_GUIDE },
    { "manageShortcuts", Buttons::MANAGE_SHORTCUTS },
    { "taskboard", Buttons::TASKBOARD_DIALOG },
};

struct TProductShortcut {
    QString id;
    QString label;
    QString imageSource;
    std::function<void()> run;
    bool defaultEnabled = true;
    int buttonId = 0;
};

std::vector<TProductShortcut>& productShortcutList()
{
    static std::vector<TProductShortcut> list;
    return list;
}

/// Packs register after TButtonBarModel's first reload. When the id is already
/// in controlButtonsOptions, ensureProductShortcutInOptions is a no-op and
/// optionChanged never fires — wake every live button bar model instead.
std::vector<QPointer<TButtonBarModel>>& liveProductShortcutModels()
{
    static std::vector<QPointer<TButtonBarModel>> models;
    return models;
}

void notifyProductShortcutModels()
{
    auto& models = liveProductShortcutModels();
    for (auto it = models.begin(); it != models.end();) {
        if (!*it) {
            it = models.erase(it);
            continue;
        }
        (*it)->reload();
        ++it;
    }
}

void ensureProductShortcutInOptions(const TProductShortcut& shortcut)
{
    constexpr auto Section = "controlButtonsOptions";
    const QJsonObject root = app::ClientOptions::load().root();
    QJsonObject section = root.value(QLatin1String(Section)).toObject();
    QStringList enabled;
    QStringList disabled;
    for (const QJsonValue& value : section.value(QLatin1String("enabledButtons")).toArray()) {
        const QString id = value.toString();
        if (!id.isEmpty())
            enabled << id;
    }
    for (const QJsonValue& value : section.value(QLatin1String("disabledButtons")).toArray()) {
        const QString id = value.toString();
        if (!id.isEmpty())
            disabled << id;
    }
    if (enabled.contains(shortcut.id) || disabled.contains(shortcut.id))
        return;
    if (shortcut.defaultEnabled)
        enabled << shortcut.id;
    else
        disabled << shortcut.id;
    section.insert(QLatin1String("enabledButtons"), QJsonArray::fromStringList(enabled));
    section.insert(QLatin1String("disabledButtons"), QJsonArray::fromStringList(disabled));
    app::ClientOptions::storeAll({ { QLatin1String(Section), section } });
}

}

TButtonBarModel::TButtonBarModel(QObject* parent)
    : cip::TButtonBarModel(parent)
{
    liveProductShortcutModels().push_back(this);
    reload();
}

void TButtonBarModel::reload()
{
    std::vector<int> buttons;
    const QJsonObject options = app::ClientOptions::load().root().value(QLatin1String("controlButtonsOptions")).toObject();
    for (const QJsonValue& value : options.value(QLatin1String("enabledButtons")).toArray()) {
        const QString name = value.toString();
        bool found = false;
        for (const auto& [buttonName, id] : ButtonNames) {
            if (name == QLatin1String(buttonName)) {
                buttons.push_back(id);
                found = true;
                break;
            }
        }
        if (!found) {
            const int productId = TButtonBarController::productShortcutButtonId(name);
            if (productId >= 0)
                buttons.push_back(productId);
        }
    }
    if (buttons == m_buttons)
        return;
    beginResetModel();
    m_buttons = std::move(buttons);
    endResetModel();
}

void TButtonBarController::addProductShortcut(const QString& id, const QString& label, std::function<void()> run,
                                              const bool defaultEnabled, const QString& imageSource)
{
    if (id.isEmpty() || !run)
        return;
    auto& list = productShortcutList();
    for (TProductShortcut& existing : list) {
        if (existing.id == id) {
            existing.label = label;
            existing.imageSource = imageSource;
            existing.run = std::move(run);
            existing.defaultEnabled = defaultEnabled;
            ensureProductShortcutInOptions(existing);
            notifyProductShortcutModels();
            return;
        }
    }
    TProductShortcut shortcut;
    shortcut.id = id;
    shortcut.label = label.isEmpty() ? id : label;
    shortcut.imageSource = imageSource;
    shortcut.run = std::move(run);
    shortcut.defaultEnabled = defaultEnabled;
    shortcut.buttonId = ProductShortcutIdBase + static_cast<int>(list.size());
    list.push_back(shortcut);
    ensureProductShortcutInOptions(list.back());
    notifyProductShortcutModels();
}

QStringList TButtonBarController::productShortcutIds()
{
    QStringList ids;
    for (const TProductShortcut& shortcut : productShortcutList())
        ids << shortcut.id;
    return ids;
}

QStringList TButtonBarController::defaultEnabledProductShortcutIds()
{
    QStringList ids;
    for (const TProductShortcut& shortcut : productShortcutList()) {
        if (shortcut.defaultEnabled)
            ids << shortcut.id;
    }
    return ids;
}

QString TButtonBarController::productShortcutLabel(const QString& id)
{
    for (const TProductShortcut& shortcut : productShortcutList()) {
        if (shortcut.id == id)
            return shortcut.label;
    }
    return {};
}

QString TButtonBarController::productShortcutImage(const QString& id)
{
    for (const TProductShortcut& shortcut : productShortcutList()) {
        if (shortcut.id == id)
            return shortcut.imageSource;
    }
    return {};
}

QString TButtonBarController::productShortcutIdFromLabel(const QString& label)
{
    for (const TProductShortcut& shortcut : productShortcutList()) {
        if (shortcut.label == label)
            return shortcut.id;
    }
    return {};
}

int TButtonBarController::productShortcutButtonId(const QString& id)
{
    for (const TProductShortcut& shortcut : productShortcutList()) {
        if (shortcut.id == id)
            return shortcut.buttonId;
    }
    return -1;
}

QString TButtonBarController::productShortcutIdFromButtonId(const int buttonId)
{
    for (const TProductShortcut& shortcut : productShortcutList()) {
        if (shortcut.buttonId == buttonId)
            return shortcut.id;
    }
    return {};
}

bool TButtonBarController::isProductShortcutButtonId(const int buttonId)
{
    return buttonId >= ProductShortcutIdBase;
}

QString TButtonBarController::productShortcutLabelForButtonId(const int buttonId) const
{
    const QString id = productShortcutIdFromButtonId(buttonId);
    const QString label = productShortcutLabel(id);
    return label.isEmpty() ? id : label;
}

QString TButtonBarController::productShortcutImageForButtonId(const int buttonId) const
{
    return productShortcutImage(productShortcutIdFromButtonId(buttonId));
}

void TButtonBarController::onProductShortcutClicked(const int buttonId)
{
    const QString id = productShortcutIdFromButtonId(buttonId);
    for (const TProductShortcut& shortcut : productShortcutList()) {
        if (shortcut.id == id && shortcut.run) {
            shortcut.run();
            return;
        }
    }
}

int TButtonBarModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_buttons.size()); }

QVariant TButtonBarModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount() || role != ButtonIdRole)
        return {};
    return m_buttons[static_cast<size_t>(index.row())];
}

QHash<int, QByteArray> TButtonBarModel::roleNames() const { return { { ButtonIdRole, "buttonId" } }; }

TButtonBarController::TButtonBarController(config::TClientOptions* ClientOptions, TSocialDialogController* SocialDialog,
                                           QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TSidebarPanelController(parent)
    , m_pClientOptions(ClientOptions)
    , m_logoutQuestion(std::make_unique<TMessageDialogController>(Engine, DialogParentItem))
    , m_socialButton(std::make_unique<TSocialButtonController>(SocialDialog))
    , m_newsButton(std::make_unique<TNewsButtonController>())
    , m_storeButton(std::make_unique<TStoreButtonController>())
    , m_cyclopediaButton(std::make_unique<TCyclopediaButtonController>())
    , m_bosstiaryButton(std::make_unique<TBosstiaryButtonController>())
{
    m_buttonsModel = &m_model;
    m_socialButtonController = m_socialButton.get();
    m_newsButtonController = m_newsButton.get();
    m_storeButtonController = m_storeButton.get();
    m_cyclopediaButtonController = m_cyclopediaButton.get();
    m_bosstiaryButtonController = m_bosstiaryButton.get();

    if (m_pClientOptions)
        connect(m_pClientOptions.data(), &config::TClientOptions::buttonBarOptionsChanged, this, &TButtonBarController::onButtonOptionsChanged);
    connect(m_logoutQuestion.get(), &TMessageDialogController::buttonClicked, this, &TButtonBarController::onLogoutRequestConfirmedOrCanceled);
    m_logoutQuestion->setCloseOnButtonClicked(true);
    // The buttons' game actions go out as the bar's.
    connect(m_socialButton.get(), &TSocialButtonController::publishGameAction, this, &TButtonBarController::publishGameAction);
    connect(m_newsButton.get(), &TNewsButtonController::publishGameAction, this, &TButtonBarController::publishGameAction);
    connect(m_storeButton.get(), &TStoreButtonController::publishGameAction, this, &TButtonBarController::publishGameAction);
    connect(m_cyclopediaButton.get(), &TCyclopediaButtonController::publishGameAction, this, &TButtonBarController::publishGameAction);
    connect(m_bosstiaryButton.get(), &TBosstiaryButtonController::publishGameAction, this, &TButtonBarController::publishGameAction);

    // Host: product shortcuts write controlButtonsOptions straight to the options file.
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("controlButtonsOptions"))
            onButtonOptionsChanged();
    });
    const auto setRewardWallHighlight = [this](const bool on) {
        if (m_highlightRewardWallButton == on)
            return;
        m_highlightRewardWallButton = on;
        Q_EMIT highlightRewardWallButtonChanged();
    };
    m_gameActions.on(input::EGameAction::DAILY_REWARD_READY_TO_PICK_UP, [setRewardWallHighlight](const input::IGameAction&) {
        setRewardWallHighlight(true);
        return true;
    });
    m_gameActions.on(input::EGameAction::DAILY_REWARD_NOT_COLLECTABLE, [setRewardWallHighlight](const input::IGameAction&) {
        setRewardWallHighlight(false);
        return true;
    });
}

TButtonBarController::~TButtonBarController()
{
}

void TButtonBarController::setAttentionStorage(mods::AttentionBackend* storage)
{
    if (m_attention == storage)
        return;
    if (m_attention)
        disconnect(m_attention, nullptr, this, nullptr);
    m_attention = storage;
    if (storage) {
        connect(storage, &mods::AttentionBackend::signalChanged, this, [this](const QString& signalId, bool) {
            if (signalId == QLatin1String("rewardWall"))
                Q_EMIT highlightRewardWallButtonChanged();
            else if (signalId == QLatin1String("proficiency"))
                Q_EMIT highlightWeaponProficiencyButtonChanged();
        });
    }
    Q_EMIT highlightRewardWallButtonChanged();
    Q_EMIT highlightWeaponProficiencyButtonChanged();
}

void TButtonBarController::connectToPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage)
{
    if (m_premiumTimeStorage)
        disconnect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TButtonBarController::premiumTimeChanged);
    m_premiumTimeStorage = premiumTimeStorage;
    if (m_premiumTimeStorage)
        connect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TButtonBarController::premiumTimeChanged);
    Q_EMIT premiumTimeChanged();
}

void TButtonBarController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_logoutQuestion->dialog()->close();
    m_gameSessionConnections.disconnectAll();
    m_skillWheelStorage = nullptr;
    m_gameSession = GameSession;
    using Self = TButtonBarController;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
        const config::TCharacterConfigurationManager* configuration = session->characterConfigurationManager().get();
        if (configuration != nullptr) {
            if (m_options)
                disconnect(m_options, nullptr, this, nullptr);
            m_options = configuration->sidebarOptions();
            if (m_options)
                m_gameSessionConnections.add(connect(m_options.data(), &config::TSidebarOptions::sidebarPanelsOptionsChanged, this,
                                                     &Self::onSidebarPanelsOptionsChanged));
            m_skillWheelStorage = configuration->skillWheelStorage();
            if (m_skillWheelStorage)
                m_gameSessionConnections.add(connect(m_skillWheelStorage.data(), &cip::tibia::skillwheel::TSkillWheelStorage::unspentSkillPointsChanged,
                                                     this, &Self::highlightSkillWheelButtonChanged));
        }
        if (exaltation::TExaltationStorage* exaltation = session->exaltationStorage().get()) {
            m_gameSessionConnections.add(connect(exaltation, &cip::tibia::exaltation::TExaltationStorage::currentDustLimitChanged, this,
                                                 &Self::highlightExaltationForgeDialogButtonChanged));
            m_gameSessionConnections.add(connect(exaltation, &cip::tibia::exaltation::TExaltationStorage::exaltedCurrencyBalanceChanged,
                                                 this, &Self::highlightExaltationForgeDialogButtonChanged));
        }
        if (proficiency::TProficiencyStorage* proficiencies = session->proficiencyStorage())
            m_gameSessionConnections.add(connect(proficiencies, &proficiency::TProficiencyStorage::unspentPerkChanged, this,
                                                 &Self::highlightWeaponProficiencyButtonChanged));
        if (taskboard::TTaskboardWeeklyTasksStorage* weeklyTasks = session->taskboardWeeklyTasksStorage().get())
            m_gameSessionConnections.add(connect(weeklyTasks, &taskboard::TTaskboardWeeklyTasksStorage::weeklyTaskDataChanged, this,
                                                 [this] { Q_EMIT highlightTaskboardButtonChanged(); }));
        if (taskboard::TTaskboardBountyTasksStorage* bountyTasks = session->taskboardBountyTasksStorage().get())
            m_gameSessionConnections.add(connect(bountyTasks, &taskboard::TTaskboardBountyTasksStorage::bountyTaskDataChanged, this,
                                                 [this] { Q_EMIT highlightTaskboardButtonChanged(); }));
        onSidebarPanelsOptionsChanged();
    }
    // Host: a pack's social button (setSocialButtonController) follows the session itself.
    m_socialButton->connectToGameSession(m_gameSession);
    m_storeButton->connectToGameSession(m_gameSession);
    m_cyclopediaButton->connectToGameSession(m_gameSession);
    m_bosstiaryButton->connectToGameSession(m_gameSession);
    Q_EMIT highlightSkillWheelButtonChanged();
    Q_EMIT highlightExaltationForgeDialogButtonChanged();
}

void TButtonBarController::onSidebarPanelsOptionsChanged()
{
    if (!m_options || m_shortcutsVisible == m_options->sidebarPanelsOptions().controlButtonsVisible)
        return;
    m_shortcutsVisible = m_options->sidebarPanelsOptions().controlButtonsVisible;
    config::SidebarPanelsOptions options = m_options->sidebarPanelsOptions();
    options.controlButtonsVisible = m_shortcutsVisible;
    m_options->setSidebarPanelsOptions(options);
    Q_EMIT shortcutsVisibleChanged();
}

void TButtonBarController::onGamesessionChanged()
{
    m_logoutQuestion->dialog()->close();
}

void TButtonBarController::onButtonOptionsChanged() { m_model.reload(); }

bool TButtonBarController::highlightRewardWallButton() const
{
    return m_highlightRewardWallButton || (m_attention && m_attention->rewardWall());
}

bool TButtonBarController::highlightWeaponProficiencyButton() const
{
    if (m_attention && m_attention->proficiency())
        return true;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return false;
    const proficiency::TProficiencyStorage* proficiencies = session->proficiencyStorage();
    return proficiencies != nullptr && proficiencies->hasUnspentPerk();
}

bool TButtonBarController::highlightSkillWheelButton() const
{
    return m_skillWheelStorage && m_skillWheelStorage->unspentSkillPoints() > 0;
}

bool TButtonBarController::highlightExaltationForgeDialogButton() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (session == nullptr || !session->exaltationStorage())
        return false;
    const exaltation::TExaltationStorage& exaltation = *session->exaltationStorage();
    return static_cast<uint16_t>(exaltation.currentDustLimit()) <= exaltation.dustBalance();
}

bool TButtonBarController::taskboardButtonHighlight() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (session == nullptr || !session->taskboardWeeklyTasksStorage() || !session->taskboardBountyTasksStorage())
        return false;
    if (session->taskboardWeeklyTasksStorage()->weeklyTaskData().progressFinished == 1)
        return true;
    for (const taskboard::TBountyTask& task : session->taskboardBountyTasksStorage()->bountyTaskData().tasks) {
        if (task.claimState == taskboard::EBountyTaskClaimState::Claimable)
            return true;
    }
    return false;
}

void TButtonBarController::setSocialButtonController(QObject* controller)
{
    QObject* const current = controller ? controller : m_socialButton.get();
    if (m_socialButtonController == current)
        return;
    m_socialButtonController = current;
    Q_EMIT buttonControllersChanged();
}

void TButtonBarController::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    m_options = options;
    if (!options)
        return;
    connect(options, &config::TSidebarOptions::sidebarPanelsOptionsChanged, this, &TButtonBarController::onSidebarPanelsOptionsChanged);
    onSidebarPanelsOptionsChanged();
}

void TButtonBarController::onDropdownButtonClicked()
{
    m_shortcutsVisible = !m_shortcutsVisible;
    if (m_options) {
        config::SidebarPanelsOptions o = m_options->sidebarPanelsOptions();
        o.controlButtonsVisible = m_shortcutsVisible;
        m_options->setSidebarPanelsOptions(o);
    }
    Q_EMIT shortcutsVisibleChanged();
}

void TButtonBarController::toggleWidget(const int widgetType)
{
    auto action = std::make_shared<input::TGameActionToggleWidget>(widgetType);
    Q_EMIT publishGameAction(action);
}

void TButtonBarController::onSkillButtonClicked() { toggleWidget(W::Skills); }
void TButtonBarController::onBattleListButtonClicked() { toggleWidget(W::BattleList); }
void TButtonBarController::onPartyListButtonClicked() { toggleWidget(W::PartyList); }
void TButtonBarController::onVipWidgetButtonClicked() { toggleWidget(W::Vip); }
void TButtonBarController::onQuestLogButtonClicked() { Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenQuestLog>(0)); }
void TButtonBarController::onQuestTrackerButtonClicked() { toggleWidget(W::QuestTracker); }
void TButtonBarController::onBestiaryTrackerButtonClicked() { toggleWidget(W::BestiaryTracker); }
void TButtonBarController::onBosstiaryTrackerButtonClicked() { toggleWidget(W::BosstiaryTracker); }
void TButtonBarController::onUnjustifiedPointsButtonClicked() { toggleWidget(W::UnjustifiedPoints); }
void TButtonBarController::onKillTrackerWidgetButtonClicked() { toggleWidget(W::Prey); }
void TButtonBarController::onPreyDialogButtonClicked() { Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_PREY_DIALOG)); }
void TButtonBarController::onSpellListWidgetButtonClicked() { toggleWidget(W::SpellList); }
void TButtonBarController::onOptionsButtonClicked()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCloseAllMultiActionButtons>());
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenOptionsMenuDialog>(0));
}

void TButtonBarController::onSwitchCharacterButtonClicked()
{
    Q_EMIT publishGameAction(input::makeGameAction(A::CHANGE_CHARACTER));
}

void TButtonBarController::onLogoutButtonClicked()
{
    m_logoutQuestion->setYesNoButtons();
    m_logoutQuestion->showMessage(qtTrId("warning"), qtTrId("buttonbar_logout_warning"));
}

void TButtonBarController::onLogoutRequestConfirmedOrCanceled(const TMessageDialogButton::EMessageDialogButtonIdentifier Button)
{
    if (Button == TMessageDialogButton::EMessageDialogButtonIdentifier::YES)
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionQuitGame>(true));
}

void TButtonBarController::onRewardWallButtonClicked() { Q_EMIT publishGameAction(input::makeGameAction(A::REQUEST_OPEN_REWARD_WALL)); }
void TButtonBarController::onAnalyticsSelectorWidgetButtonClicked() { toggleWidget(W::AnalyticsSelector); }
void TButtonBarController::onLenshelpButtonClicked() { Q_EMIT publishGameAction(input::makeGameAction(A::LENSHELP_ACTIVATE)); }
void TButtonBarController::onHighscoresDialogButtonClicked() { Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_HIGHSCORES_DIALOG)); }
void TButtonBarController::onExaltationForgeDialogButtonClicked() { Q_EMIT publishGameAction(input::makeGameAction(A::REQUEST_OPEN_EXALTATION_FORGE_DIALOG)); }
void TButtonBarController::onBossSlotsDialogButtonClicked() { Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenCyclopedia>(7)); }
void TButtonBarController::onSkillWheelDialogButtonClicked() { Q_EMIT publishGameAction(input::makeGameAction(A::REQUEST_OWN_SKILL_WHEEL)); }
void TButtonBarController::onImbuementTrackerWidgetClicked() { toggleWidget(W::ImbuementTracker); }
void TButtonBarController::onWeaponProficiencyDialogButtonClicked()
{
    uint16_t typeId = 0;
    if (container::TInventoryContainer* inventory = tibia::game::currentInventoryContainer()) {
        const container::TInventorySlot& left = inventory->leftHandSlot();
        if (!left.empty())
            typeId = static_cast<uint16_t>(left.typeId);
    }
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionOpenWeaponProficiencyDialog>(typeId));
}
void TButtonBarController::onTaskboardDialogButtonClicked()
{
    // Opens on the weekly results (tab 1) once this week's progress is finished.
    int tab = 0;
    if (game::TGameSessionBase* session = game::currentGameSession())
        if (const auto& weeklyTasks = session->taskboardWeeklyTasksStorage(); weeklyTasks && weeklyTasks->weeklyTaskData().progressFinished == 1)
            tab = 1;
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionOpenTaskboardDialog>(tab));
}
void TButtonBarController::onPlayerGuideWidgetClicked() { toggleWidget(W::PlayerGuide); }
void TButtonBarController::onManageShortcutsClicked() { Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenOptionsMenuDialog>(2)); }

QStringList TButtonBarController::enabledShortcutIds() const
{
    QStringList ids;
    const QJsonObject options = app::ClientOptions::load().root().value(QLatin1String("controlButtonsOptions")).toObject();
    for (const QJsonValue& value : options.value(QLatin1String("enabledButtons")).toArray()) {
        const QString name = value.toString();
        if (!name.isEmpty())
            ids.push_back(name);
    }
    return ids;
}

bool TButtonBarController::shortcutHighlighted(const QString& name) const
{
    const auto controllerOn = [](const QObject* controller) {
        return controller && controller->property("highlighted").toBool();
    };
    if (name == QLatin1String("rewardWallDialog"))
        return highlightRewardWallButton();
    if (name == QLatin1String("weaponProficiency"))
        return highlightWeaponProficiencyButton();
    if (name == QLatin1String("cyclopediaDialog"))
        return controllerOn(m_cyclopediaButtonController);
    if (name == QLatin1String("friendsDialog"))
        return controllerOn(m_socialButtonController);
    if (name == QLatin1String("bosstiaryDialog"))
        return controllerOn(m_bosstiaryButtonController);
    if (name == QLatin1String("compendiumDialog"))
        return controllerOn(m_newsButtonController);
    if (name == QLatin1String("skillWheelDialog"))
        return property("highlightSkillWheelButton").toBool();
    if (name == QLatin1String("exaltationForgeDialog"))
        return property("highlightExaltationForgeDialogButton").toBool();
    if (name == QLatin1String("taskboard"))
        return property("taskboardButtonHighlight").toBool();
    return false;
}

void TButtonBarController::activateShortcutByName(const QString& name)
{
    for (const auto& [buttonName, id] : ButtonNames) {
        if (name == QLatin1String(buttonName)) {
            activateShortcut(id);
            return;
        }
    }
    const int productId = productShortcutButtonId(name);
    if (productId >= 0)
        activateShortcut(productId);
}

void TButtonBarController::activateShortcut(const int buttonId)
{
    if (isProductShortcutButtonId(buttonId)) {
        onProductShortcutClicked(buttonId);
        return;
    }
    using B = Buttons;
    switch (buttonId) {
    case B::SKILLS_WIDGET:
        onSkillButtonClicked();
        break;
    case B::BATTLE_WIDGET:
        onBattleListButtonClicked();
        break;
    case B::PARTY_WIDGET:
        onPartyListButtonClicked();
        break;
    case B::VIP_WIDGET:
        onVipWidgetButtonClicked();
        break;
    case B::SPELLS_WIDGET:
        onSpellListWidgetButtonClicked();
        break;
    case B::SKILL_WHEEL_DIALOG:
        onSkillWheelDialogButtonClicked();
        break;
    case B::QUEST_DIALOG:
        onQuestLogButtonClicked();
        break;
    case B::QUEST_TRACKER_WIDGET:
        onQuestTrackerButtonClicked();
        break;
    case B::UNJUSTIFIED_WIDGET:
        onUnjustifiedPointsButtonClicked();
        break;
    case B::PREY_DIALOG:
        onPreyDialogButtonClicked();
        break;
    case B::KILL_TRACKER_WIDGET:
        onKillTrackerWidgetButtonClicked();
        break;
    case B::DAILY_REWARD_DIALOG:
        onRewardWallButtonClicked();
        break;
    case B::ANALYTICS_WIDGET:
        onAnalyticsSelectorWidgetButtonClicked();
        break;
    case B::COMPENDIUM_DIALOG:
        if (m_newsButtonController)
            QMetaObject::invokeMethod(m_newsButtonController, "onOpenNewsButtonClicked");
        else
            Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenNewsDialog>());
        break;
    case B::CYCLOPEDIA_DIALOG:
        if (m_cyclopediaButtonController)
            QMetaObject::invokeMethod(m_cyclopediaButtonController, "onCyclopediaButtonClicked");
        break;
    case B::BOSSTIARY_DIALOG:
        if (m_bosstiaryButtonController)
            QMetaObject::invokeMethod(m_bosstiaryButtonController, "onOpenBosstiaryButtonClicked");
        break;
    case B::BOSSSLOTS_DIALOG:
        onBossSlotsDialogButtonClicked();
        break;
    case B::BOSSTIARY_TRACKER_WIDGET:
        onBosstiaryTrackerButtonClicked();
        break;
    case B::BESTIARY_TRACKER_WIDGET:
        onBestiaryTrackerButtonClicked();
        break;
    case B::IMBUEMENT_TRACKER_WIDGET:
        onImbuementTrackerWidgetClicked();
        break;
    case B::WEAPON_PROFICIENCY_WIDGET:
        onWeaponProficiencyDialogButtonClicked();
        break;
    case B::EXALTATION_FORGE_DIALOG:
        onExaltationForgeDialogButtonClicked();
        break;
    case B::SOCIAL_DIALOG:
        if (m_socialButtonController)
            QMetaObject::invokeMethod(m_socialButtonController, "onSocialButtonClicked");
        break;
    case B::LENSHELP:
        onLenshelpButtonClicked();
        break;
    case B::HIGHSCORES_DIALOG:
        onHighscoresDialogButtonClicked();
        break;
    case B::PLAYER_GUIDE:
        onPlayerGuideWidgetClicked();
        break;
    case B::MANAGE_SHORTCUTS:
        onManageShortcutsClicked();
        break;
    case B::TASKBOARD_DIALOG:
        onTaskboardDialogButtonClicked();
        break;
    default:
        break;
    }
}

void TButtonBarController::onShowContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("buttonbarcontroller_contextmenu_manage_shortcuts")), &TContextMenuEntryItem::triggered, this,
            &TButtonBarController::onManageShortcutsClicked);
    menu->showCustomMenu();
}

void TButtonBarController::setWidgetVisible(const int widgetType, const bool visible)
{
    bool* flag = nullptr;
    switch (widgetType) {
        case W::Skills: flag = &m_skillWidgetVisible; break;
        case W::BattleList: flag = &m_battleListVisible; break;
        case W::PartyList: flag = &m_partyListVisible; break;
        case W::Vip: flag = &m_vipWidgetVisible; break;
        case W::UnjustifiedPoints: flag = &m_unjustifiedWidgetVisible; break;
        case W::Prey: flag = &m_killTrackerWidgetVisible; break;
        case W::SpellList: flag = &m_spellListWidgetVisible; break;
        case W::QuestTracker: flag = &m_questTrackerVisible; break;
        case W::BestiaryTracker: flag = &m_bestiaryTrackerVisible; break;
        case W::BosstiaryTracker: flag = &m_bosstiaryTrackerVisible; break;
        case W::AnalyticsSelector: flag = &m_analyticsSelectorWidgetVisible; break;
        case W::ImbuementTracker: flag = &m_imbuementTrackerVisible; break;
        case W::PlayerGuide: flag = &m_playerGuideWidgetVisible; break;
        default: return;
    }
    if (*flag == visible)
        return;
    *flag = visible;
    Q_EMIT buttonDownChanged();
}

void TButtonBarController::onWidgetVisible(const int widgetType) { setWidgetVisible(widgetType, true); }
void TButtonBarController::onWidgetHidden(const int widgetType) { setWidgetVisible(widgetType, false); }

}
