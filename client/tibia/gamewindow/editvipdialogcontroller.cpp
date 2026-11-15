#include "editvipdialogcontroller.h"


#include <QQuickItem>

#include <algorithm>
#include <memory>

namespace tibia::gamewindow {

TVipGroupInfo::TVipGroupInfo(const uint8_t id, const QString& name, const bool selected, QObject* parent)
    : cip::tibia::gamewindow::TVipGroupInfo(parent)
{
    m_groupID = static_cast<signed char>(id);
    m_groupName = name;
    m_selected = selected;
}

TEditVipDialogController::TEditVipDialogController(QObject* parent)
    : TDialogController(QStringLiteral("EditVipDialog.qml"), parent)
{
}

void TEditVipDialogController::open(const creatures::TVipStorage::Vip& vip, const std::map<uint8_t, creatures::TVipStorage::Group>& groups)
{
    dialog()->destroy();
    dialog()->create();
    m_vip = vip;
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    item->setProperty("vipName", m_vip.name);
    item->setProperty("vipIconID", m_vip.iconId);
    item->setProperty("vipDescription", m_vip.description);
    item->setProperty("vipNotify", m_vip.notifyLogin);
    clearGroupInfos();
    for (const auto& [id, group] : groups) {
        const bool selected = std::find(m_vip.groups.begin(), m_vip.groups.end(), id) != m_vip.groups.end();
        m_groupInfos.append(new TVipGroupInfo(id, group.name, selected, this));
    }
    std::sort(m_groupInfos.begin(), m_groupInfos.end(), [](const TVipGroupInfo* left, const TVipGroupInfo* right) {
        return left->groupName().toUpper() < right->groupName().toUpper();
    });
    QList<QObject*> infos;
    for (TVipGroupInfo* info : m_groupInfos)
        infos.append(info);
    item->setProperty("vipGroupInfos", QVariant::fromValue(infos));
    dialog()->show();
}

void TEditVipDialogController::onCancelClicked() { dialog()->close(); }

void TEditVipDialogController::onOkClicked(const int VipIconId, const QString& Description, const bool Notify)
{
    dialog()->close();
    std::vector<uint8_t> groups;
    for (const TVipGroupInfo* info : m_groupInfos) {
        if (info->selected())
            groups.push_back(static_cast<uint8_t>(info->groupID()));
    }
    if (m_vip.iconId == static_cast<uint8_t>(VipIconId) && m_vip.description == Description && m_vip.notifyLogin == Notify && m_vip.groups == groups)
        return;
    creatures::TVipStorage::Vip edited = m_vip;
    edited.iconId = static_cast<uint8_t>(VipIconId);
    edited.description = Description;
    edited.notifyLogin = Notify;
    edited.groups = std::move(groups);
    Q_EMIT vipDataUpdated(edited);
}

void TEditVipDialogController::onControllerFinished() { dialog()->close(); }

void TEditVipDialogController::clearGroupInfos()
{
    for (TVipGroupInfo* info : std::as_const(m_groupInfos))
        info->deleteLater();
    m_groupInfos.clear();
}

}
