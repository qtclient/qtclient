#include "proficiency/weaponproficiencyshapedialogs.h"

#include "exaltation/exaltationstorage.h"
#include "game/gamesessionbase.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "proficiency/proficiencycatalogue.h"
#include "proficiency/proficiencyperks.h"

#include <array>
#include <tuple>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::proficiency {

namespace {

using Args = std::vector<std::any>;
using A = input::EGameAction;

constexpr int MaxRank = 10;
constexpr int ReshapeOfferCount = 3;
constexpr int ReshapeDustCost = 250;
constexpr int MaximiseOrbCost = 1;
/// CIP 's REQUEST_RESOURCE_BALANCE resources: dust and lunar ascension orbs.
constexpr int DustResource = 70;
constexpr int LunarAscensionOrbsResource = 73;
constexpr std::array<int, 2> DialogResources { DustResource, LunarAscensionOrbsResource };
/// CIP : the reshape offer index that keeps the current perk.
constexpr int KeepCurrentPerk = 3;

int dustBalance()
{
    return tibia::game::hasLocalPlayer()
               ? static_cast<int>(tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_FORGE_DUST))
               : 0;
}

uint32_t lunarAscensionOrbs()
{
    return tibia::game::hasLocalPlayer()
               ? static_cast<uint32_t>(tibia::game::localPlayerResourceBalance(
                     static_cast<tibia::client::ResourceTypes_t>(LunarAscensionOrbsResource)))
               : 0;
}

/// CIP .
QString formatReasons(const QStringList& reasons)
{
    QString text = qtTrId("weapon_proficiency_action_not_possible_because") + QStringLiteral("<br>\n");
    for (const QString& reason : reasons)
        text += (qtTrId("bullet_point") + QStringLiteral("<br/>")).arg(reason);
    if (text.endsWith(QStringLiteral("<br/>"), Qt::CaseInsensitive))
        text.chop(5);
    return text;
}

QString newValuesText(const TShapedSlot& slot, const int rank)
{
    return qtTrId("weapon_proficiency_shape_dialog_new_values").arg(rank).arg(perkDescription(shapedPerk(slot.perkType, rank)));
}


int profession()
{
    return tibia::game::hasLocalPlayer() ? tibia::game::localPlayerVocation() % 10 : 0;
}

}

TWeaponProficiencyShapeDialogController::TWeaponProficiencyShapeDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                                                 QObject* parent)
    : TDialogController(QStringLiteral("WeaponProficiencyShapeDialog.qml"), engine, dialogParentItem, parent)
    // Host: CIP reads the proficiency storage from the game session; qtclient's
    // is the game client's, alive before any controller.
    , m_storage(TProficiencyStorage::instance())
    , m_message(new gamewindow::TMessageDialogController(this))
{
    m_message->setYesNoButtons();
    m_message->setCloseOnButtonClicked(true);
    m_message->setMessageDialogTextFormat(gamewindow::TMessageDialogController::RICH_TEXT);
    connect(m_message, &gamewindow::TMessageDialogController::acceptClicked, this,
            &TWeaponProficiencyShapeDialogController::onMessageDialogAccept);

    m_gameActions.on(A::OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG, [this](const input::gameactions::TGameActionOpenWeaponProficiencyShapeDialog& action) {
        handleGameAction(action);
        return true;
    });
}

/// CIP: OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG from the perk tree opens the
/// dialog for a shaped perk the weapon has.
void TWeaponProficiencyShapeDialogController::handleGameAction(const input::gameactions::TGameActionOpenWeaponProficiencyShapeDialog& action)
{
    const uint16_t typeId = action.typeId();
    const uint8_t level = action.level();
    const uint8_t perk = action.perk();
    const TProficiencyData* data = m_storage->data(typeId);
    if (!data || !action.fromPerkTree() || !data->shapedSlot(level, perk))
        return;
    m_typeId = typeId;
    m_level = level;
    m_perk = perk;
    m_fromPerkTree = action.fromPerkTree();
    showDialog();
}

