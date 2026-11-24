#include "newsdialogcontroller.h"

#include "chat/chatchannel.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "game/gamesessionbase.h"
#include "jsondocumenthttppostsender.h"
#include "news/newsciptypes.h"
#include "news/newsrequestuploader.h"
#include "news/newsstorage.h"

#include <QDebug>
#include <QFile>
#include <QHash>
#include <QUrl>

#include <algorithm>
#include <any>
#include <vector>
#include <tibia/game/gamenotify.h>

namespace tibia::gamewindow {

namespace {

using EDialogTab = cip::tibia::gamewindow::TNewsDialogController::EDialogTab;
using A = input::EGameAction;

/// CIP: the stylesheet every entry's text starts with.
const QString& contentStyle()
{
    static const QString style = [] {
        QFile file(QStringLiteral(":/css/news.css"));
        if (!file.open(QIODevice::ReadOnly))
            return QString();
        return QStringLiteral("<style type=\"text/css\">\n") + QString::fromUtf8(file.readAll()) + QStringLiteral("\n</style>\n");
    }();
    return style;
}

}

TNewsDialogController::TNewsDialogController(const QUrl& ClientServicesUrl, std::shared_ptr<news::TNewsStorage> NewsStorage,
                                             config::TClientOptions* ClientOptions,
                                             std::shared_ptr<authentication::TAuthenticationProcessController> AuthenticationProcessController,
                                             std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                                             std::shared_ptr<shared::IClockService> ClockService, QQmlEngine* Engine,
                                             QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("NewsDialog.qml"), Engine, DialogParentItem, parent)
    , m_storage(std::move(NewsStorage))
    , m_pClientOptions(ClientOptions)
    , m_pAuthenticationProcessController(std::move(AuthenticationProcessController))
    , m_uploader(std::make_shared<news::TNewsRequestUploader>(ClientServicesUrl, m_storage,
                                                              std::make_shared<shared::TJsonDocumentHttpPostSender>(),
                                                              std::move(TibiaFileSystemHelper), std::move(ClockService)))
    , m_viewsUploader(std::make_shared<news::TNewsViewsUploader>(ClientServicesUrl,
                                                                 std::make_shared<shared::TJsonDocumentHttpPostSender>()))
    , m_entries(0, false)
{
    m_entryModel = &m_entries;
    if (m_storage)
        connect(m_storage.get(), &news::TNewsStorage::contentsChanged, this, &TNewsDialogController::onNewsStorageContentsChanged);
    connect(m_uploader.get(), &news::TNewsRequestUploader::newsQueryFailed, this, &TNewsDialogController::onNewsQueryFailed);
    onNewsStorageContentsChanged();
    for (const A type : { A::OPEN_NEWS_DIALOG, A::OPEN_NEWS_CATEGORY, A::REFRESH_COMPENDIUM_NEWS }) {
        m_gameActions.on(type, [this](const input::IGameAction& action) {
            handleGameAction(action);
            return true;
        });
    }
}

TNewsDialogController::~TNewsDialogController()
{
    setEntries({});
}

void TNewsDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    m_pNewsProtocolMessageHandler.reset();
    m_pGameSession = GameSession;
    if (!m_pGameSession)
        return;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_pGameSession.get()))
        m_pNewsProtocolMessageHandler = session->newsProtocolMessageHandler();
    if (m_pNewsProtocolMessageHandler)
        m_pNewsProtocolMessageHandler->setNewsStorage(m_storage);
    m_uploader->requestUpdate();
}

EDialogTab TNewsDialogController::dialogTab() const
{
    return static_cast<uint>(m_category) < 5 ? static_cast<EDialogTab>(m_category + 1) : EDialogTab::NoTabInitialState;
}

QString TNewsDialogController::headline() const
{
    const std::optional<news::TNewsEntry> entry = m_storage ? m_storage->entry(m_dialogCategory) : std::nullopt;
    return entry ? entry->headline() : qtTrId("dummy_unknown");
}

QString TNewsDialogController::contentText() const
{
    const std::optional<news::TNewsEntry> entry = m_storage ? m_storage->entry(m_dialogCategory) : std::nullopt;
    return entry ? contentStyle() + entry->message() : qtTrId("dummy_unknown");
}

