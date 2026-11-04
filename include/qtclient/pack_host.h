#pragma once

/// Tip-owned wiring surface for product packs (CIP-neutral).
/// Tip-filled pack wiring surface for suite_wire / pack entry.

#include "mods/attention_backend.h"
#include "mods/instance_backend.h"
#include "mods/mission_backend.h"

#include <QString>

#include <functional>

class QObject;
class QQuickItem;

namespace qtclient {

using BrowseOpenerFn = std::function<void(const QString&)>;

struct PackHostWire {
    std::function<void(tibia::mods::MissionBackend* backend, BrowseOpenerFn browse)> bindQuestTracker;
    std::function<void(tibia::mods::MissionBackend* backend, BrowseOpenerFn browse)> bindQuestLog;
    std::function<void(tibia::mods::InstanceBackend* instance)> bindInstanceToInfoDialogs;
    std::function<void(tibia::mods::AttentionBackend* attention)> bindAttentionToButtonBar;
    /// `page` must be a `config::TOptionsPage*` (void* avoids pulling tip config into SDK).
    std::function<void(int category, void* optionsPage)> addProductOptionsPage;
    std::function<void(int page)> openOptionsPage;
    std::function<void()> openStore;
    std::function<void()> requestLogoutViaButtonBar;
    std::function<void(QObject* controller)> setSocialButtonOnButtonBar;
    std::function<void(QQuickItem* item)> setActiveChatItem;
    std::function<void(QObject* controller)> installCreateCharacterController;

    /// QObject* accessors (pack may qobject_cast when a feature still needs a tip type).
    std::function<QObject*()> blessingsDialog;
    std::function<QObject*()> npcDialog;
    std::function<QObject*()> chatController;
    std::function<QObject*()> chatStorage;
    std::function<QObject*()> storeMessages;
    std::function<QObject*()> storeImageManager;
    std::function<QObject*()> minimapController;
    std::function<QObject*()> sidebarManager;
    std::function<QObject*()> sidebarWidgets;
    std::function<QObject*()> containers;
    std::function<QObject*()> buttonBar;
    std::function<QObject*()> imbuingDialog;
    std::function<QObject*()> imbuingStorage;
    std::function<QObject*()> sessionBootstrap;

    std::function<QQuickItem*()> minimapPanelItem;
    std::function<QQuickItem*()> inventoryPanelItem;
    std::function<QQuickItem*()> buttonBarPanelItem;

    bool valid() const { return static_cast<bool>(bindQuestTracker); }
};

} // namespace qtclient
