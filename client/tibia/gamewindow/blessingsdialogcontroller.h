#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <vector>

struct BlessDialogData;

namespace tibia::gamewindow {

/// One blessing row (CIP: game::TQmlBlessingWithCharges; name 0x100b53bf0,
/// image 0x100b53da0 → 0x100927670).
class TQmlBlessingWithCharges final : public cip::tibia::game::TQmlBlessingWithCharges
{
    Q_OBJECT
public:
    TQmlBlessingWithCharges(int id, int charges, int chargesFromStore, QObject* parent);
};

/// One history row (CIP: game::TQmlBlessingHistoryEntry; timestamp
/// 0x100b53ea0 → 0x10095c790, gained 0x100b53ec0).
class TQmlBlessingHistoryEntry final : public cip::tibia::game::TQmlBlessingHistoryEntry
{
    Q_OBJECT
public:
    TQmlBlessingHistoryEntry(quint32 timestamp, int type, const QString& description, QObject* parent);
};

/// The dialog's data (CIP: game::TQmlBlessingDialogData, built by
/// 0x100b54200; texts 0x100b543b0 and 0x100b54520).
class TQmlBlessingDialogData final : public cip::tibia::game::TQmlBlessingDialogData
{
    Q_OBJECT
public:
    TQmlBlessingDialogData(const BlessDialogData& data, bool premium, QObject* parent);

    /// CIP 0x100b54390.
    void setPremium(bool premium);

private:
    QString promotionText() const;

    bool m_promoted = false;
    int m_promotionReduction = 0;
};

/// BlessingsDialog.qml (CIP: gamewindow::TBlessingsDialogController, ctor
/// 0x100c9f7b0). Opens when the server sends the blessing dialog (answer to
/// the inventory's blessings button).
class TBlessingsDialogController final : public TDialogController<cip::tibia::gamewindow::TBlessingsDialogController>
{
    Q_OBJECT
public:
    explicit TBlessingsDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void closeDialog() override;
    /// CIP 0x100c9fc30.
    void onPremiumStatusChanged() override;
    /// CIP 0x100c9fe10: store request type 1 (premium category).
    void requestOpenStoreWithPremiumCategory() override;
    /// CIP 0x100c9ffc0: store request type 3 with the blessing's offer.
    void requestOpenStoreWithBlessingOffer(int BlessingId) override;
    void requestClose() override;

private:
    /// CIP 0x100c9fa50: replaces the data and shows the dialog. CIP only
    /// creates the item when there is none; the QML reads blessingsData once,
    /// so an open dialog is rebuilt instead.
    void setData(const BlessDialogData& data);

    std::vector<client::gamecore::Hook> m_hooks;
};

}