void TNewsDialogController::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
        case A::REFRESH_COMPENDIUM_NEWS: {
            // CIP: with the storage's parameters; the action's flag forces it.
            const auto* refresh = input::gameActionAs<input::TGameActionRefreshCompendiumNews>(&action);
            if (m_uploader && m_storage)
                m_uploader->requestNews({ m_storage->isReturner(), m_storage->showRewardNews(),
                                          static_cast<uint>(m_storage->version()) },
                                        refresh && refresh->flag());
            return;
        }
        case A::OPEN_NEWS_CATEGORY:
            showDialog();
            if (const auto* open = input::gameActionAs<input::TGameActionOpenNewsDialogWithCategory>(&action);
                open && m_storage && m_category != open->category()) {
                m_category = open->category();
                m_userChoseCategory = true;
                Q_EMIT selectedCategoryChanged();
                refresh();
            }
            return;
        case A::OPEN_NEWS_DIALOG:
            // The second optional entry wins; the first one only when set and non-zero.
            if (const auto* open = input::gameActionAs<input::TGameActionOpenNewsDialog>(&action)) {
                if (open->category())
                    selectEntryForOpening(*open->category());
                else if (open->newsId() && *open->newsId() != 0)
                    selectEntryForOpening(*open->newsId());
            }
            showDialog();
            return;
        default:
            return;
    }
}

bool TNewsDialogController::selectEntryForOpening(const uint entryId)
{
    const std::optional<news::TNewsEntry> entry = m_storage ? m_storage->entry(entryId) : std::nullopt;
    if (!entry)
        return false;
    m_category = entry->category();
    m_pendingEntry = entry->id();
    return true;
}

void TNewsDialogController::showDialog()
{
    m_userChoseCategory = false;
    if (m_pendingEntry == 0 && m_storage && m_storage->hasEntries()) {
        const int category = m_storage->categoryOfNewestUnreadOrNewestEntry();
        if (m_category != category) {
            m_category = category;
            Q_EMIT selectedCategoryChanged();
        }
    }
    refresh();
    if (dialog()->isShown())
        return;
    dialog()->create();
    dialog()->show();
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setPointingHand(false);
}

void TNewsDialogController::resetCategoryAndSelection()
{
    m_category = news::NEWS_CATEGORY_USEFUL_INFO;
    m_dialogCategory = 0;
    m_pendingEntry = 0;
    setEntries({});
}

void TNewsDialogController::closeDialog()
{
    dialog()->close();
    resetCategoryAndSelection();
}

void TNewsDialogController::requestClose()
{
    closeDialog();
}

void TNewsDialogController::onNewsStorageContentsChanged()
{
    if (m_pendingEntry == 0 && !m_userChoseCategory && m_storage && m_storage->hasEntries()) {
        const int category = m_storage->categoryOfNewestUnreadOrNewestEntry();
        if (m_category != category) {
            m_category = category;
            Q_EMIT selectedCategoryChanged();
        }
    }
    refresh();
    // CIP: with the dialog not open, an unread unlocked entry highlights the
    // news button.
    if (m_storage && m_storage->hasUnreadUnlockedEntry() && !dialog()->item()) {
        if (TNewsButtonController* button = TNewsButtonController::instance())
            button->highlight();
    }
}

void TNewsDialogController::selectCategoryWithNewestEntryOrNewestUnreadEntryIfDataAvailableAndCategoryChanged()
{
    if (!m_storage || !m_storage->hasEntries())
        return;
    const int category = m_storage->categoryOfNewestUnreadOrNewestEntry();
    if (m_category != category) {
        m_category = category;
        Q_EMIT selectedCategoryChanged();
    }
}

void TNewsDialogController::setCategory(const int category)
{
    if (!m_storage || m_category == category)
        return;
    m_category = category;
    m_userChoseCategory = true;
    Q_EMIT selectedCategoryChanged();
    refresh();
}

void TNewsDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    const uint category = static_cast<uint>(NewTab) - 1u;
    setCategory(category < 5 ? static_cast<int>(category) : news::NEWS_CATEGORY_USEFUL_INFO);
}

void TNewsDialogController::refresh()
{
    setEntries(m_storage ? m_storage->entriesOfCategory(m_category) : std::vector<news::TNewsEntry>{});
}

void TNewsDialogController::setEntries(const std::vector<news::TNewsEntry>& entries)
{
    std::vector<TAbstractNewsEntry> rows;
    rows.reserve(entries.size());
    for (const news::TNewsEntry& item : entries) {
        TAbstractNewsEntry row;
        row.categoryId = item.id();
        row.headline = item.headline();
        row.displayFrom = item.publishDate();
        row.unlockLevel = item.unlockLevel();
        row.isUnlocked = item.isUnlocked();
        row.isRead = item.isRead();
        row.groupHeaderID = item.groupHeaderId().value_or(0);
        row.isGroupHeader = item.type() == news::NEWS_TYPE_GROUP_HEADER;
        // A group header lists its entries from the same model.
        if (row.isGroupHeader)
            row.subEntryModel = std::make_shared<TNewsEntrySortProxyModel>(row.categoryId, true);
        rows.push_back(std::move(row));
    }
    // CIP: the player guide tab shows the newest entries first.
    m_entries.setNewestFirst(dialogTab() == EDialogTab::PlayerGuide);
    TNewsEntryModel& model = TNewsEntryModel::instance();
    model.setEntries(std::move(rows));
    Q_EMIT model.listUpdated();

    uint selected = 0;
    if (const std::optional<news::TNewsEntry> current = m_storage ? m_storage->entry(m_dialogCategory) : std::nullopt;
        current && current->category() == m_category)
        selected = m_dialogCategory;
    if (!entries.empty()) {
        if (m_pendingEntry != 0
            && std::any_of(entries.begin(), entries.end(), [this](const news::TNewsEntry& item) { return item.id() == m_pendingEntry; })) {
            selected = m_pendingEntry;
            m_pendingEntry = 0;
        }
        if (selected == 0)
            selected = m_entries.rowCount() == 0 ? 0u : m_entries.index(0, 0).data(TNewsEntryModel::CategoryIdRole).toUInt();
    }
    setDialogCategory(selected);
}

