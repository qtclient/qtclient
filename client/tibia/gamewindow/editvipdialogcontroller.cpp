#include "editvipdialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QQuickItem>

#include <algorithm>
#include <memory>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

}

TVipGroupInfo::TVipGroupInfo(const uint8_t id, const QString& name, const bool selected, QObject* parent)
    : cip::tibia::gamewindow::TVipGroupInfo(parent)
{
    m_groupID = static_cast<signed char>(id);
    m_groupName = name;
    m_selected = selected;
}

TEditVipDialogController::TEditVipDialogController(TVipStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("EditVipDialog.qml"), parent)
    , m_storage(storage)
{
}

void TEditVipDialogController::open(const TVipStorage::Vip& vip)
{
    dialog()->destroy();
    clearGroupInfos();
    m_vipId = vip.id;
    for (const TVipStorage::Group& group : m_storage->groups()) {
        const bool selected = std::find(vip.groups.begin(), vip.groups.end(), group.id) != vip.groups.end();
        m_groupInfos.append(new TVipGroupInfo(group.id, group.name, selected, this));
    }

    dialog()->create();
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    QList<QObject*> infos;
    for (TVipGroupInfo* info : m_groupInfos)
        infos.append(info);
    item->setProperty("vipName", vip.name);
    item->setProperty("vipIconID", vip.iconId);
    item->setProperty("vipDescription", vip.description);
    item->setProperty("vipNotify", vip.notifyLogin);
    item->setProperty("vipGroupInfos", QVariant::fromValue(infos));
    dialog()->show();
}

void TEditVipDialogController::onCancelClicked() { dialog()->close(); }

void TEditVipDialogController::onOkClicked(const int VipIconId, const QString& Description, const bool Notify)
{
    // CIP 0x100be72f0: publish REQUST_VIP_DATA_UPDATE; TVipStorage handles it.
    auto action = input::mutableClone(*input::makeAction(A::REQUST_VIP_DATA_UPDATE));
    action->instance = static_cast<int>(m_vipId);
    action->text = Description;
    action->parameter = VipIconId;
    action->enable = Notify;
    for (const TVipGroupInfo* info : m_groupInfos) {
        if (info->selected())
            action->vipGroups.push_back(static_cast<uint8_t>(info->groupID()));
    }
    input::publish(action);
    dialog()->close();
}

void TEditVipDialogController::onControllerFinished() { dialog()->close(); }

void TEditVipDialogController::clearGroupInfos()
{
    for (TVipGroupInfo* info : std::as_const(m_groupInfos))
        info->deleteLater();
    m_groupInfos.clear();
}

}
