#include "buttonbarcontroller.h"

#include "app/clientoptions.h"
#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/sidebarwidgetsmanager.h"
#include "mods/attention_backend.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <client/const.h>

#include <QJsonArray>
#include <QMetaObject>
#include <QStringList>
#include <tibia/game/gamesession.h>

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

void publish(const A type, const int parameter = 0)
{
    auto action = input::mutableClone(*input::makeAction(type));
    action->parameter = parameter;
    input::publish(action);
}

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

TButtonBarController::TButtonBarController(QObject* parent)
    : cip::tibia::gamewindow::TButtonBarController(parent)
{
    m_buttonsModel = &m_model;
    // Replaced by TGameWindowController::newsButton() via setNewsButtonController.
    m_newsButtonController = new cip::tibia::gamewindow::TNewsButtonController(this);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("controlButtonsOptions"))
            m_model.reload();
    });
    const auto setRewardWallHighlight = [this](const bool on) {
        if (m_highlightRewardWallButton == on)
            return;
        m_highlightRewardWallButton = on;
        Q_EMIT highlightRewardWallButtonChanged();
    };
    input::setActionHandler(input::EGameAction::DAILY_REWARD_READY_TO_PICK_UP, [setRewardWallHighlight](const input::TGameAction&) {
        setRewardWallHighlight(true);
        return true;
    });
    input::setActionHandler(input::EGameAction::DAILY_REWARD_NOT_COLLECTABLE, [setRewardWallHighlight](const input::TGameAction&) {
        setRewardWallHighlight(false);
        return true;
    });
}

TButtonBarController::~TButtonBarController()
{
    input::setActionHandler(input::EGameAction::DAILY_REWARD_READY_TO_PICK_UP, nullptr);
    input::setActionHandler(input::EGameAction::DAILY_REWARD_NOT_COLLECTABLE, nullptr);
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

bool TButtonBarController::highlightRewardWallButton() const
{
    return m_highlightRewardWallButton || (m_attention && m_attention->rewardWall());
}

bool TButtonBarController::highlightWeaponProficiencyButton() const
{
    return m_highlightWeaponProficiencyButton || (m_attention && m_attention->proficiency());
}

void TButtonBarController::setButtonController(QObject*& member, QObject* controller)
{
    if (member == controller)
        return;
    if (member && member->parent() == this)
        member->deleteLater();
    member = controller;
    Q_EMIT buttonControllersChanged();
}

void TButtonBarController::setStoreButtonController(QObject* controller) { setButtonController(m_storeButtonController, controller); }
void TButtonBarController::setNewsButtonController(QObject* controller) { setButtonController(m_newsButtonController, controller); }
void TButtonBarController::setSocialButtonController(QObject* controller) { setButtonController(m_socialButtonController, controller); }
void TButtonBarController::setCyclopediaButtonController(QObject* controller)
{
    setButtonController(m_cyclopediaButtonController, controller);
}
void TButtonBarController::setBosstiaryButtonController(QObject* controller)
{
    setButtonController(m_bosstiaryButtonController, controller);
}

void TButtonBarController::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    m_options = options;
    if (!options)
        return;
    const auto apply = [this] {
        if (m_options && m_options->sidebarPanelsOptions().controlButtonsVisible != m_shortcutsVisible)
            setShortcutsVisible(m_options->sidebarPanelsOptions().controlButtonsVisible);
    };
    connect(options, &config::TSidebarOptions::sidebarPanelsOptionsChanged, this, apply);
    apply();
}

void TButtonBarController::onDropdownButtonClicked()
{
    setShortcutsVisible(!m_shortcutsVisible);
    if (m_options) {
        config::SidebarPanelsOptions o = m_options->sidebarPanelsOptions();
        o.controlButtonsVisible = m_shortcutsVisible;
        m_options->setSidebarPanelsOptions(o);
    }
}

void TButtonBarController::toggleWidget(const int widgetType)
{
    auto action = input::mutableClone(*input::makeAction(A::TOGGLE_WIDGET));
    action->instance = widgetType;
    input::publish(action);
}

