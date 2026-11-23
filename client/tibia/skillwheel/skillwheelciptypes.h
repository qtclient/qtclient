#pragma once

// CIP-named skillwheel surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>
#include <QPointer>

#include <memory>

namespace tibia {
namespace creatures {
class TCreature;
}
namespace input {
class IGameAction;
}
namespace protobuf {
namespace protocol {
class GameserverMessageGemAtelierGemRevealed;
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

    /// The login session's premium flag (CIP: the premium time storage).
    void setSessionPremium(bool premium) { m_sessionPremium = premium; }

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
    /// player is Premium or the login session is not; otherwise a vocation
    /// offers the preview message, none the no-wheel message.
    void handleRequestOnwSkillWheelGameAction() override;

private:
    std::shared_ptr<TSkillWheelProtocolMessageHandler> m_pProtocolMessageHandler;
    uint32_t m_playerId = 0;
    bool m_sessionPremium = false;
};

/// CIP `skillwheel::TSkillWheelProtocolMessageHandler` — native Q→H for GemAtelierGemRevealed.
class TSkillWheelProtocolMessageHandler final : public cip::tibia::skillwheel::TSkillWheelProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TSkillWheelProtocolMessageHandler(QObject* parent = nullptr);

    /// Host: CipSoft's handler gets the session's skill wheel storage when it
    /// is made; the client's storage is handed over when the session becomes
    /// current (TSkillWheelManager::connectToGameSession).
    void setSkillWheelStorage(TSkillWheelStorage* storage);

    /// SkillWheel: without a wheel the no-wheel or preview message; otherwise the gem inventory, the
    /// mod grades and the server wheel go to the storage, the mod list is rebuilt and the dialog opens.
    void handleSkillWheelMessage(const ::tibia::protobuf::protocol::GameserverMessageSkillWheel& message);
    /// ResourceBalance 80..85: the unspent points, the unrevealed gems by quality and the fragments.
    void handleResourceBalanceMessage(int type, qint64 balance);

    /// GemAtelierGemRevealed: the atelier selects the revealed gem.
    void handleGemAtelierGemRevealedMessage(
        const ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemRevealed& Message);

    /// The Gem Atelier requests: dismantle (0), reveal a gem of a quality 0..2
    /// (1), reshape one mod (2), lock or unlock (3) and enhance a fragment mod
    /// grade (4).
    /// The wheel of a player.
    static void requestSkillWheel(uint32_t playerId);
    /// The character's wheel through the cyclopedia character info (the character info dialog).
    static void requestCharacterSkillWheel(uint32_t characterId);
    /// The 36 slice points and the eight vessels' gems (-1 empty).
    static void saveSkillWheel(const TWheel& wheel);
    static void dismantleGem(int gemId);
    static void revealGem(int quality);
    static void reshapeGem(int gemId, int modIndex);
    static void toggleGemLock(int gemId);
    static void enhanceModGrade(int modType, int modId);

private:
    QPointer<TSkillWheelStorage> m_storage;
};

}
}
