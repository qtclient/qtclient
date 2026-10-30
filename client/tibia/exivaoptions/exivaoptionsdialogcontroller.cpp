#include "exivaoptionsdialogcontroller.h"

#include "exivaoptions/exivaoptionsstorage.h"
#include "input/gameactionhandlers.h"

#include <QStringList>

namespace tibia::exivaoptions {

namespace {

using A = input::EGameAction;

/// CIP EWorldType's Optional PvP.
constexpr int WorldTypeOptionalPvp = 1;

/// CIP 0x100d2df80: one name per line, each line ended.
QString joined(const TExivaOptionsStorage::Names& names)
{
    QString text;
    for (const QString& name : names)
        text += name + QLatin1Char('\n');
    return text;
}

TExivaOptionsStorage::Names lines(const QString& text)
{
    TExivaOptionsStorage::Names names;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (!line.isEmpty())
            names.insert(line);
    }
    return names;
}

}

TExivaOptionsDialogController::TExivaOptionsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ExivaOptionsDialog.qml"), parent)
{
    m_onlyOptionalPvp.setButtons({ new gamewindow::TMessageDialogButton(qtTrId("ok"), gamewindow::TMessageDialogController::Button::OK,
                                                                        QKeySequence(QStringLiteral("Return"))) });
    m_onlyOptionalPvp.setCloseOnButtonClicked(true);
    input::setActionHandler(A::OPEN_EXIVA_OPTIONS_DIALOG, [this](const input::TGameAction& action) { return handleAction(action); });
}

TExivaOptionsDialogController::~TExivaOptionsDialogController()
{
    input::setActionHandler(A::OPEN_EXIVA_OPTIONS_DIALOG, nullptr);
}

void TExivaOptionsDialogController::setStorage(TExivaOptionsStorage* storage, std::function<int()> worldType)
{
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_storage = storage;
    m_worldType = std::move(worldType);
    if (storage) {
        connect(storage, &TExivaOptionsStorage::optionsChanged, this, &TExivaOptionsDialogController::onAllowEveryoneChanged);
        connect(storage, &TExivaOptionsStorage::optionsChanged, this, &TExivaOptionsDialogController::onAllowedGroupsChanged);
        connect(storage, &TExivaOptionsStorage::characterWhitelistChanged, this, &TExivaOptionsDialogController::characterWhitelistChanged);
        connect(storage, &TExivaOptionsStorage::guildWhitelistChanged, this, &TExivaOptionsDialogController::guildWhitelistChanged);
    }
    onAllowEveryoneChanged();
    onAllowedGroupsChanged();
}

/// CIP 0x100d2e260.
bool TExivaOptionsDialogController::handleAction(const input::TGameAction& action)
{
    if (action.type != A::OPEN_EXIVA_OPTIONS_DIALOG)
        return false;
    showDialog();
    return true;
}

void TExivaOptionsDialogController::showDialog()
{
    if (m_worldType && m_worldType() != WorldTypeOptionalPvp) {
        // CIP 0x100d2e310.
        m_onlyOptionalPvp.showMessage(qtTrId("exivaoptions_only_optional_pvp_caption"), qtTrId("exivaoptions_only_optional_pvp_text"));
        return;
    }
    onAllowEveryoneChanged();
    onAllowedGroupsChanged();
    dialog()->create();
    dialog()->show();
}

void TExivaOptionsDialogController::closeDialog()
{
    dialog()->close();
}

void TExivaOptionsDialogController::closeButtonClicked()
{
    closeDialog();
}

/// CIP 0x100d2e490: the dialog's flags and the edited whitelists, one name per line.
void TExivaOptionsDialogController::applyButtonClicked(const QString& CharacterWhitelistString, const QString& GuildWhitelistString)
{
    if (!m_storage)
        return;
    const TExivaOptionsStorage::Options options{ m_allowEveryone, m_allowGuild, m_allowParty, m_allowVIPs, m_allowCharacterWhitelist,
                                                 m_allowGuildWhitelist };
    m_storage->update(options, lines(CharacterWhitelistString), lines(GuildWhitelistString));
}

void TExivaOptionsDialogController::onAllowEveryoneChanged()
{
    m_allowEveryone = m_storage && m_storage->options().allowAll;
    Q_EMIT allowEveryoneChanged();
}

void TExivaOptionsDialogController::onAllowedGroupsChanged()
{
    const TExivaOptionsStorage::Options options = m_storage ? m_storage->options() : TExivaOptionsStorage::Options{};
    m_allowGuild = options.allowOwnGuild;
    m_allowParty = options.allowOwnParty;
    m_allowVIPs = options.allowVipList;
    m_allowCharacterWhitelist = options.allowPlayerWhitelist;
    m_allowGuildWhitelist = options.allowGuildWhitelist;
    Q_EMIT allowedGroupsChanged();
}

/// CIP 0x100d2df00.
QString TExivaOptionsDialogController::characterWhitelist() const
{
    return m_storage ? joined(m_storage->characterWhitelist()) : QString();
}

/// CIP 0x100d2e1e0.
QString TExivaOptionsDialogController::guildWhitelist() const
{
    return m_storage ? joined(m_storage->guildWhitelist()) : QString();
}

}
