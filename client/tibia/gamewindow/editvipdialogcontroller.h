#pragma once

#include "gamewindow/dialogmanager.h"
#include "creatures/vipstorage.h"

#include <cip_gen.h>

#include <QList>

namespace tibia {
namespace gamewindow {

/// A group's check box in EditVipDialog.qml (CIP: TVipGroupInfo).
class TVipGroupInfo final : public cip::tibia::gamewindow::TVipGroupInfo
{
    Q_OBJECT
public:
    TVipGroupInfo(uint8_t id, const QString& name, bool selected, QObject* parent = nullptr);
};

/// EditVipDialog.qml (CIP: TEditVipDialogController): the VIP's icon,
/// description, login notification and groups.
class TEditVipDialogController final : public TDialogController<cip::tibia::gamewindow::TEditVipDialogController>
{
    Q_OBJECT
public:
    explicit TEditVipDialogController(QObject* parent = nullptr);

    /// Shows the dialog for `vip`: its name, icon, description, notification
    /// and one check box per group (the VIP's checked), sorted by upper-case name.
    void open(const creatures::TVipStorage::Vip& vip, const std::map<uint8_t, creatures::TVipStorage::Group>& groups);
    /// CIP: deletes the dialog item.
    void destroyDialog();

public Q_SLOTS:
    void onCancelClicked() override;
    /// Closes; reports the VIP with the new values unless nothing changed.
    void onOkClicked(int VipIconId, const QString& Description, bool Notify) override;
    void onControllerFinished() override;

Q_SIGNALS:
    void vipDataUpdated(const tibia::creatures::TVipStorage::Vip& vip);

private:
    void clearGroupInfos();

    creatures::TVipStorage::Vip m_vip;
    QList<TVipGroupInfo*> m_groupInfos;
};

}
}
