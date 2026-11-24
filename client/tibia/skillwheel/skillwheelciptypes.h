#pragma once

// CIP-named skillwheel surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QByteArray>
#include <QObject>
#include <QPointer>

#include <memory>

namespace tibia {
namespace creatures {
class TCreature;
class TCreatureStorage;
}
namespace cyclopedia {
class TCharacterInfoStorage;
}
namespace game {
class TPlayerData;
class TPremiumTimeStorage;
}
namespace input {
class IGameAction;
}
namespace protobuf {
namespace protocol {
class GameserverMessageGemAtelierGemResponse;
class GameserverMessageSkillWheel;
}
}
}

namespace tibia {
namespace skillwheel {

/// TModListSortFilterProxyModel product twin lives in skillwheelmodels.h (FragmentWorkshop).

struct TWheel;
class TSkillWheelStorage;

/// The skill wheel game actions (CIP: skillwheel::TSkillWheelGameActionHandler):
/// another player's wheel, the own wheel or the no-wheel / preview message,
/// and saving the edited wheel.
class TSkillWheelProtocolMessageHandler;

class TSkillWheelGameActionHandler final : public cip::tibia::skillwheel::TSkillWheelGameActionHandler
{
    Q_OBJECT
public:
    /// The session's skill wheel protocol handler sends the requests; the own
    /// player's id is read from the player creature when it already exists.
    TSkillWheelGameActionHandler(std::shared_ptr<TSkillWheelProtocolMessageHandler> pProtocolMessageHandler,
                                 const std::weak_ptr<creatures::TCreature>& pPlayerWeak);

    /// The session's premium time storage: without premium time the own
    /// wheel can be requested even when the player is not Premium.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage);

    /// CIP: SAVE_SKILL_WHEEL sends its configuration, REQUEST_OWN_SKILL_WHEEL the own request,
    /// REQUEST_SKILL_WHEEL the creature's.
    void handleGameAction(const input::IGameAction& action);
    /// The session router's game actions.
    void handleGameAction(const input::IGameActionPtr& pGameAction);
    /// REQUEST_SKILL_WHEEL: the own player's goes through the own request.
    void handleCreatureGameAction(uint32_t creatureId);
    /// CIP: the own player's creature id, 0 when it is gone.
    void onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<creatures::TCreature> pPlayerWeak);
    /// SAVE_SKILL_WHEEL.
    void handleSaveSkillWheelGameAction(const TWheel& wheel);

public Q_SLOTS:
    /// REQUEST_OWN_SKILL_WHEEL: from level 51 the server is asked when the
    /// player is Premium or the premium time storage has no premium; otherwise a vocation
    /// offers the preview message, none the no-wheel message.
    void handleRequestOnwSkillWheelGameAction() override;

private:
    std::shared_ptr<TSkillWheelProtocolMessageHandler> m_pProtocolMessageHandler;
    uint32_t m_playerId = 0;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
};

/// CIP `skillwheel::TSkillWheelProtocolMessageHandler`: the skill wheel and gem atelier messages.
class TSkillWheelProtocolMessageHandler final : public cip::tibia::skillwheel::TSkillWheelProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The handler keeps the skill wheel storage, the session's creature and
    /// character info storages and its player data. Host: the skill wheel
    /// storage is the game client's (it may be null when the session is made).
    TSkillWheelProtocolMessageHandler(TSkillWheelStorage* skillWheelStorage,
                                      std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                                      std::shared_ptr<cyclopedia::TCharacterInfoStorage> characterInfoStorage,
                                      std::shared_ptr<game::TPlayerData> playerData);

    /// Host: replaces the storage the handler fills (the session makes it
    /// and hands it in at construction; nothing calls this today).
    void setSkillWheelStorage(TSkillWheelStorage* storage);

    /// SkillWheel: without a wheel the no-wheel or preview message; otherwise the gem inventory, the
    /// mod grades and the server wheel go to the storage, the mod list is rebuilt and the dialog opens.
    void handleSkillWheelMessage(const ::tibia::protobuf::protocol::GameserverMessageSkillWheel& message);
    /// ResourceBalance 80..85: the unspent points, the unrevealed gems by quality and the fragments.
    void handleResourceBalanceMessage(int type, qint64 balance);

    /// GemAtelierGemResponse: a revealed gem (0) is selected in the atelier; the mods a reshape
    /// attempt rolled (1) go to the storage.
    void handleGemAtelierGemResponseMessage(
        const ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemResponse& Message);

    /// The Gem Atelier requests: dismantle (0), reveal a gem of a quality 0..2
    /// (1), reshape one mod (2), lock or unlock (3), enhance a fragment mod
    /// grade (4) and attempt a reshape (5: the server rolls the new mod).
    /// The wheel of a player.
    void requestSkillWheel(uint32_t playerId);
    /// The character's wheel through the cyclopedia character info (the character info dialog).
    static void requestCharacterSkillWheel(uint32_t characterId);
    /// The 36 slice points and the eight vessels' gems (-1 empty).
    void saveSkillWheel(const TWheel& wheel);
    static void dismantleGem(int gemId);
    static void revealGem(int quality);
    static void reshapeGem(int gemId, int modIndex);
    static void attemptReshapeGem(int gemId, int modIndex);
    static void toggleGemLock(int gemId);
    static void enhanceModGrade(int modType, int modId);

Q_SIGNALS:
    void sendRequestSkillWheel(const ::tibia::protobuf::protocol::GameclientMessageRequestSkillWheel& RequestSkillWheelMessage);
    void sendRequestSkillWheelViaCyclopedia(const ::tibia::protobuf::protocol::GameclientMessageOpenCyclopediaCharacterInfo& OpenCyclopediaCharacterInfoMessage);
    void sendApplySkillWheel(const ::tibia::protobuf::protocol::GameclientMessageApplySkillWheel& ApplySkillWheelMessage);
    void sendGemAtelierAction(const ::tibia::protobuf::protocol::GameclientMessageGemAtelier& GemAtelierMessage);

private:
    QPointer<TSkillWheelStorage> m_storage;
    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    std::shared_ptr<cyclopedia::TCharacterInfoStorage> m_characterInfoStorage;
    std::shared_ptr<game::TPlayerData> m_playerData;
};

}
}
