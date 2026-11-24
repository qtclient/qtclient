#include "storeandresourcebalancehelpercontroller.h"

#include "config/characterconfigurationmanager.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "exaltation/exaltationstorage.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "gamewindow/preydialogcontroller.h"
#include "proficiency/proficiencystorage.h"
#include "skillwheel/skillwheelstorage.h"
#include "store/qmlstoreobjects.h"
#include "taskboard/taskboardcurrenciesstorage.h"

#include <QLocale>
#include <QQmlContext>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

game::TGameSessionBase* sessionBase(const std::shared_ptr<game::IGameSession>& session)
{
    return dynamic_cast<game::TGameSessionBase*>(session.get());
}

}

TStoreAndResourceBalanceHelperController::TStoreAndResourceBalanceHelperController(QObject* parent)
    : cip::tibia::gamewindow::TStoreAndResourceBalanceHelperController(parent)
{
    m_storeBalance = new store::TQmlStoreBalance(std::nullopt, false, this);
    m_exaltedDustMaximum = 100;
}

void TStoreAndResourceBalanceHelperController::registerInRootContext(QQmlContext* RootContext)
{
    RootContext->setContextProperty(QStringLiteral("storeAndResourceBalanceHelper"), this);
}

void TStoreAndResourceBalanceHelperController::connectToGameSession(std::shared_ptr<game::IGameSession> GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_gameSession = std::move(GameSession);
    m_skillWheelStorage = nullptr;
    game::TGameSessionBase* session = sessionBase(m_gameSession);
    if (!session)
        return;
    if (session->characterConfigurationManager())
        m_skillWheelStorage = session->characterConfigurationManager()->skillWheelStorage();
    using Self = TStoreAndResourceBalanceHelperController;
    const auto add = [this](const QMetaObject::Connection& connection) { m_gameSessionConnections.add(connection); };
    add(connect(session->playerData().get(), &game::TPlayerData::playerDataChanged, this, &Self::onGoldBalanceChanged));
    add(connect(session->storeStorage().get(), &store::TStoreStorage::balanceChanged, this, &Self::onStoreBalanceChanged));
    add(connect(session->storeStorage().get(), &store::TStoreStorage::balanceUpdatingChanged, this, &Self::onStoreBalanceChanged));
    add(connect(session->preyStorage().get(), &prey::TPreyStorage::preyWildcardAmountChanged, this, &Self::onPreyWildcardsChanged));
    add(connect(session->taskboardCurrenciesStorage().get(), &taskboard::TTaskboardCurrenciesStorage::preyHuntingTaskTokensAmountChanged,
                this, &Self::onPreyHuntingTaskTokensChanged));
    add(connect(session->exaltationStorage().get(), &exaltation::TExaltationStorage::exaltedCurrencyBalanceChanged, this,
                &Self::onExaltedCurrencyBalanceChanged));
    add(connect(session->exaltationStorage().get(), &exaltation::TExaltationStorage::currentDustLimitChanged, this,
                &Self::onExaltedCurrencyBalanceChanged));
    if (m_skillWheelStorage) {
        add(connect(m_skillWheelStorage, &skillwheel::TSkillWheelStorage::lesserFragmentsChanged, this, &Self::onLesserFragmentsChanged));
        add(connect(m_skillWheelStorage, &skillwheel::TSkillWheelStorage::greaterFragmentsChanged, this, &Self::onGreaterFragmentsChanged));
    }
    // Host: the proficiency storage is the client's (CipSoft: the session's).
    if (proficiency::TProficiencyStorage* proficiencyStorage = proficiency::TProficiencyStorage::instance())
        add(connect(proficiencyStorage, &proficiency::TProficiencyStorage::resourceBalanceChanged, this,
                    &Self::onLunarAscensionOrbsAmountChanged));
    add(connect(session->dailyRewardStorage().get(), &dailyreward::TDailyRewardStorage::cosmeticTokenCountChanged, this,
                &Self::onCosmeticTokenCountChanged));
    onGoldBalanceChanged();
    onStoreBalanceChanged();
    onPreyWildcardsChanged();
    onPreyHuntingTaskTokensChanged();
    onExaltedCurrencyBalanceChanged();
    onLesserFragmentsChanged();
    onGreaterFragmentsChanged();
    onLunarAscensionOrbsAmountChanged();
    onCosmeticTokenCountChanged();
}

bool TStoreAndResourceBalanceHelperController::canAffordWithTotalGoldBalance(const int Price)
{
    return static_cast<qulonglong>(std::max(0, Price)) <= m_bankGoldBalance + m_inventoryGoldBalance;
}

qulonglong TStoreAndResourceBalanceHelperController::totalGoldBalance() const
{
    return m_bankGoldBalance + m_inventoryGoldBalance;
}

QString TStoreAndResourceBalanceHelperController::totalGoldBalanceString() const
{
    return QLocale(QLocale::English).toString(totalGoldBalance());
}

QString TStoreAndResourceBalanceHelperController::bankGoldBalanceString() const
{
    return QLocale(QLocale::English).toString(m_bankGoldBalance);
}

QString TStoreAndResourceBalanceHelperController::inventoryGoldBalanceString() const
{
    return QLocale(QLocale::English).toString(m_inventoryGoldBalance);
}

