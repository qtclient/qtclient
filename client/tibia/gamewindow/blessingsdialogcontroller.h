#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <vector>

struct BlessDialogData;

namespace tibia {
namespace gamewindow {

/// One blessing row (CIP: game::TQmlBlessingWithCharges; name ,
/// image →).
class TQmlBlessingWithCharges final : public cip::tibia::game::TQmlBlessingWithCharges
{
    Q_OBJECT
public:
    TQmlBlessingWithCharges(int id, int charges, int chargesFromStore, QObject* parent);
};

/// One history row (CIP: game::TQmlBlessingHistoryEntry; timestamp
/// → , gained).
class TQmlBlessingHistoryEntry final : public cip::tibia::game::TQmlBlessingHistoryEntry
{
    Q_OBJECT
public:
    TQmlBlessingHistoryEntry(quint32 timestamp, int type, const QString& description, QObject* parent);
};

/// The dialog's data (CIP: game::TQmlBlessingDialogData, built by
/// ; texts and).
class TQmlBlessingDialogData final : public cip::tibia::game::TQmlBlessingDialogData
{
    Q_OBJECT
public:
    TQmlBlessingDialogData(const BlessDialogData& data, bool premium, QObject* parent);

    /// CIP .
    void setPremium(bool premium);

private:
    QString promotionText() const;

    bool m_promoted = false;
    int m_promotionReduction = 0;
};

/// BlessingsDialog.qml (CIP: gamewindow::TBlessingsDialogController, ctor
/// ). Opens when the server sends the blessing dialog (answer to
/// the inventory's blessings button).
class TBlessingsDialogController final : public TDialogController<cip::tibia::gamewindow::TBlessingsDialogController>
{
    Q_OBJECT
public:
    explicit TBlessingsDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void closeDialog() override;
    /// CIP .
    void onPremiumStatusChanged() override;
    /// CIP : store request type 1 (premium category).
    void requestOpenStoreWithPremiumCategory() override;
    /// CIP : store request type 3 with the blessing's offer.
    void requestOpenStoreWithBlessingOffer(int BlessingId) override;
    void requestClose() override;

private:
    /// CIP : replaces the data and shows the dialog. CIP only
    /// creates the item when there is none; the QML reads blessingsData once,
    /// so an open dialog is rebuilt instead.
    void setData(const BlessDialogData& data);

    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