void TButtonBarController::onSkillButtonClicked() { toggleWidget(W::Skills); }
void TButtonBarController::onBattleListButtonClicked() { toggleWidget(W::BattleList); }
void TButtonBarController::onPartyListButtonClicked() { toggleWidget(W::PartyList); }
void TButtonBarController::onVipWidgetButtonClicked() { toggleWidget(W::Vip); }
void TButtonBarController::onQuestLogButtonClicked() { publish(A::OPEN_QUESTLOG_DIALOG); }
void TButtonBarController::onQuestTrackerButtonClicked() { toggleWidget(W::QuestTracker); }
void TButtonBarController::onBestiaryTrackerButtonClicked() { toggleWidget(W::BestiaryTracker); }
void TButtonBarController::onBosstiaryTrackerButtonClicked() { toggleWidget(W::BosstiaryTracker); }
void TButtonBarController::onUnjustifiedPointsButtonClicked() { toggleWidget(W::UnjustifiedPoints); }
void TButtonBarController::onKillTrackerWidgetButtonClicked() { toggleWidget(W::Prey); }
void TButtonBarController::onPreyDialogButtonClicked() { publish(A::OPEN_PREY_DIALOG); }
void TButtonBarController::onSpellListWidgetButtonClicked() { toggleWidget(W::SpellList); }
void TButtonBarController::onOptionsButtonClicked()
{
    publish(A::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS);
    publish(A::OPEN_OPTIONS_MENU_DIALOG, 0);
}

void TButtonBarController::onSwitchCharacterButtonClicked()
{
    publish(A::CHANGE_CHARACTER);
}

void TButtonBarController::onLogoutButtonClicked()
{
    if (!m_logoutQuestion) {
        m_logoutQuestion = new TMessageDialogController(this);
        m_logoutQuestion->setCloseOnButtonClicked(true);
        m_logoutQuestion->setYesNoButtons();
        connect(m_logoutQuestion, &TMessageDialogController::acceptClicked, this, &TButtonBarController::logoutRequested);
    }
    m_logoutQuestion->showMessage(qtTrId("warning"), qtTrId("buttonbar_logout_warning"));
}

void TButtonBarController::onRewardWallButtonClicked() { publish(A::REQUEST_OPEN_REWARD_WALL); }
void TButtonBarController::onAnalyticsSelectorWidgetButtonClicked() { toggleWidget(W::AnalyticsSelector); }
void TButtonBarController::onLenshelpButtonClicked() { publish(A::LENSHELP_ACTIVATE); }
void TButtonBarController::onHighscoresDialogButtonClicked() { publish(A::OPEN_HIGHSCORES_DIALOG); }
void TButtonBarController::onExaltationForgeDialogButtonClicked() { publish(A::REQUEST_OPEN_EXALTATION_FORGE_DIALOG); }
void TButtonBarController::onBossSlotsDialogButtonClicked() { publish(A::OPEN_CYCLOPEDIA, 7); }
void TButtonBarController::onSkillWheelDialogButtonClicked() { publish(A::REQUEST_OWN_SKILL_WHEEL); }
void TButtonBarController::onImbuementTrackerWidgetClicked() { toggleWidget(W::ImbuementTracker); }
void TButtonBarController::onWeaponProficiencyDialogButtonClicked()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_WEAPON_PROFICIENCY_DIALOG));
    if (container::TInventoryContainer* inventory = container::TInventoryContainer::instance()) {
        const container::TInventorySlot& left = inventory->slotAt(tibia::client::InventorySlotLeft);
        if (!left.empty()) {
            action->objectClientId = left.typeId;
            action->instance = left.typeId;
        }
    }
    input::publish(action);
}
void TButtonBarController::onTaskboardDialogButtonClicked() { publish(A::OPEN_TASKBOARD_DIALOG); }
void TButtonBarController::onPlayerGuideWidgetClicked() { toggleWidget(W::PlayerGuide); }
void TButtonBarController::onManageShortcutsClicked() { publish(A::OPEN_OPTIONS_MENU_DIALOG, 2); }

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
            input::publish(input::makeAction(input::EGameAction::OPEN_NEWS_DIALOG));
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