TWeaponProficiencyShapeDialogController::~TWeaponProficiencyShapeDialogController() { closeDialog(); }

/// The shown perk and the slot are forgotten; refine, maximise and reshape
/// are reset (not possible, price -1, no texts). No change signal.
void TWeaponProficiencyShapeDialogController::resetShapeData()
{
    if (QObject* perk = std::exchange(m_selectedPerk, nullptr))
        perk->deleteLater();
    m_typeId = 0;
    m_level = 0;
    m_perk = 0;
    m_fromPerkTree = false;
    m_refinePossible = false;
    m_refinePrice = -1;
    m_refinePriceToHigh = false;
    m_refineHoverText.clear();
    m_refineTooltip.clear();
    m_refineGuiHelp.clear();
    m_maximisePossible = false;
    m_maximisePrice = -1;
    m_maximisePriceToHigh = false;
    m_maximiseHoverText.clear();
    m_maximiseTooltip.clear();
    m_maximiseGuiHelp.clear();
    m_reshapePossible = false;
    m_reshapePrice = -1;
    m_reshapePriceToHigh = false;
    m_reshapeTooltip.clear();
    m_reshapeGuiHelp.clear();
}

std::optional<TShapedSlot> TWeaponProficiencyShapeDialogController::shapedSlot() const
{
    const TProficiencyData* data = m_storage->data(m_typeId);
    if (!data || !m_fromPerkTree)
        return std::nullopt;
    const TShapedSlot* slot = data->shapedSlot(m_level, m_perk);
    return slot ? std::optional<TShapedSlot>(*slot) : std::nullopt;
}

void TWeaponProficiencyShapeDialogController::onProficiencyDataChanged(const uint16_t typeId)
{
    if (typeId != m_typeId)
        return;
    const std::optional<TShapedSlot> slot = shapedSlot();
    updateRefine(slot);
    updateMaximise(slot);
    updateReshape(slot);
    QObject* old = nullptr;
    if (slot) {
        old = std::exchange(m_selectedPerk, new TQMLProficiencyPerk(m_fromPerkTree ? m_perk : 0, shapedPerk(slot->perkType, slot->rank),
                                                                    true, true, slot->rank, this));
    }
    Q_EMIT perkInformationChanged();
    if (old)
        old->deleteLater();
}

void TWeaponProficiencyShapeDialogController::updateRefine(const std::optional<TShapedSlot>& slot)
{
    m_refinePossible = true;
    m_refinePrice = -1;
    m_refinePriceToHigh = false;
    m_refineHoverText.clear();
    m_refineTooltip.clear();
    m_refineGuiHelp.clear();
    QStringList reasons;
    if (!slot) {
        m_refinePossible = false;
        reasons << qtTrId("weapon_proficiency_no_perk_selected");
    } else if (slot->rank < MaxRank) {
        m_refinePrice = slot->rank * 75 + 125;
        if (dustBalance() < m_refinePrice) {
            m_refinePossible = false;
            m_refinePriceToHigh = true;
            reasons << qtTrId("weapon_proficiency_not_enough_dust");
        }
        m_refineHoverText = newValuesText(*slot, slot->rank + 1);
    } else {
        m_refinePossible = false;
        reasons << qtTrId("weapon_proficiency_shape_dialog_max_rank_reached");
    }
    if (!reasons.isEmpty())
        m_refineTooltip = formatReasons(reasons);
    m_refineGuiHelp = qtTrId("weapon_proficiency_shape_dialog_refine_general_information").arg(MaxRank);
}

