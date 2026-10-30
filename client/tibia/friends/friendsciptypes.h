#pragma once

// CIP-named friends surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::friends {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TFriendsAccountSearchStorage final : public cip::tibia::friends::TFriendsAccountSearchStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsAccountSearchStorage::TFriendsAccountSearchStorage;
};

class TFriendsFriendListStorage final : public cip::tibia::friends::TFriendsFriendListStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsFriendListStorage::TFriendsFriendListStorage;
};

class TFriendsInviteStorage final : public cip::tibia::friends::TFriendsInviteStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsInviteStorage::TFriendsInviteStorage;
};

class TFriendsProtocolMessageHandler final : public cip::tibia::friends::TFriendsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsProtocolMessageHandler::TFriendsProtocolMessageHandler;
};

class TFriendsStorage final : public cip::tibia::friends::TFriendsStorage
{
    Q_OBJECT
public:
    using cip::tibia::friends::TFriendsStorage::TFriendsStorage;
};

} // namespace tibia::friends
