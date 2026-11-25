// Host API v9 cip_object: the game window glue of the CipSoft object table
// (cip_object_abi.cpp has the generic, meta-object part). Host classes' C++ members that are
// no moc methods, and the host surfaces a pack's CipSoft object can take over
// (set_host_controller).
#include "mods/abi_internal.h"
#include "mods/cip_object_support.h"

#include "game/gameactionrouter.h"
#include "gamewindow/buttonbarcontroller.h"
#include "gamewindow/characterinfodialogcontroller.h"
#include "gamewindow/sidebarpanelsmanager.h"
#include "gamewindow/socialdialogcontroller.h"

#include <QJsonArray>

#include <functional>
#include <map>
#include <string>
#include <utility>

namespace tibia::mods::cipobjects {

namespace {

/// Host methods of host classes that are no moc members (C++ setters / overloads CipSoft
/// calls directly) but that a pack page drives, by name and argument count.
using THostMethod = std::function<bool(QObject*, const QJsonArray&)>;

const std::multimap<std::string, std::pair<qsizetype, THostMethod>>& hostMethods()
{
    using gamewindow::TCharacterInfoDialogController;
    static const std::multimap<std::string, std::pair<qsizetype, THostMethod>> methods = {
        { "setCharacterId",
          { 1, [](QObject* o, const QJsonArray& a) {
               auto* c = qobject_cast<TCharacterInfoDialogController*>(o);
               if (c)
                   c->setCharacterId(static_cast<uint32_t>(a.at(0).toInteger()));
               return c != nullptr;
           } } },
        { "showDialogTab",
          { 1, [](QObject* o, const QJsonArray& a) {
               auto* c = qobject_cast<TCharacterInfoDialogController*>(o);
               if (c)
                   c->showDialogTab(a.at(0).toInt());
               return c != nullptr;
           } } },
    };
    return methods;
}

} // namespace

bool cipInvokeHostMethod(QObject* object, const QByteArray& method, const QJsonArray& args)
{
    const auto range = hostMethods().equal_range(method.toStdString());
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.first == args.size() && it->second.second(object, args))
            return true;
    }
    return false;
}

int cipSetHostController(const int slot, QObject* controller)
{
    if (slot != QTCLIENT_HOST_CONTROLLER_SOCIAL_BUTTON)
        return 0;
    if (controller && !controller->inherits("cip::tibia::gamewindow::TSocialButtonController"))
        return 0;
    const HostObjects* host = hostObjects();
    gamewindow::TButtonBarController* buttonBar = host && host->sidebarPanels ? host->sidebarPanels->buttonBar() : nullptr;
    if (!buttonBar)
        return 0;
    buttonBar->setSocialButtonController(controller);
    // A pack's social button brings its own dialog: CipSoft's stops answering the router.
    if (controller && host->socialDialog && host->router)
        QObject::disconnect(host->router, nullptr, host->socialDialog, nullptr);
    return 1;
}

} // namespace tibia::mods::cipobjects
