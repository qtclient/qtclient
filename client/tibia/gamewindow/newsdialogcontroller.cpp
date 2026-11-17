#include "newsdialogcontroller.h"

#include "chat/chatchannel.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
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
        file.open(QIODevice::ReadOnly);
        return QStringLiteral("<style type=\"text/css\">\n") + QString::fromUtf8(file.readAll()) + QStringLiteral("\n</style>\n");
    }();
    return style;
}

}

TNewsDialogController::TNewsDialogController(news::TNewsStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("NewsDialog.qml"), parent)
    , m_storage(storage)
    , m_model(this)
{
    m_entryModel = &m_model;
    if (m_storage)
        connect(m_storage, &news::TNewsStorage::contentsChanged, this, &TNewsDialogController::onNewsStorageContentsChanged);
    onNewsStorageContentsChanged();
}

void TNewsDialogController::setNewsUploader(news::TNewsRequestUploader* uploader)
{
    m_uploader = uploader;
}

EDialogTab TNewsDialogController::dialogTab() const
{
    return static_cast<uint>(m_category) < 5 ? static_cast<EDialogTab>(m_category + 1) : EDialogTab::NoTabInitialState;
}

QString TNewsDialogController::headline() const
{
    const std::optional<news::NewsItem> entry = m_storage ? m_storage->entry(m_dialogCategory) : std::nullopt;
    return entry ? entry->headline : qtTrId("dummy_unknown");
}

QString TNewsDialogController::contentText() const
{
    const std::optional<news::NewsItem> entry = m_storage ? m_storage->entry(m_dialogCategory) : std::nullopt;
    return entry ? contentStyle() + entry->message : qtTrId("dummy_unknown");
}

void TNewsDialogController::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
        case A::REFRESH_COMPENDIUM_NEWS:
            if (m_uploader)
                m_uploader->requestUpdate();
            return;
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
            if (const auto* open = input::gameActionAs<input::TGameActionOpenNewsDialog>(&action); open && open->newsId())
                selectEntryForOpening(*open->newsId());
            showDialog();
            return;
        default:
            return;
    }
}

bool TNewsDialogController::selectEntryForOpening(const uint entryId)
{
    const std::optional<news::NewsItem> entry = m_storage ? m_storage->entry(entryId) : std::nullopt;
    if (!entry)
        return false;
    m_category = entry->categoryId();
    m_pendingEntry = static_cast<uint>(entry->id);
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
    m_category = news::NewsItem::UsefulInfo;
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
    setCategory(category < 5 ? static_cast<int>(category) : news::NewsItem::UsefulInfo);
}

void TNewsDialogController::refresh()
{
    setEntries(m_storage ? m_storage->entriesOfCategory(m_category) : QVector<news::NewsItem>{});
}

void TNewsDialogController::setEntries(const QVector<news::NewsItem>& entries)
{
    // Group headers are rows of their own; an entry of a header in the list
    // becomes its sub row.
    QVector<news::NewsItem> roots;
    QVector<QVector<news::NewsItem>> children;
    QHash<int, int> headerRow;
    for (const news::NewsItem& item : entries) {
        if (item.isGroupHeader()) {
            headerRow.insert(item.id, static_cast<int>(roots.size()));
            roots.push_back(item);
            children.push_back({});
        }
    }
    for (const news::NewsItem& item : entries) {
        if (item.isGroupHeader())
            continue;
        if (item.groupHeaderId > 0 && headerRow.contains(item.groupHeaderId)) {
            children[headerRow.value(item.groupHeaderId)].push_back(item);
            continue;
        }
        roots.push_back(item);
        children.push_back({});
    }
    m_model.setRoots(roots, children);

    uint selected = 0;
    if (const std::optional<news::NewsItem> current = m_storage ? m_storage->entry(m_dialogCategory) : std::nullopt;
        current && current->categoryId() == m_category)
        selected = m_dialogCategory;
    if (!entries.isEmpty()) {
        if (m_pendingEntry != 0
            && std::any_of(entries.begin(), entries.end(), [this](const news::NewsItem& item) { return uint(item.id) == m_pendingEntry; })) {
            selected = m_pendingEntry;
            m_pendingEntry = 0;
        }
        if (selected == 0 && m_model.rowCount() > 0)
            selected = m_model.data(m_model.index(0), TNewsEntryModel::CategoryIdRole).toUInt();
    }
    setDialogCategory(selected);
}

