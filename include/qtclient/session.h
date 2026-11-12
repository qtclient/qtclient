#pragma once

/// Pack-facing session accessors (CIP-shaped). Prefer this over `world/game.h`.
///
/// Linked tip packs that still need wire helpers (`useMapObject`, …) must go
/// through `mod_api` hooks or an allowlisted CIP header — not the OTC Game hub.

#include "game/gamesession.h"
#include "game/gamenotify.h"
#include "game/creaturesession.h"
#include "game/mapsession.h"
#include "game/thingsession.h"


namespace qtclient::session {

using tibia::game::localPlayer;
using tibia::game::localPlayerPosition;
using tibia::game::localPlayerMapPos;
using tibia::game::localPlayerName;
using tibia::game::localPlayerId;
using tibia::game::localPlayerLevel;
using tibia::game::localPlayerHasReachedMain;
using tibia::game::localPlayerInventoryTypeId;
using tibia::game::openLocalPlayerInventory;
using tibia::game::localPlayerStopAutoWalk;
using tibia::game::localPlayerIsAutoWalking;
using tibia::game::localPlayerAutoWalk;
using tibia::game::localPlayerBlessVisualState;
using tibia::game::localPlayerSetInformationChannel;
using tibia::game::localPlayerClearInformationChannel;
using tibia::game::localPlayerSetSoftDead;
using tibia::game::localPlayerOutfit;
using tibia::game::itemName;
using tibia::game::isValidItemId;
using tibia::game::itemIsContainer;
using tibia::game::itemIsPickupable;
using tibia::game::itemMeanPrice;
using tibia::game::itemMaxNpcSaleGoldPrice;
using tibia::game::clearEffectVariants;
using tibia::game::registerEffectVariant;
using tibia::game::InventorySlotInfo;
using tibia::game::localPlayerInventorySlot;
using tibia::game::raceName;
using tibia::game::raceExists;
using tibia::game::raceIdsByName;
using tibia::game::RaceOutfitInfo;
using tibia::game::raceOutfit;
using tibia::game::raceIsBoss;
using tibia::game::allRaceIds;
using tibia::game::QuestInfo;
using tibia::game::allQuestInfos;
using tibia::game::questInfo;
using tibia::game::CreatureOutfitInfo;
using tibia::game::CreatureSnapshot;
using tibia::game::spectatorsNear;
using tibia::game::creatureAt;
using tibia::game::MapPos;
using tibia::game::MapAwareRange;
using tibia::game::MapUseHit;
using tibia::game::MapItemHit;
using tibia::game::canTravelPath;
using tibia::game::canTravelPathAsync;
using tibia::game::isWalkable;
using tibia::game::awareRange;
using tibia::game::itemHits;
using tibia::game::topUseHit;
using tibia::game::findItemsInRange;
using tibia::game::isSightClear;
using tibia::game::isOnline;
using tibia::game::isDead;
using tibia::game::isSceneFrozen;
using tibia::game::setSceneFrozen;
using tibia::game::isSwitchingCharacter;
using tibia::game::characterName;
using tibia::game::cipFeature;
using tibia::game::enableFeature;
using tibia::game::setCustomOs;
using tibia::game::attackingCreatureId;
using tibia::game::setAttackingCreatureId;
using tibia::game::talk;
using tibia::game::turn;
using tibia::game::safeLogout;
using tibia::game::requestBless;
using tibia::game::partyJoin;
using tibia::game::requestQuickLootBlackWhiteList;
using tibia::game::sendStatusTrackerBestiary;
using tibia::game::requestBestiarySearch;
using tibia::game::sendFriendSystemPayload;
using tibia::game::openPrivateChannel;
using tibia::game::requestTeamFinderList;
using tibia::game::requestTeamFinderLeader;
using tibia::game::joinTeamFinder;
using tibia::game::leaveTeamFinder;
using tibia::game::createTeamFinder;
using tibia::game::resetTeamFinderLeader;
using tibia::game::updateTeamFinderMember;
using tibia::game::useInventoryItemOnCreature;
using tibia::game::notify;


} // namespace qtclient::session