void TNewsDialogController::setDialogCategory(const uint ID)
{
    if (m_dialogCategory == ID)
        return;
    const std::optional<news::TNewsEntry> entry = m_storage ? m_storage->entry(ID) : std::nullopt;
    if (!entry)
        return;
    // A group header without text shows its first entry.
    if (entry->type() == news::NEWS_TYPE_GROUP_HEADER && entry->message().isEmpty()) {
        uint first = 0;
        for (int row = 0; row < m_entries.rowCount(); ++row) {
            const QModelIndex index = m_entries.index(row, 0);
            if (index.data(TNewsEntryModel::CategoryIdRole).toUInt() != ID)
                continue;
            if (auto* sub = qobject_cast<QAbstractItemModel*>(index.data(TNewsEntryModel::SubEntryModelRole).value<QObject*>());
                sub && sub->rowCount() > 0)
                first = sub->index(0, 0).data(TNewsEntryModel::CategoryIdRole).toUInt();
            break;
        }
        setDialogCategory(first);
        return;
    }
    m_userChoseCategory = true;
    m_dialogCategory = ID;
    Q_EMIT selectedEntryChanged();
    if (m_viewsUploader)
        m_viewsUploader->reportNewsViewed(entry->id());
    m_storage->setEntryRead(entry->id(), true);
}

void TNewsDialogController::linkClicked(const QString& Url)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenUrl>(Url));
}

void TNewsDialogController::linkHoverChanged(const bool IsOverLink)
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setPointingHand(IsOverLink);
}

void TNewsDialogController::onNewsQueryFailed(const QString& ErrorMessage)
{
    qWarning() << "News query failed:" << ErrorMessage;
    auto status = std::make_shared<input::TGameActionSetStatusLine>(qtTrId("news_error_text"), chat::systemWhite());
    Q_EMIT publishGameAction(status);
}

void TNewsDialogController::requestUpdateNewsFromWebsite()
{
    auto refresh = std::make_shared<input::TGameActionRefreshCompendiumNews>(1 != 0);
    Q_EMIT publishGameAction(refresh);
}

TNewsButtonController* TNewsButtonController::s_instance = nullptr;

TNewsButtonController::TNewsButtonController(QObject* parent)
    : cip::tibia::gamewindow::TNewsButtonController(parent)
{
    s_instance = this;
    connect(&m_highlightTimer, &shared::THighlightTimer::changed, this, &TNewsButtonController::changed);

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onGameNews", [this](GameObject*, const std::vector<std::any>& args) {
        const int newsType = static_cast<int>(client::gamecore::argInt(args, 0));
        const bool notificationOnly = args.size() > 1 && client::gamecore::argBool(args, 1);
        onShowGameNewsMessage(newsType, notificationOnly);
        return std::any{};
    });
}

TNewsButtonController::~TNewsButtonController()
{
    if (s_instance == this)
        s_instance = nullptr;
}

TNewsButtonController* TNewsButtonController::instance()
{
    return s_instance;
}

void TNewsButtonController::onOpenNewsButtonClicked()
{
    Q_EMIT publishGameAction(input::TGameActionOpenNewsDialog::withNews(static_cast<uint32_t>(m_newsType)));
    m_highlightTimer.stop(false);
    m_newsType = 0;
}

void TNewsButtonController::onHighlightNewsButtonEnds()
{
    m_highlightTimer.stop(false);
    m_newsType = 0;
}

void TNewsButtonController::highlight()
{
    m_highlightTimer.start(false);
}

void TNewsButtonController::onShowGameNewsMessage(const int newsType, const bool notificationOnly)
{
    m_newsType = newsType;
    m_highlightTimer.start(false);
    if (!notificationOnly && m_newsType != 0) {
        onOpenNewsButtonClicked();
        m_newsType = 0;
    }
}

}
