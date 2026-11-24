#include "characterinformationmodeldata.h"

#include "colorhelper.h"
#include <client/outfit.h>

namespace tibia::gamewindow {

namespace {

/// The outfit of a row; its colours are the outfit colour indices.
class TRowOutfit final : public cip::tibia::gamewindow::TOutfitQMLInformation
{
public:
    TRowOutfit(const authentication::TCharacter& c, QObject* parent)
        : TOutfitQMLInformation(parent)
    {
        const auto color = [](const int index) { return tibia::shared::outfitColor(index); };
        m_id = c.outfitId;
        m_headColor = color(c.headColor);
        m_torsoColor = color(c.torsoColor);
        m_legsColor = color(c.legsColor);
        m_detailColor = color(c.detailColor);
        m_firstAddOn = c.firstAddOn;
        m_secondAddOn = c.secondAddOn;
    }
};

}

TCharacterInformationModelData::TCharacterInformationModelData(const authentication::TCharacter& character, const QString& level,
                                                               const QString& vocation, const QString& world, const QString& worldRules,
                                                               const bool isPinned)
{
    m_characterName = character.name;
    m_level = level;
    m_vocation = vocation;
    m_world = world;
    m_worldRules = worldRules;
    m_outfit = new TRowOutfit(character, this);
    m_isMainCharacter = character.isMainCharacter;
    m_dailyRewardState = cip::tibia::qmlcomponents::enums::EDailyRewardState(character.dailyRewardState);
    m_isPinned = isPinned;
    m_isHidden = character.isHidden;
}

}
