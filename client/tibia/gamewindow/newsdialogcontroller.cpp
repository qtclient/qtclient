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
    , m_entries(0, false)
{
    m_entryModel = &m_entries;
    if (m_storage)
        connect(m_storage, &news::TNewsStorage::contentsChanged, this, &TNewsDialogController::onNewsStorageContentsChanged);
    onNewsStorageContentsChanged();
}

TNewsDialogController::~TNewsDialogController()
{
    setEntries({});
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

void TNewsDialogController::handleGameAction(const input::TGameAction& action)
{
    switch (action.type) {
        case A::REFRESH_COMPENDIUM_NEWS:
            if (m_uploader)
                m_uploader->requestUpdate();
            return;
        case A::OPEN_NEWS_CATEGORY:
            showDialog();
            if (m_storage && m_category != action.parameter) {
                m_category = action.parameter;
                m_userChoseCategory = true;
                Q_EMIT selectedCategoryChanged();
                refresh();
            }
            return;
        case A::OPEN_NEWS_DIALOG:
            if (action.parameter != 0)
                selectEntryForOpening(static_cast<uint>(action.parameter));
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
    setCategory(category < 5 ? static_cast<int>(category) : news::NewsItem::UsefulInfo);
}

void TNewsDialogController::refresh()
{
    setEntries(m_storage ? m_storage->entriesOfCategory(m_category) : QVector<news::NewsItem>{});
}

void TNewsDialogController::setEntries(const QVector<news::NewsItem>& entries)
{
    std::vector<TAbstractNewsEntry> rows;
    rows.reserve(static_cast<size_t>(entries.size()));
    for (const news::NewsItem& item : entries) {
        TAbstractNewsEntry row;
        row.categoryId = item.id > 0 ? static_cast<uint>(item.id) : 0u;
        row.headline = item.headline;
        row.displayFrom = QDateTime::fromSecsSinceEpoch(item.publishDate);
        row.unlockLevel = static_cast<ushort>(item.unlockLevel);
        row.isUnlocked = item.unlocked;
        row.isRead = item.isRead;
        row.groupHeaderID = item.groupHeaderId > 0 ? static_cast<uint>(item.groupHeaderId) : 0u;
        row.isGroupHeader = item.typeId() == -1;
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
    if (const std::optional<news::NewsItem> current = m_storage ? m_storage->entry(m_dialogCategory) : std::nullopt;
        current && current->categoryId() == m_category)
        selected = m_dialogCategory;
    if (!entries.isEmpty()) {
        if (m_pendingEntry != 0
            && std::any_of(entries.begin(), entries.end(), [this](const news::NewsItem& item) { return uint(item.id) == m_pendingEntry; })) {
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
    const std::optional<news::NewsItem> entry = m_storage ? m_storage->entry(ID) : std::nullopt;
    if (!entry)
        return;
    // A group header without text shows its first entry.
    if (entry->typeId() == -1 && entry->message.isEmpty()) {
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
    auto status = input::mutableClone(*input::makeAction(A::SET_STATUS_LINE));
    status->text = qtTrId("news_error_text");
    status->color = chat::systemWhite();
    input::publish(status);
}

void TNewsDialogController::requestUpdateNewsFromWebsite()
{
    auto refresh = input::mutableClone(*input::makeAction(A::REFRESH_COMPENDIUM_NEWS));
    refresh->parameter = 1;
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
    input::publish(input::makeAction(A::OPEN_NEWS_DIALOG));
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
    input::publish(input::makeAction(A::REFRESH_COMPENDIUM_NEWS));
    if (notificationOnly) {
        highlight();
        return;
    }
    using ENewsTab = cip::tibia::gamewindow::TNewsDialogController::EDialogTab;
    auto action = input::mutableClone(*input::makeAction(A::OPEN_NEWS_CATEGORY));
    switch (newsType) {
    case int(ENewsTab::ClientFeatures):
    case int(ENewsTab::UsefulInfo):
    case int(ENewsTab::PlayerGuide):
    case int(ENewsTab::MajorUpdates):
    case int(ENewsTab::Support):
        action->parameter = newsType;
        break;
    default:
        action->parameter = int(ENewsTab::NoTabInitialState);
        break;
    }
    input::publish(action);
}

void TNewsButtonController::stopHighlight()
{
    if (!m_highlightTimer.isActive())
        return;
    m_highlightTimer.stop();
    Q_EMIT changed();
}

}
