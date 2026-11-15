#pragma once

#include "game/cooldownstorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

class QQuickItem;

namespace tibia {
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
    explicit TCooldownBarController(QQuickItem* gameWindowItem, QObject* parent = nullptr);

    /// Rebuilds both rows when the cooldowns changed since the last call.
    void update(qint64 now);
    /// The cooldowns changed: rebuild on the next update.
    void markDirty();

private:
    /// Shows every bar group idle.
    void showIdleSpellGroups();
    /// "<name> (<seconds> sec. cooldown)" for a spell's icon.
    static QString spellTooltip(int spellId);
    /// Option 0x77 cooldownBarEnabled; CIP's game window controller does this.

    QPointer<QQuickItem> m_bar;
    bool m_dirty = false;
    /// The host's stand-in for CIP's per-frame update call.
    QTimer m_nextFrame;
};

}
}
