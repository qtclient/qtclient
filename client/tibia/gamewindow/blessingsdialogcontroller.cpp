#include "blessingsdialogcontroller.h"

#include "input/gameactionhandlers.h"

#include <client/staticdata.h>

#include <QDateTime>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

/// CIP .
const char* blessingNameId(const int id)
{
    switch (id) {
        case 0x1: return "blessing_adventurers_blessing";
        case 0x2: return "blessing_twist_of_fate";
        case 0x4: return "blessing_wisdom_of_solitude";
        case 0x8: return "blessing_spark_of_the_phoenix";
        case 0x10: return "blessing_fire_of_the_suns";
        case 0x20: return "blessing_spritual_shielding";
        case 0x40: return "blessing_embrace_of_tibia";
        case 0x80: return "blessing_heart_of_the_mountain";
        case 0x100: return "blessing_blood_of_the_mountain";
        default: return "dummy_unknown";
    }
}

/// CIP : the cell of the "blessings-icons" strip.
int blessingIconIndex(const int id)
{
    switch (id) {
        case 0x2: return 8;
        case 0x4: return 3;
        case 0x8: return 5;
        case 0x10: return 2;
        case 0x20: return 4;
        case 0x40: return 1;
        case 0x80: return 6;
        case 0x100: return 7;
        default: return 0;
    }
}

/// CIP : the store offer of a blessing.
int blessingStoreOffer(const int id)
{
    switch (id) {
        case 0x2: return 12;
        case 0x4: return 4;
        case 0x8: return 5;
        case 0x10: return 6;
        case 0x20: return 7;
        case 0x40: return 8;
        case 0x80: return 9;
        case 0x100: return 10;
        default: return 0;
    }
}

QString lossText(const char* single, const char* range, const int first, const int second)
{
    return first == second ? qtTrId(single).arg(first) : qtTrId(range).arg(first).arg(second);
}

/// CIP : the server sends one equipment loss byte per PvP / PvE; it is the backpack loss and a
/// tenth of it the items loss, 0xff meaning 100 for both.
int backpackLoss(const int value)
{
    return value == 0xff ? 100 : value;
}

int itemsLoss(const int value)
{
    return value == 0xff ? 100 : value / 10;
}

bool localPlayerPremium()
{
    return tibia::game::localPlayer() && tibia::game::localPlayerIsPremium();
}

}

TQmlBlessingWithCharges::TQmlBlessingWithCharges(const int id, const int charges, const int chargesFromStore, QObject* parent)
    : cip::tibia::game::TQmlBlessingWithCharges(parent)
{
    m_blessingId = id;
    m_blessingName = qtTrId(blessingNameId(id));
    m_blessingImage = QStringLiteral("image://blessings-icons/%1").arg(blessingIconIndex(id));
    m_blessingCharges = charges;
    m_blessingChargesFromStore = chargesFromStore;
}

TQmlBlessingHistoryEntry::TQmlBlessingHistoryEntry(const quint32 timestamp, const int type, const QString& description, QObject* parent)
    : cip::tibia::game::TQmlBlessingHistoryEntry(parent)
{
    m_timestamp = QDateTime::fromSecsSinceEpoch(timestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
    m_blessingWasGained = type != 0 && type != 5;
    m_description = description;
}

TQmlBlessingDialogData::TQmlBlessingDialogData(const BlessDialogData& data, const bool premium, QObject* parent)
    : cip::tibia::game::TQmlBlessingDialogData(parent)
    , m_promoted(data.premium != 0)
    , m_promotionReduction(data.promotion)
{
    m_isPremium = premium;
    for (const BlessData& bless : data.blesses)
        m_blessingsWithCharges.append(new TQmlBlessingWithCharges(bless.blessBitwise, bless.playerBlessCount, bless.store, this));
    for (const LogData& log : data.logs)
        m_blessingHistory.append(new TQmlBlessingHistoryEntry(log.timestamp, log.colorMessage, QString::fromStdString(log.historyMessage), this));
    m_promotionLossReductionText = promotionText();

    m_deathPenaltyText.append(lossText("blessings_dialog_pvp_death_single_chance", "blessings_dialog_pvp_death_range_chance", data.pvpMinXpLoss, data.pvpMaxXpLoss));
    m_deathPenaltyText.append(qtTrId("blessings_dialog_pve_death").arg(data.pveExpLoss));
    m_deathPenaltyText.append(lossText("blessings_dialog_backpack_loss", "blessings_dialog_backpack_loss_pvp_pve", backpackLoss(data.equipPvpLoss),
                                       backpackLoss(data.equipPveLoss)));
    m_deathPenaltyText.append(
        lossText("blessings_dialog_items_loss", "blessings_dialog_items_loss_pvp_pve", itemsLoss(data.equipPvpLoss), itemsLoss(data.equipPveLoss)));
    const bool adventurers = std::any_of(data.blesses.begin(), data.blesses.end(), [](const BlessData& bless) { return bless.blessBitwise == 1 && bless.playerBlessCount > 0; });
    if (adventurers)
        m_deathPenaltyText.append(qtTrId("blessings_dialog_adventurers_blessing"));
    if (data.aol)
        m_deathPenaltyText.append(qtTrId(data.skull ? "blessings_dialog_amulet_of_loss_disabled_by_skull" : "blessings_dialog_amulet_of_loss"));
    if (data.skull)
        m_deathPenaltyText.append(qtTrId("blessings_dialog_has_skull"));
}

void TQmlBlessingDialogData::setPremium(const bool premium)
{
    if (premium == m_isPremium)
        return;
    m_isPremium = premium;
    m_promotionLossReductionText = promotionText();
    Q_EMIT premiumStatusChanged();
}

QString TQmlBlessingDialogData::promotionText() const
{
    const char* id = m_isPremium ? (m_promoted ? "blessings_dialog_premium_promoted" : "blessings_dialog_premium_unpromoted")
                                 : (m_promoted ? "blessings_dialog_nopremium_promoted" : "blessings_dialog_nopremium_unpromoted");
    return qtTrId(id).arg(m_promotionReduction);
}

TBlessingsDialogController::TBlessingsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("BlessingsDialog.qml"), parent)
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onUpdateBlessDialog", [this](GameObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<BlessDialogData>(&args[0]))
            setData(*data);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        onPremiumStatusChanged();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        dialog()->destroy();
    });
}

void TBlessingsDialogController::setData(const BlessDialogData& data)
{
    QObject* previous = m_blessingsData;
    m_blessingsData = new TQmlBlessingDialogData(data, localPlayerPremium(), this);
    dialog()->destroy();
    if (previous)
        previous->deleteLater();
    dialog()->show();
}

void TBlessingsDialogController::closeDialog() { dialog()->close(); }

void TBlessingsDialogController::onPremiumStatusChanged()
{
    if (auto* data = qobject_cast<TQmlBlessingDialogData*>(m_blessingsData))
        data->setPremium(localPlayerPremium());
}

void TBlessingsDialogController::requestOpenStoreWithPremiumCategory()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

void TBlessingsDialogController::requestOpenStoreWithBlessingOffer(const int BlessingId)
{
    const int offer = blessingStoreOffer(BlessingId);
    if (!offer)
        return;
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 3;
    action->instance = offer;
    input::publish(action);
}

void TBlessingsDialogController::requestClose() { dialog()->close(); }

}