void TStoreAndResourceBalanceHelperController::onGoldBalanceChanged()
{
    game::TGameSessionBase* session = sessionBase(m_gameSession);
    if (!session) {
        m_bankGoldBalance = 0;
        m_inventoryGoldBalance = 0;
        m_guildBankGoldBalance = 0;
        Q_EMIT goldBalanceChanged();
        return;
    }
    const game::TPlayerData& data = *session->playerData();
    const auto bank = static_cast<qulonglong>(data.bankGold());
    const auto inventory = static_cast<qulonglong>(data.inventoryGold());
    const auto guild = static_cast<qulonglong>(data.guildBankGold());
    const bool hasGuild = data.isGuildBankGoldKnown();
    bool changed = false;
    if (m_bankGoldBalance != bank) {
        m_bankGoldBalance = bank;
        changed = true;
    }
    if (m_inventoryGoldBalance != inventory) {
        m_inventoryGoldBalance = inventory;
        changed = true;
    }
    if (m_guildBankGoldBalance != guild) {
        m_guildBankGoldBalance = guild;
        changed = true;
    }
    if (m_hasGuildBankGoldBalance != hasGuild) {
        m_hasGuildBankGoldBalance = hasGuild;
        changed = true;
    }
    if (changed)
        Q_EMIT goldBalanceChanged();
}

void TStoreAndResourceBalanceHelperController::onStoreBalanceChanged()
{
    game::TGameSessionBase* session = sessionBase(m_gameSession);
    if (!session)
        return;
    const store::TStoreStorage& storage = *session->storeStorage();
    const std::optional<store::TStoreBalance> balance = storage.balance();
    if (!store::balanceDiffers(m_storeBalanceValue, balance))
        return;
    m_storeBalanceValue = balance;
    QObject* previous = m_storeBalance;
    m_storeBalance = new store::TQmlStoreBalance(m_storeBalanceValue, storage.balanceUpdating(), this);
    // Host: QML may still read the old object in this pass.
    if (previous)
        previous->deleteLater();
    Q_EMIT storeBalanceChanged();
}

void TStoreAndResourceBalanceHelperController::onPreyWildcardsChanged()
{
    game::TGameSessionBase* session = sessionBase(m_gameSession);
    if (!session) {
        m_preyWildcards = 0;
        Q_EMIT preyWildcardsChanged();
        return;
    }
    const int wildcards = session->preyStorage()->preyWildcards();
    if (m_preyWildcards == wildcards)
        return;
    m_preyWildcards = wildcards;
    Q_EMIT preyWildcardsChanged();
}

void TStoreAndResourceBalanceHelperController::onPreyHuntingTaskTokensChanged()
{
    game::TGameSessionBase* session = sessionBase(m_gameSession);
    if (!session) {
        m_preyHuntingTaskTokens = 0;
        Q_EMIT preyHuntingTaskTokensChanged();
        return;
    }
    const int tokens = session->taskboardCurrenciesStorage()->preyHuntingTaskTokensAmount();
    if (m_preyHuntingTaskTokens == tokens)
        return;
    m_preyHuntingTaskTokens = tokens;
    Q_EMIT preyHuntingTaskTokensChanged();
}

void TStoreAndResourceBalanceHelperController::onExaltedCurrencyBalanceChanged()
{
    game::TGameSessionBase* session = sessionBase(m_gameSession);
    if (!session) {
        m_exaltedDustBalance = 0;
        m_exaltedDustMaximum = 100;
        m_exaltedSliversCount = 0;
        m_exaltedCoreCount = 0;
        Q_EMIT exaltedCurrencyBalanceChanged();
        return;
    }
    const exaltation::TExaltationStorage& exaltation = *session->exaltationStorage();
    const auto update = [](int& member, const int value) {
        if (member == value)
            return false;
        member = value;
        return true;
    };
    const bool dust = update(m_exaltedDustBalance, exaltation.dustBalance());
    const bool maximum = update(m_exaltedDustMaximum, exaltation.currentDustLimit());
    const bool slivers = update(m_exaltedSliversCount, exaltation.sliversBalance());
    const bool cores = update(m_exaltedCoreCount, exaltation.coresBalance());
    if (dust || maximum || slivers || cores)
        Q_EMIT exaltedCurrencyBalanceChanged();
}

void TStoreAndResourceBalanceHelperController::onLesserFragmentsChanged()
{
    if (!m_gameSession || !m_skillWheelStorage)
        return;
    const int fragments = m_skillWheelStorage->lesserFragments();
    if (fragments == m_lesserFragmentCount)
        return;
    m_lesserFragmentCount = fragments;
    Q_EMIT fragmentBalanceChanged();
}

void TStoreAndResourceBalanceHelperController::onGreaterFragmentsChanged()
{
    if (!m_gameSession || !m_skillWheelStorage)
        return;
    const int fragments = m_skillWheelStorage->greaterFragments();
    if (fragments == m_greaterFragmentCount)
        return;
    m_greaterFragmentCount = fragments;
    Q_EMIT fragmentBalanceChanged();
}

void TStoreAndResourceBalanceHelperController::onLunarAscensionOrbsAmountChanged()
{
    const proficiency::TProficiencyStorage* proficiencyStorage = proficiency::TProficiencyStorage::instance();
    if (!m_gameSession || !proficiencyStorage)
        return;
    const auto orbs = static_cast<uint>(proficiencyStorage->lunarAscensionOrbs());
    if (orbs == m_lunarAscensionOrbsAmount)
        return;
    m_lunarAscensionOrbsAmount = orbs;
    Q_EMIT lunarAscensionOrbsAmountChanged();
}

void TStoreAndResourceBalanceHelperController::onCosmeticTokenCountChanged()
{
    game::TGameSessionBase* session = sessionBase(m_gameSession);
    if (!session)
        return;
    const auto tokens = static_cast<uint>(session->dailyRewardStorage()->cosmeticTokenCount());
    if (tokens == m_cosmeticTokenCount)
        return;
    m_cosmeticTokenCount = tokens;
    Q_EMIT cosmeticTokenCountChanged();
}

}
