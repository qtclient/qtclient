#include "proficiency/weaponproficiencyshapedialogs.h"

#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "proficiency/proficiencycatalogue.h"
#include "proficiency/proficiencyperks.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <array>
#include <tuple>

namespace tibia::proficiency {

namespace {

using Args = std::vector<std::any>;
using A = input::EGameAction;

constexpr int MaxRank = 10;
constexpr int ReshapeOfferCount = 3;
constexpr int ReshapeDustCost = 250;
constexpr int MaximiseOrbCost = 1;
/// CIP 0x100e2c4c0's REQUEST_RESOURCE_BALANCE resources: dust and lunar ascension orbs.
constexpr int DustResource = 70;
constexpr int LunarAscensionOrbsResource = 73;
constexpr std::array<int, 2> DialogResources { DustResource, LunarAscensionOrbsResource };
/// CIP FUN_100eebf70: the reshape offer index that keeps the current perk.
constexpr int KeepCurrentPerk = 3;

int dustBalance()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? static_cast<int>(player->getResourceBalance(Otc::RESOURCE_FORGE_DUST)) : 0;
}

uint32_t lunarAscensionOrbs()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? static_cast<uint32_t>(player->getResourceBalance(static_cast<Otc::ResourceTypes_t>(LunarAscensionOrbsResource))) : 0;
}

/// CIP FUN_100e2e500.
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

void requestBalances()
{
    for (const int resource : DialogResources) {
        auto action = input::mutableClone(*input::makeAction(A::REQUEST_RESOURCE_BALANCE));
        action->parameter = resource;
        input::publish(action);
    }
}

int profession()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getVocation() % 10 : 0;
}

}

TWeaponProficiencyShapeDialogController::TWeaponProficiencyShapeDialogController(TProficiencyStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("WeaponProficiencyShapeDialog.qml"), parent)
    , m_storage(storage)
    , m_message(new gamewindow::TMessageDialogController(this))
{
    m_message->setYesNoButtons();
    m_message->setCloseOnButtonClicked(true);
    m_message->setMessageDialogTextFormat(gamewindow::TMessageDialogController::RICH_TEXT);
    connect(m_message, &gamewindow::TMessageDialogController::acceptClicked, this,
            &TWeaponProficiencyShapeDialogController::onMessageDialogAccept);
    connect(m_storage, &TProficiencyStorage::proficiencyDataChanged, this, [this](const uint typeId) { update(static_cast<uint16_t>(typeId)); });

    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args& args) {
        const auto* type = args.size() > 2 ? std::any_cast<Otc::ResourceTypes_t>(&args[2]) : nullptr;
        if (type && (*type == DustResource || *type == LunarAscensionOrbsResource))
            onResourceBalanceChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_message->dialog()->hide();
        if (dialog()->isShown())
            dialog()->close();
        m_typeId = 0;
        return std::any{};
    });

    input::setActionHandler(A::OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG, [this](const input::TGameAction& action) {
        const auto typeId = static_cast<uint16_t>(action.instance);
        const auto level = static_cast<uint8_t>(action.parameter);
        const auto perk = static_cast<uint8_t>(action.category);
        const TProficiencyData* data = m_storage->data(typeId);
        if (!data || !data->shapedSlot(level, perk))
            return true;
        m_typeId = typeId;
        m_level = level;
        m_perk = perk;
        m_fromPerkTree = action.enable;
        showDialog();
        return true;
    });
}

TWeaponProficiencyShapeDialogController::~TWeaponProficiencyShapeDialogController()
{
    input::setActionHandler(A::OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG, nullptr);
}

std::optional<TShapedSlot> TWeaponProficiencyShapeDialogController::shapedSlot() const
{
    const TProficiencyData* data = m_storage->data(m_typeId);
    if (!data || !m_fromPerkTree)
        return std::nullopt;
    const TShapedSlot* slot = data->shapedSlot(m_level, m_perk);
    return slot ? std::optional<TShapedSlot>(*slot) : std::nullopt;
}

void TWeaponProficiencyShapeDialogController::update(const uint16_t typeId)
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
    m_message->dialog()->hide();
    update(m_typeId);
    requestBalances();
    dialog()->show();
}

void TWeaponProficiencyShapeDialogController::requestClose() { closeDialog(); }

void TWeaponProficiencyShapeDialogController::closeDialog() { dialog()->close(); }

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
    g_game.sendWeaponProficiencySlotAction(type, m_typeId, m_level, m_perk);
}

void TWeaponProficiencyShapeDialogController::onMessageDialogAccept()
{
    switch (m_pending) {
    case EConfirm::Refine: sendSlotAction(Otc::WEAPON_PROFICIENCY_REFINE_SLOT); break;
    case EConfirm::Maximise: sendSlotAction(Otc::WEAPON_PROFICIENCY_MAXIMISE_SLOT); break;
    case EConfirm::Reshape: sendSlotAction(Otc::WEAPON_PROFICIENCY_RESHAPE_SLOT); break;
    case EConfirm::Clear:
        sendSlotAction(Otc::WEAPON_PROFICIENCY_CLEAR_SLOT);
        closeDialog();
        break;
    }
}