void TNewsDialogController::setDialogCategory(const uint ID)
{
    if (m_dialogCategory == ID)
        return;
    const std::optional<news::NewsItem> entry = m_storage ? m_storage->entry(ID) : std::nullopt;
    if (!entry)
        return;
    // A group header without text shows its first entry.
    if (entry->typeId() == -1 && entry->message.isEmpty()) {
        uint first = 0;
        for (int row = 0; row < m_model.rowCount(); ++row) {
            const QModelIndex index = m_model.index(row);
            if (m_model.data(index, TNewsEntryModel::CategoryIdRole).toUInt() != ID)
                continue;
            if (auto* sub = qobject_cast<QAbstractItemModel*>(m_model.data(index, TNewsEntryModel::SubEntryModelRole).value<QObject*>());
                sub && sub->rowCount() > 0)
                first = sub->data(sub->index(0, 0), TNewsEntrySortProxyModel::CategoryIdRole).toUInt();
            break;
        }
        setDialogCategory(first);
        return;
    }
    m_userChoseCategory = true;
    m_dialogCategory = ID;
    Q_EMIT selectedEntryChanged();
    m_storage->setEntryRead(static_cast<uint>(entry->id), true);
}

void TNewsDialogController::linkClicked(const QString& Url)
{
    input::publish(std::make_shared<input::TGameActionOpenUrl>(Url));
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
    input::publish(status);
}

void TNewsDialogController::requestUpdateNewsFromWebsite()
{
    auto refresh = std::make_shared<input::TGameActionRefreshCompendiumNews>(1 != 0);
    input::publish(refresh);
}

TNewsButtonController* TNewsButtonController::s_instance = nullptr;

TNewsButtonController::TNewsButtonController(QObject* parent)
    : cip::tibia::gamewindow::TNewsButtonController(parent)
{
    s_instance = this;
    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(600000);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TNewsButtonController::changed);

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onGameNews", [this](GameObject*, const std::vector<std::any>& args) {
        const int newsType = static_cast<int>(client::gamecore::argInt(args, 0));
        const bool notificationOnly = args.size() > 1 && client::gamecore::argBool(args, 1);
        onShowGameNews(newsType, notificationOnly);
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
    input::publish(std::make_shared<input::TGameActionOpenNewsDialog>());
    stopHighlight();
}

void TNewsButtonController::onHighlightNewsButtonEnds()
{
    stopHighlight();
}

void TNewsButtonController::highlight()
{
    m_highlightTimer.start();
    Q_EMIT changed();
}

void TNewsButtonController::onShowGameNews(const int newsType, const bool notificationOnly)
{
    // Refresh portal gamenews so the dialog / unread badges stay current.
    input::publish(std::make_shared<input::TGameActionRefreshCompendiumNews>(false));
    if (notificationOnly) {
        highlight();
        return;
    }
    using ENewsTab = cip::tibia::gamewindow::TNewsDialogController::EDialogTab;
    int category = int(ENewsTab::NoTabInitialState);
    switch (newsType) {
    case int(ENewsTab::ClientFeatures):
    case int(ENewsTab::UsefulInfo):
    case int(ENewsTab::PlayerGuide):
    case int(ENewsTab::MajorUpdates):
    case int(ENewsTab::Support):
        category = newsType;
        break;
    default:
        break;
    }
    input::publish(std::make_shared<input::TGameActionOpenNewsDialogWithCategory>(category));
}

void TNewsButtonController::stopHighlight()
{
    if (!m_highlightTimer.isActive())
        return;
    m_highlightTimer.stop();
    Q_EMIT changed();
}

}
