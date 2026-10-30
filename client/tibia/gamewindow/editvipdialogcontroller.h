#pragma once

#include "gamewindow/dialogmanager.h"
#include "gamewindow/vipstorage.h"

#include <cip_gen.h>

#include <QList>

namespace tibia::gamewindow {

/// A group's check box in EditVipDialog.qml (CIP: TVipGroupInfo).
class TVipGroupInfo final : public cip::tibia::gamewindow::TVipGroupInfo
{
    Q_OBJECT
public:
    TVipGroupInfo(uint8_t id, const QString& name, bool selected, QObject* parent = nullptr);
};

/// EditVipDialog.qml (CIP: TEditVipDialogController, ctor 0x100c077f0): the
/// VIP's icon, description, login notification and groups.
class TEditVipDialogController final : public TDialogController<cip::tibia::gamewindow::TEditVipDialogController>
{
    Q_OBJECT
public:
    explicit TEditVipDialogController(TVipStorage* storage, QObject* parent = nullptr);

    /// CIP 0x100c07920: creates the dialog, sets vipName, vipIconID,
    /// vipDescription, vipNotify and vipGroupInfos (every group, the VIP's
    /// checked) and shows it.
    void open(const TVipStorage::Vip& vip);

public Q_SLOTS:
    void onCancelClicked() override;
    /// Sends the edit (CIP: REQUST_VIP_DATA_UPDATE through 0x100be72f0) and closes.
    void onOkClicked(int VipIconId, const QString& Description, bool Notify) override;
    /// CIP 0x100c083c0.
    void onControllerFinished() override;

private:
    void clearGroupInfos();

    TVipStorage* m_storage;
    uint32_t m_vipId = 0;
    QList<TVipGroupInfo*> m_groupInfos;
};

}
