#include "magicalarchivedialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <QFile>
#include <QJsonDocument>
#include <QJSEngine>
#include <QLocale>

#include <algorithm>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

}

TMagicalArchiveDialogController::TMagicalArchiveDialogController(QObject* parent)
    : cip::tibia::gamewindow::TMagicalArchiveDialogController(parent)
    , m_proxy(new TSpellListDataModelSortFilterProxy(this))
{
    m_spellListModel = m_proxy;

    const QString path = spells::spellsAssetPath(QStringLiteral("spells-previews.json"));
    QFile file(path);
    if (!path.isEmpty() && file.open(QIODevice::ReadOnly))
        m_previewsVariant = QJsonDocument::fromJson(file.readAll()).toVariant();
    else
        qWarning("spells-previews.json missing (make extract → data/assets/spells-previews.json)");
}

QJSValue TMagicalArchiveDialogController::spellsPreviewsJson() const
{
    if (QJSEngine* engine = qjsEngine(const_cast<TMagicalArchiveDialogController*>(this))) {
        if (m_previewsJs.isUndefined() || m_previewsJs.isNull())
            m_previewsJs = engine->toScriptValue(m_previewsVariant);
        return m_previewsJs;
    }
    return {};
}

void TMagicalArchiveDialogController::setSelectedSpellID(const int v)
{
    if (m_selectedSpellID == v)
        return;
    m_selectedSpellID = v;
    refreshSelectionFlags();
    Q_EMIT selectedSpellIDChanged();
}

/// CIP: any selected spell id is a valid selection.
void TMagicalArchiveDialogController::refreshSelectionFlags()
{
    m_isValidSpellSelected = m_selectedSpellID != 0;
}

void TMagicalArchiveDialogController::showDialogTab()
{
    m_proxy->resetFilter();
    m_proxy->onPlayerDataChanged();
    // Re-convert previews now that this controller is in a QML tree (CONSTANT
    // property was often empty on the first read before qjsEngine was set).
    m_previewsJs = QJSValue();
    if (QJSEngine* engine = qjsEngine(this))
        m_previewsJs = engine->toScriptValue(m_previewsVariant);
    if (m_initialSpellID == 0 && m_proxy->rowCount() > 0) {
        const QVariant row = m_proxy->getModelDataForRow(0);
        m_initialSpellID = row.toMap().value(QStringLiteral("enumId")).toInt();
    }
    setSelectedSpellID(m_initialSpellID);
    Q_EMIT dataChanged();
    Q_EMIT selectedSpellIDChanged();
}

void TMagicalArchiveDialogController::hideDialogTab() { m_proxy->resetFilter(); }

void TMagicalArchiveDialogController::requestAssignSpellToActionButton()
{
    if (m_selectedSpellID <= 0)
        return;
    auto action = input::mutableClone(*input::makeAction(A::REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON));
    action->parameter = m_selectedSpellID;
    action->actionBarId = 1;
    action->actionButtonId = 0;
    input::publish(action);
}

}
