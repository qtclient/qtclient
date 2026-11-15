#pragma once

#include "gamewindow/assignactiondialogs.h"

#include <cip_gen.h>

#include <QSortFilterProxyModel>
#include <QVariant>
#include <QJSValue>

#include <set>
#include <vector>

namespace tibia {
namespace gamewindow {

/// Magical Archive tab (CIP: TMagicalArchiveDialogController, session
/// , show).
class TMagicalArchiveDialogController final : public cip::tibia::gamewindow::TMagicalArchiveDialogController
{
    Q_OBJECT
public:
    explicit TMagicalArchiveDialogController(QObject* parent = nullptr);

    QObject* spellListModel() const override { return m_proxy; }
    QJSValue spellsPreviewsJson() const override;
    void setSelectedSpellID(int v) override;

    void showDialogTab() override;
    void hideDialogTab() override;
    void requestAssignSpellToActionButton() override;

private:
    void refreshSelectionFlags();

    TSpellListDataModelSortFilterProxy* m_proxy;
    mutable QVariant m_previewsVariant;
    mutable QJSValue m_previewsJs;
};

}
}