void TWeaponProficiencyShapeDialogController::updateMaximise(const std::optional<TShapedSlot>& slot)
{
    m_maximisePossible = true;
    m_maximisePrice = -1;
    m_maximisePriceToHigh = false;
    m_maximiseHoverText.clear();
    m_maximiseTooltip.clear();
    m_maximiseGuiHelp.clear();
    QStringList reasons;
    if (!slot) {
        m_maximisePossible = false;
        reasons << qtTrId("weapon_proficiency_no_perk_selected");
    } else if (slot->rank < MaxRank) {
        m_maximisePrice = MaximiseOrbCost;
        if (lunarAscensionOrbs() < static_cast<uint32_t>(m_maximisePrice)) {
            m_maximisePossible = false;
            m_maximisePriceToHigh = true;
            reasons << qtTrId("weapon_proficiency_shape_dialog_not_enough_lunar_ascension_orbs");
        }
        m_maximiseHoverText = newValuesText(*slot, MaxRank);
    } else {
        m_maximisePossible = false;
        reasons << qtTrId("weapon_proficiency_shape_dialog_max_rank_reached");
    }
    if (!reasons.isEmpty())
        m_maximiseTooltip = formatReasons(reasons);
    m_maximiseGuiHelp = qtTrId("weapon_proficiency_shape_dialog_maximise_general_information").arg(MaxRank);
}

void TWeaponProficiencyShapeDialogController::updateReshape(const std::optional<TShapedSlot>& slot)
{
    m_reshapePossible = true;
    m_reshapePrice = -1;
    m_reshapePriceToHigh = false;
    m_reshapeTooltip.clear();
    m_reshapeGuiHelp.clear();
    QStringList reasons;
    if (!slot) {
        m_reshapePossible = false;
        reasons << qtTrId("weapon_proficiency_no_perk_selected");
    } else {
        m_reshapePrice = ReshapeDustCost;
        if (dustBalance() < m_reshapePrice) {
            m_reshapePossible = false;
            m_reshapePriceToHigh = true;
            reasons << qtTrId("weapon_proficiency_not_enough_dust");
        }
    }
    if (!reasons.isEmpty())
        m_reshapeTooltip = formatReasons(reasons);
    m_reshapeGuiHelp = qtTrId("weapon_proficiency_shape_dialog_reshape_general_information").arg(ReshapeOfferCount);
}

void TWeaponProficiencyShapeDialogController::showDialog()
{
    m_message->dialog()->close();
    onProficiencyDataChanged(m_typeId);
    for (const int resource : DialogResources)
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionRequestResourceBalance>(resource));
    if (!dialog()->item()) {
        dialog()->create();
        dialog()->show();
    }
}

void TWeaponProficiencyShapeDialogController::requestClose() { closeDialog(); }

void TWeaponProficiencyShapeDialogController::closeDialog()
{
    m_message->dialog()->close();
    dialog()->close();
    resetShapeData();
}

void TWeaponProficiencyShapeDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    if (!m_gameSession)
        return;
    // The session's proficiency storage is the client's.
    m_gameSessionConnections.add(connect(m_storage, &TProficiencyStorage::proficiencyDataChanged, this,
                                         [this](const uint typeId) { onProficiencyDataChanged(static_cast<uint16_t>(typeId)); }));
    m_gameSessionConnections.add(connect(m_storage, &TProficiencyStorage::resourceBalanceChanged, this,
                                         &TWeaponProficiencyShapeDialogController::onResourceBalanceChanged));
    // The forge dust the session's exaltation storage holds.
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get()))
        m_gameSessionConnections.add(connect(session->exaltationStorage().get(), &exaltation::TExaltationStorage::exaltedCurrencyBalanceChanged,
                                             this, &TWeaponProficiencyShapeDialogController::onResourceBalanceChanged));
    onProficiencyDataChanged(m_typeId);
    onProficiencyDataChanged(m_typeId);
}

void TWeaponProficiencyShapeDialogController::confirm(const EConfirm action, const QString& caption, const QString& text)
{
    m_pending = action;
    m_message->showMessage(caption + QLatin1Char('?'), text);
}

