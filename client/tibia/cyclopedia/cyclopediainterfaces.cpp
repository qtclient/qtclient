#include "cyclopedia/cyclopediainterfaces.h"

namespace tibia::gamewindow {

IDialogTabPage::IDialogTabPage(QObject* parent)
    : cip::tibia::gamewindow::IDialogTabPage(parent)
{
}

IDialogTabPage::~IDialogTabPage() = default;

void IDialogTabPage::showDialogTab()
{
    // Live path: tip page controllers (friends / forge / prey / …) override
    // cip::IDialogTabPage::showDialogTab. Tip twin keeps the moc/slot floor.
}

void IDialogTabPage::hideDialogTab()
{
    // Live path: tip page controllers override hideDialogTab.
}

ICyclopediaDialogTab::ICyclopediaDialogTab(QObject* parent)
    : cip::tibia::gamewindow::ICyclopediaDialogTab(parent)
{
}

ICyclopediaDialogTab::~ICyclopediaDialogTab() = default;

void ICyclopediaDialogTab::showDialogTab()
{
    // Live path: TCyclopediaDialogController::switchTab → page()->showDialogTab().
}

void ICyclopediaDialogTab::hideDialogTab()
{
    // Live path: TCyclopediaDialogController::hideCurrentPage → page()->hideDialogTab().
}

void ICyclopediaDialogTab::restoreContext(const QVariantMap& CyclopediaContext)
{
    // Live path: TCyclopediaDialogController::requestBack restores per-tab
    // Context on the concrete tip controllers (item / monsters / houses / …).
    (void)CyclopediaContext;
}

ICyclopediaMapSelectionInfo::ICyclopediaMapSelectionInfo(QObject* parent)
    : cip::tibia::gamewindow::ICyclopediaMapSelectionInfo(parent)
{
}

ICyclopediaMapSelectionInfo::~ICyclopediaMapSelectionInfo() = default;

::cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType ICyclopediaMapSelectionInfo::selectionType() const
{
    return m_selectionType;
}

IDialogTabPage* tipIDialogTabPage()
{
    static IDialogTabPage keep;
    return &keep;
}

ICyclopediaDialogTab* tipICyclopediaDialogTab()
{
    static ICyclopediaDialogTab keep;
    return &keep;
}

ICyclopediaMapSelectionInfo* tipICyclopediaMapSelectionInfo()
{
    static ICyclopediaMapSelectionInfo keep;
    return &keep;
}

namespace {
const bool kTipIDialogTabPageLinked = (tipIDialogTabPage() != nullptr);
const bool kTipICyclopediaDialogTabLinked = (tipICyclopediaDialogTab() != nullptr);
const bool kTipICyclopediaMapSelectionInfoLinked = (tipICyclopediaMapSelectionInfo() != nullptr);
}

} // namespace tibia::gamewindow
