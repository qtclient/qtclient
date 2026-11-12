#pragma once

#include <cip_gen.h>

#include <QObject>
#include <QVariantMap>

namespace tibia {
namespace gamewindow {

/// Qtclient twin of CIP `gamewindow::IDialogTabPage` (show/hide tab lifecycle).
/// Live pages inherit `cip::…::IDialogTabPage` via their CIP `T*` bases
/// (cyclopedia / friends / forge / prey / taskboard / skillwheel).
class IDialogTabPage : public cip::tibia::gamewindow::IDialogTabPage
{
    Q_OBJECT
public:
    explicit IDialogTabPage(QObject* parent = nullptr);
    ~IDialogTabPage() override;

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;
};

/// Qtclient twin of CIP `gamewindow::ICyclopediaDialogTab` (hide/show + restoreContext).
/// Live cyclopedia tabs inherit `cip::…::ICyclopediaDialogTab` via CIP `T*` bases;
/// `TCyclopediaDialogController::page` dispatches show/hide through that interface.
class ICyclopediaDialogTab : public cip::tibia::gamewindow::ICyclopediaDialogTab
{
    Q_OBJECT
public:
    explicit ICyclopediaDialogTab(QObject* parent = nullptr);
    ~ICyclopediaDialogTab() override;

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;
    /// CIP `restoreContext(TCyclopediaDialogContext)` — tip-safe map floor
    /// (CIP context type is not in cip_gen). Live backlog restore stays on the
    /// concrete qtclient page controllers' `Context` structs.
    void restoreContext(const QVariantMap& CyclopediaContext = {});
};

/// Qtclient twin of CIP `gamewindow::ICyclopediaMapSelectionInfo` (`selectionType`).
/// Live Area / SubArea / Passage qtclient classes set `m_selectionType` on the CIP base.
class ICyclopediaMapSelectionInfo : public cip::tibia::gamewindow::ICyclopediaMapSelectionInfo
{
    Q_OBJECT
    Q_PROPERTY(::cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType selectionType READ selectionType CONSTANT)
public:
    explicit ICyclopediaMapSelectionInfo(QObject* parent = nullptr);
    ~ICyclopediaMapSelectionInfo() override;

    ::cip::tibia::qmlcomponents::enums::ECyclopediaMapSelectionType selectionType() const override;
};

IDialogTabPage* tipIDialogTabPage();
ICyclopediaDialogTab* tipICyclopediaDialogTab();
ICyclopediaMapSelectionInfo* tipICyclopediaMapSelectionInfo();

} // namespace gamewindow
} // namespace tibia