void TWeaponProficiencyShapeDialogController::onRefineClicked()
{
    confirm(EConfirm::Refine, qtTrId("weapon_proficiency_shape_dialog_refine"),
            qtTrId("weapon_proficiency_shape_dialog_confirm_refine_description").arg(m_refinePrice));
}

void TWeaponProficiencyShapeDialogController::onMaximiseClicked()
{
    confirm(EConfirm::Maximise, qtTrId("weapon_proficiency_shape_dialog_maximise"),
            qtTrId("weapon_proficiency_shape_dialog_confirm_maximise_description").arg(m_maximisePrice));
}

void TWeaponProficiencyShapeDialogController::onReshapeClicked()
{
    confirm(EConfirm::Reshape, qtTrId("weapon_proficiency_shape_dialog_reshape"),
            qtTrId("weapon_proficiency_shape_dialog_confirm_reshape_description").arg(m_reshapePrice));
}

void TWeaponProficiencyShapeDialogController::onClearClicked()
{
    confirm(EConfirm::Clear, qtTrId("clear"), qtTrId("weapon_proficiency_shape_dialog_confirm_clear_description"));
}

void TWeaponProficiencyShapeDialogController::sendSlotAction(const uint8_t type) const
{
    tibia::game::sendWeaponProficiencySlotAction(type, m_typeId, m_level, m_perk);
}

void TWeaponProficiencyShapeDialogController::onMessageDialogAccept()
{
    switch (m_pending) {
    case EConfirm::Refine: sendSlotAction(tibia::client::WEAPON_PROFICIENCY_REFINE_SLOT); break;
    case EConfirm::Maximise: sendSlotAction(tibia::client::WEAPON_PROFICIENCY_MAXIMISE_SLOT); break;
    case EConfirm::Reshape: sendSlotAction(tibia::client::WEAPON_PROFICIENCY_RESHAPE_SLOT); break;
    case EConfirm::Clear:
        sendSlotAction(tibia::client::WEAPON_PROFICIENCY_CLEAR_SLOT);
        closeDialog();
        break;
    }
}

void TWeaponProficiencyShapeDialogController::onResourceBalanceChanged() { onProficiencyDataChanged(m_typeId); }

void TWeaponProficiencyShapeDialogController::onShowShapeOptionsClicked()
{
    if (!tibia::game::isOnline())
        return;
    auto action = std::make_shared<input::gameactions::TGameActionOpenWeaponProficiencyShapeOptionsDialog>(profession());
    Q_EMIT publishGameAction(action);
}

TWeaponProficiencyReshapeDialogController::TWeaponProficiencyReshapeDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                                                     TWeaponProficiencyShapeDialogController* shape,
                                                                                     QObject* parent)
    : TDialogController(QStringLiteral("WeaponProficiencyReshapeDialog.qml"), engine, dialogParentItem, parent)
    // Host: the session's proficiency storage is the game client's.
    , m_storage(TProficiencyStorage::instance())
    , m_shape(shape)
    , m_message(new gamewindow::TMessageDialogController(this))
{
    m_message->setYesNoButtons();
    m_message->setCloseOnButtonClicked(true);
    m_message->setMessageDialogTextFormat(gamewindow::TMessageDialogController::RICH_TEXT);
    connect(m_message, &gamewindow::TMessageDialogController::acceptClicked, this,
            &TWeaponProficiencyReshapeDialogController::onMessageDialogAccept);
}

TWeaponProficiencyReshapeDialogController::~TWeaponProficiencyReshapeDialogController() { closeDialog(); }

void TWeaponProficiencyReshapeDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    if (!m_gameSession)
        return;
    // The session's proficiency storage is the client's.
    m_gameSessionConnections.add(connect(m_storage, &TProficiencyStorage::shapedPerkReshapeOffersChanged, this,
                                         &TWeaponProficiencyReshapeDialogController::onShapedPerkReshapeOffersChanged));
}

