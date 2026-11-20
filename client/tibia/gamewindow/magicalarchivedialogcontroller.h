#pragma once

#include "gamewindow/assignactiondialogs.h"
#include "gamewindow/cyclopediadialogcontext.h"
#include "input/gameaction.h"

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

    /// The page left for another tab keeps no spell: back, the archive opens
    /// on its first one.
    TCyclopediaDialogContext captureContext() const { return TMagicalArchiveCyclopediaContext{}; }
    /// The context's spell becomes the one the page opens on; another tab's
    /// context throws.
    void restoreContext(const TCyclopediaDialogContext& CyclopediaContext)
    {
        setInitialSpellID(cyclopediaContextAs<TMagicalArchiveCyclopediaContext>(CyclopediaContext).spellId);
    }
    /// The spell the page opens on (the Cyclopedia's open action, the back button).
    void setInitialSpellID(int spellId);

    QObject* spellListModel() const override { return m_proxy; }
    QJSValue spellsPreviewsJson() const override;
    void setSelectedSpellID(int v) override;

    void showDialogTab() override;
    void hideDialogTab() override;
    /// Assigning the selected spell: the Cyclopedia steps aside while an
    /// action button is picked (REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON).
    void requestAssignSpellToActionButton() override;
    /// The picked action button gets the selected spell; the selection's end
    /// stops waiting for it.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    void refreshSelectionFlags();

    TSpellListDataModelSortFilterProxy* m_proxy;
    /// An action button is being picked for the selected spell.
    bool m_waitingForActionButton = false;
    mutable QVariant m_previewsVariant;
    mutable QJSValue m_previewsJs;
};

}
}
