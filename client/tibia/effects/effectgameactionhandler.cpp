#include "effects/effectgameactionhandler.h"

#include "appearances/appearanceinstance.h"
#include "creatures/creaturestorage.h"
#include "effects/effectstorage.h"

namespace tibia::effects {

TEffectGameActionHandler::TEffectGameActionHandler(TEffectStorage& storage, QObject* parent)
    : cip::tibia::effects::TEffectGameActionHandler(parent)
    , m_storage(storage)
{
}

TEffectGameActionHandler::~TEffectGameActionHandler() = default;

void TEffectGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    const input::IGameAction& action = *pGameAction;
    if (action.gameActionType() == input::EGameAction::REMOVE_ALL_TUTORIAL_MARKERS) {
        m_storage.stopTrackedEffects();
        return;
    }
    const auto* marker = input::gameActionAs<input::TGameActionAddGUITutorialMarker>(&action);
    if (!marker)
        return;
    // A marker on a map field, or on the field of a creature.
    Position field = marker->position();
    if (marker->creatureId() != 0) {
        if (creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance()) {
            if (const auto creature = storage->creatureAt(marker->creatureId()))
                field = creature->position();
        }
    }
    if (!field.isValid())
        return;
    m_storage.trackEffect(appearances::TAppearanceInstanceFactory::createTutorialFieldEffect(1, field));
    m_storage.trackEffect(appearances::TAppearanceInstanceFactory::createTutorialFieldEffect(0, field));
}

}