void TWeaponProficiencyReshapeDialogController::onShapedPerkReshapeOffersChanged()
{
    rebuildOffers();
    m_message->dialog()->hide();
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
}

QObject* TWeaponProficiencyReshapeDialogController::currentPerk() const { return m_shape->selectedPerk(); }

void TWeaponProficiencyReshapeDialogController::rebuildOffers()
{
    const QList<QObject*> old = std::exchange(m_perkOffersModel, {});
    if (const TReshapeOffers* offers = m_storage->reshapeOffers()) {
        for (const TShapedSlot& offer : offers->offers)
            m_perkOffersModel << new TQMLProficiencyPerk(offer.perk, shapedPerk(offer.perkType, offer.rank), false, true, offer.rank, this);
    }
    Q_EMIT perkOffersModelChanged();
    for (QObject* perk : old)
        perk->deleteLater();
}

void TWeaponProficiencyReshapeDialogController::pickOffer(const int index) const
{
    if (const TReshapeOffers* offers = m_storage->reshapeOffers())
        tibia::game::sendWeaponProficiencySlotAction(tibia::client::WEAPON_PROFICIENCY_PICK_OFFER, offers->typeId, offers->level, offers->perk, index);
}

void TWeaponProficiencyReshapeDialogController::requestCancel()
{
    pickOffer(KeepCurrentPerk);
    closeDialog();
}

void TWeaponProficiencyReshapeDialogController::replacePerkSelected(const int PerkIndex)
{
    m_selectedOffer = PerkIndex;
    m_message->showMessage(qtTrId("weapon_proficiency_reshape_dialog_caption") + QLatin1Char('?'),
                           qtTrId("weapon_proficiency_reshape_confirm_reshape_description"));
}

/// Only while the dialog is not there yet.
void TWeaponProficiencyReshapeDialogController::showDialog()
{
    m_message->dialog()->hide();
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
}

void TWeaponProficiencyReshapeDialogController::closeDialog()
{
    m_message->dialog()->close();
    dialog()->close();
}

void TWeaponProficiencyReshapeDialogController::onMessageDialogAccept()
{
    if (m_storage->reshapeOffers() && m_selectedOffer >= 0 && m_selectedOffer < KeepCurrentPerk)
        pickOffer(m_selectedOffer);
    closeDialog();
}

int TShapedPerkOptionsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_perks.size());
}

QVariant TShapedPerkOptionsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_perks.size()))
        return {};
    const TProficiencyPerk& perk = m_perks[index.row()];
    switch (role) {
    case PerkName: return perkName(perk);
    case ImageSource: return perkImageSource(perk).toString();
    case HasAugmentEffect: return perkHasAugmentEffect(perk);
    case AugmentEffectImageSource:
        return perkHasAugmentEffect(perk) ? QVariant(perkAugmentEffectImageSource(perk).toString()) : QVariant(QUrl());
    case AugmentSpellName:
        return perkHasAugmentEffect(perk) ? perkAugmentSpellName(perk) : QString();
    case AugmentType:
        return perkHasAugmentEffect(perk) ? perkAugmentTypeName(perk) : QString();
    case PerkDescriptionRank0: return perkDescription(perkAtRank(perk, 0));
    case PerkDescriptionRank10: return perkDescription(perkAtRank(perk, MaxRank));
    default: return {};
    }
}

QHash<int, QByteArray> TShapedPerkOptionsModel::roleNames() const
{
    return {
        { PerkName, "perkName" },
        { ImageSource, "imageSource" },
        { HasAugmentEffect, "hasAugmentEffect" },
        { AugmentEffectImageSource, "augmentEffectImageSource" },
        { AugmentSpellName, "augmentSpellName" },
        { AugmentType, "augmentType" },
        { PerkDescriptionRank0, "perkDescriptionRank0" },
        { PerkDescriptionRank10, "perkDescriptionRank10" },
    };
}

