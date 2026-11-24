#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QMetaObject>
#include <QString>

class QQmlEngine;
class QQuickItem;

#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
}

namespace tibia {
namespace gamewindow {

class TGenericEnterTextDialogController;

/// The hireling's new name (CIP: THirelingNameChangeController). The server asks for it
/// (the player protocol handler's showHirelingNameChangeConfiguration, with the hireling
/// creature and the object it was asked from); a text prompt takes the name, and a
/// non-empty one is published as SET_HIRELING_NAME.
class THirelingNameChangeController final : public cip::tibia::gamewindow::THirelingNameChangeController
{
    Q_OBJECT
public:
    THirelingNameChangeController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);
    ~THirelingNameChangeController() override;

    /// CIP (v12): the prompt closes; then the session's player protocol handler asks for names.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// CIP (v13): keeps the creature and the object and shows the prompt empty.
    void onShowHirelingNameChangeConfiguration(quint32 CreatureID, quint32 ObjectID);

Q_SIGNALS:
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    /// CIP: the prompt's entered text, when not empty, becomes the name sent.
    void onTextEntered();

    /// CIP +0x10: "request_hireling_name_change_caption/_text", at most 30 characters.
    TGenericEnterTextDialogController* m_enterTextDialog;
    /// CIP +0x20.
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// CIP +0x30.
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    /// CIP +0x50 / +0x54.
    quint32 m_creatureId = 0;
    quint32 m_objectId = 0;
    /// CIP +0x58: the last name entered.
    QString m_name;
};

}
}