void TWeaponProficiencyShapeDialogController::onResourceBalanceChanged() { update(m_typeId); }

void TWeaponProficiencyShapeDialogController::onShowShapeOptionsClicked()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG));
    action->parameter = profession();
    input::publish(action);
}

TWeaponProficiencyReshapeDialogController::TWeaponProficiencyReshapeDialogController(TProficiencyStorage* storage,
                                                                                     TWeaponProficiencyShapeDialogController* shape,
                                                                                     QObject* parent)
    : TDialogController(QStringLiteral("WeaponProficiencyReshapeDialog.qml"), parent)
    , m_storage(storage)
    , m_shape(shape)
    , m_message(new gamewindow::TMessageDialogController(this))
{
    m_message->setYesNoButtons();
    m_message->setCloseOnButtonClicked(true);
    m_message->setMessageDialogTextFormat(gamewindow::TMessageDialogController::RICH_TEXT);
    connect(m_message, &gamewindow::TMessageDialogController::acceptClicked, this,
            &TWeaponProficiencyReshapeDialogController::onMessageDialogAccept);
    connect(m_shape, &TWeaponProficiencyShapeDialogController::perkInformationChanged, this,
            &TWeaponProficiencyReshapeDialogController::currentPerkChanged);
    // CIP FUN_100e2a1c0.
    connect(m_storage, &TProficiencyStorage::reshapeOffersChanged, this, [this] {
        rebuildOffers();
        m_message->dialog()->hide();
        dialog()->show();
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_message->dialog()->hide();
        if (dialog()->isShown())
            dialog()->close();
        return std::any{};
    });
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
        g_game.sendWeaponProficiencySlotAction(Otc::WEAPON_PROFICIENCY_PICK_OFFER, offers->typeId, offers->level, offers->perk, index);
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

void TWeaponProficiencyReshapeDialogController::showDialog()
{
    m_message->dialog()->hide();
    dialog()->show();
}

void TWeaponProficiencyReshapeDialogController::closeDialog() { dialog()->close(); }

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

TShapedPerkOptionsProxy::TShapedPerkOptionsProxy(QObject* parent)
    : cip::TShapedPerkOptionsSortFilterProxyModel(parent)
{
    setSourceModel(&m_model);
    sort(0, Qt::AscendingOrder);
    connect(this, &TShapedPerkOptionsProxy::filterChanged, this, [this] {
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    });
}

void TShapedPerkOptionsProxy::setFilterString(const QString& filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    Q_EMIT filterChanged();
}

bool TShapedPerkOptionsProxy::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
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

bool TShapedPerkOptionsProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto key = [this](const QModelIndex& index) {
        return std::make_tuple(m_model.data(index, TShapedPerkOptionsModel::PerkName).toString(),
                               m_model.data(index, TShapedPerkOptionsModel::AugmentSpellName).toString(),
                               m_model.data(index, TShapedPerkOptionsModel::AugmentType).toString(),
                               m_model.data(index, TShapedPerkOptionsModel::PerkDescriptionRank0).toString());
    };
    return key(left) < key(right);
}

TWeaponProficiencyShapeOptionsDialogController::TWeaponProficiencyShapeOptionsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("WeaponProficiencyShapeOptionsDialog.qml"), parent)
{
    m_perkOptionsModel = &m_proxy;
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        if (dialog()->isShown())
            dialog()->close();
        return std::any{};
    });
    input::setActionHandler(A::OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG, [this](const input::TGameAction& action) {
        const int profession = action.parameter;
        if (profession >= 1 && profession <= 5) {
            std::vector<TProficiencyPerk> perks;
            for (const int index : shapeOptions(profession)) {
                if (const std::optional<TProficiencyPerk> perk = shapePerk(index))
                    perks.push_back(*perk);
            }
            m_proxy.setPerks(std::move(perks));
        }
        showDialog();
        return true;
    });
}

TWeaponProficiencyShapeOptionsDialogController::~TWeaponProficiencyShapeOptionsDialogController()
{
    input::setActionHandler(A::OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG, nullptr);
}

void TWeaponProficiencyShapeOptionsDialogController::requestClose() { closeDialog(); }

void TWeaponProficiencyShapeOptionsDialogController::setFilterString(const QString& Filter) { m_proxy.setFilterString(Filter); }

void TWeaponProficiencyShapeOptionsDialogController::showDialog() { dialog()->show(); }

void TWeaponProficiencyShapeOptionsDialogController::closeDialog() { dialog()->close(); }

}