void TShapedPerkOptionsModel::setPerks(std::vector<TProficiencyPerk> perks)
{
    beginResetModel();
    m_perks = std::move(perks);
    endResetModel();
}

TShapedPerkOptionsSortFilterProxyModel::TShapedPerkOptionsSortFilterProxyModel(QObject* parent)
    : cip::TShapedPerkOptionsSortFilterProxyModel(parent)
{
    setSourceModel(&m_model);
    sort(0, Qt::AscendingOrder);
    connect(this, &TShapedPerkOptionsSortFilterProxyModel::filterChanged, this, [this] {
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    });
}

void TShapedPerkOptionsSortFilterProxyModel::setPerks(std::vector<TProficiencyPerk> perks)
{
    m_model.setPerks(std::move(perks));
    setFilterString(QString());
}

void TShapedPerkOptionsSortFilterProxyModel::setFilterString(const QString& filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    Q_EMIT filterChanged();
}

bool TShapedPerkOptionsSortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (m_filter.isEmpty())
        return true;
    const QModelIndex index = m_model.index(sourceRow);
    for (const int role : { TShapedPerkOptionsModel::PerkName, TShapedPerkOptionsModel::AugmentSpellName,
                            TShapedPerkOptionsModel::AugmentType, TShapedPerkOptionsModel::PerkDescriptionRank0 }) {
        if (m_model.data(index, role).toString().contains(m_filter, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool TShapedPerkOptionsSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto key = [this](const QModelIndex& index) {
        return std::make_tuple(m_model.data(index, TShapedPerkOptionsModel::PerkName).toString(),
                               m_model.data(index, TShapedPerkOptionsModel::AugmentSpellName).toString(),
                               m_model.data(index, TShapedPerkOptionsModel::AugmentType).toString(),
                               m_model.data(index, TShapedPerkOptionsModel::PerkDescriptionRank0).toString());
    };
    return key(left) < key(right);
}

TWeaponProficiencyShapeOptionsDialogController::TWeaponProficiencyShapeOptionsDialogController(QQmlEngine* engine,
                                                                                               QQuickItem* dialogParentItem,
                                                                                               QObject* parent)
    : TDialogController(QStringLiteral("WeaponProficiencyShapeOptionsDialog.qml"), engine, dialogParentItem, parent)
{
    m_perkOptionsModel = &m_proxy;
    m_gameActions.on(A::OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG,
                     [this](const input::gameactions::TGameActionOpenWeaponProficiencyShapeOptionsDialog& action) {
                         handleGameAction(action);
                         return true;
                     });
}

/// Only a profession 1..5 fills the options and opens the dialog.
void TWeaponProficiencyShapeOptionsDialogController::handleGameAction(
    const input::gameactions::TGameActionOpenWeaponProficiencyShapeOptionsDialog& action)
{
    const int profession = action.profession();
    if (profession < 1 || profession > 5)
        return;
    std::vector<TProficiencyPerk> perks;
    for (const int index : shapeOptions(profession)) {
        if (const std::optional<TProficiencyPerk> perk = shapePerk(index))
            perks.push_back(*perk);
    }
    m_proxy.setPerks(std::move(perks));
    dialog()->create();
    dialog()->show();
}

TWeaponProficiencyShapeOptionsDialogController::~TWeaponProficiencyShapeOptionsDialogController() { closeDialog(); }

void TWeaponProficiencyShapeOptionsDialogController::requestClose() { closeDialog(); }

void TWeaponProficiencyShapeOptionsDialogController::setFilterString(const QString& Filter) { m_proxy.setFilterString(Filter); }

/// Only while the dialog is not there yet.
void TWeaponProficiencyShapeOptionsDialogController::showDialog()
{
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
}

/// The options are dropped with the dialog.
void TWeaponProficiencyShapeOptionsDialogController::closeDialog()
{
    dialog()->close();
    m_proxy.setPerks({});
}

}
