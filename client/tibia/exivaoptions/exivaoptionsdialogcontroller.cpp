#include "exivaoptionsdialogcontroller.h"

#include "exivaoptions/exivaoptionsstorage.h"
#include "input/gameactionhandlers.h"

#include <QStringList>

namespace tibia::exivaoptions {

namespace {

using A = input::EGameAction;

/// The world type of Optional PvP worlds.
constexpr int WorldTypeOptionalPvp = 1;

/// One name per line, and always a final line break (an empty list is "\n").
QString joined(const TExivaOptionsStorage::Names& names)
{
    QStringList list;
    list.reserve(static_cast<qsizetype>(names.size()));
    for (const QString& name : names)
        list.append(name);
    return list.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/// The non-empty lines of `text`.
TExivaOptionsStorage::Names lines(const QString& text)
{
    TExivaOptionsStorage::Names names;
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        names.insert(line);
    return names;
}

}

TExivaOptionsDialogController::TExivaOptionsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ExivaOptionsDialog.qml"), parent)
{
    // Until a storage is set, own guild and own party are allowed.
    m_allowGuild = true;
    m_allowParty = true;
    m_onlyOptionalPvp.setButtons({ new gamewindow::TMessageDialogButton(qtTrId("ok"), gamewindow::TMessageDialogController::Button::OK,
                                                                        QKeySequence(QStringLiteral("Return"))) });
    m_onlyOptionalPvp.setCloseOnButtonClicked(true);
    m_gameActions.on(A::OPEN_EXIVA_OPTIONS_DIALOG, [this](const input::IGameAction& action) { return handleAction(action); });
}

TExivaOptionsDialogController::~TExivaOptionsDialogController()
{
}

void TExivaOptionsDialogController::setStorage(TExivaOptionsStorage* storage, std::function<int()> worldType)
{
    closeDialog();
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_storage = storage;
    m_worldType = std::move(worldType);
    if (storage) {
        connect(storage, &TExivaOptionsStorage::allowEveryoneChanged, this, &TExivaOptionsDialogController::onAllowEveryoneChanged);
        connect(storage, &TExivaOptionsStorage::allowedGroupsChanged, this, &TExivaOptionsDialogController::onAllowedGroupsChanged);
        connect(storage, &TExivaOptionsStorage::characterWhitelistChanged, this, &TExivaOptionsDialogController::characterWhitelistChanged);
        connect(storage, &TExivaOptionsStorage::guildWhitelistChanged, this, &TExivaOptionsDialogController::guildWhitelistChanged);
    }
    onAllowEveryoneChanged();
    onAllowedGroupsChanged();
    Q_EMIT characterWhitelistChanged();
    Q_EMIT guildWhitelistChanged();
}

bool TExivaOptionsDialogController::handleAction(const input::IGameAction& action)
{
    if (action.gameActionType() != A::OPEN_EXIVA_OPTIONS_DIALOG)
        return false;
    showDialog();
    return true;
}

void TExivaOptionsDialogController::showDialog()
{
    if (m_worldType && m_worldType() != WorldTypeOptionalPvp) {
        m_onlyOptionalPvp.showMessage(qtTrId("exivaoptions_only_optional_pvp_caption"), qtTrId("exivaoptions_only_optional_pvp_text"));
        return;
    }
    dialog()->create();
    dialog()->show();
}

void TExivaOptionsDialogController::closeDialog()
{
    m_onlyOptionalPvp.dialog()->close();
    dialog()->close();
}

void TExivaOptionsDialogController::closeButtonClicked()
{
    closeDialog();
}

/// The dialog's flags and the edited whitelists, one name per line, become
/// the exiva options update.
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

QString TExivaOptionsDialogController::characterWhitelist() const
{
    return m_storage ? joined(m_storage->characterWhitelist()) : QString();
}

QString TExivaOptionsDialogController::guildWhitelist() const
{
    return m_storage ? joined(m_storage->guildWhitelist()) : QString();
}

}
