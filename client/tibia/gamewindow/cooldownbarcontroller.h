#pragma once

#include "game/cooldownstorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <memory>
#include <vector>

class QQuickItem;

namespace tibia {
namespace game {
class IGameSession;
}
namespace spells {
class TSpellInfoStorage;
}
namespace gamewindow {

/// One cooldown icon for CooldownBar.qml (CIP: TCooldownProgressDataForQML).
/// CooldownBar.qml animates the bar from remaining/total itself.
class TCooldownProgressDataForQML final : public cip::tibia::gamewindow::TCooldownProgressDataForQML
{
    Q_OBJECT
public:
    TCooldownProgressDataForQML(QString sourceUrl, int remainingMs, int totalMs, QString tooltip);
};

/// CooldownBar under the map (CIP: TCooldownBarController). The spell group
/// row always shows the ten groups; the spell row the spells on cooldown.
/// Both rows are rebuilt on the next frame after the cooldowns changed.
class TCooldownBarController final : public cip::tibia::gamewindow::TCooldownBarController
{
    Q_OBJECT
public:
    /// `cooldownBar`: the game window's CooldownBar item.
    TCooldownBarController(spells::TSpellInfoStorage* spellInfoStorage, QQuickItem* cooldownBar,
                           QObject* parent = nullptr);

    /// Rebuilds both rows when the cooldowns changed since the last call.
    void update(qint64 now);
    /// The cooldowns changed: rebuild on the next update.
    void markDirty();
    /// Shows the bar idle and follows the session's cooldown storage (none
    /// without a session).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    /// Shows every bar group idle.
    void showIdleSpellGroups();
    /// "<name> (<seconds> sec. cooldown)" for a spell's icon.
    QString spellTooltip(int spellId) const;
    /// Option 0x77 cooldownBarEnabled; CIP's game window controller does this.

    spells::TSpellInfoStorage* m_spellInfoStorage;
    QPointer<QQuickItem> m_bar;
    std::shared_ptr<game::TCooldownStorage> m_cooldownStorage;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    bool m_dirty = false;
    /// The host's stand-in for CIP's per-frame update call.
    QTimer m_nextFrame;
};

}
}
