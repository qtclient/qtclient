#include "newsdialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "news/newsrequestuploader.h"
#include "news/newsstorage.h"

#include <QDesktopServices>
#include <QHash>
#include <QUrl>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using EDialogTab = cip::tibia::gamewindow::TNewsDialogController::EDialogTab;
using A = input::EGameAction;

EDialogTab tabForCategory(const QString& category)
{
    const QString c = category.trimmed().toUpper();
    if (c.contains(QLatin1String("CLIENT")))
        return EDialogTab::ClientFeatures;
    if (c.contains(QLatin1String("SUPPORT")))
        return EDialogTab::Support;
    if (c.contains(QLatin1String("MAJOR")))
        return EDialogTab::MajorUpdates;
    // CIP PlayerGuide tab hosts GAME CONTENTS and PLAYER GUIDE articles.
    if (c.contains(QLatin1String("GAME")) || c.contains(QLatin1String("PLAYER")) || c.contains(QLatin1String("GUIDE")))
        return EDialogTab::PlayerGuide;
    return EDialogTab::UsefulInfo;
}

uint categoryIdOf(const news::NewsItem& item)
{
    return item.id > 0 ? uint(item.id) : 0u;
}

}

TNewsSubEntryListModel::TNewsSubEntryListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TNewsSubEntryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant TNewsSubEntryListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const news::NewsItem& item = m_items.at(index.row());
    switch (role) {
    case CategoryIdRole:
        return categoryIdOf(item);
    case HeadlineRole:
        return item.headline;
    case IsReadRole:
        return item.isRead;
    default:
        return {};
    }
}

QHash<int, QByteArray> TNewsSubEntryListModel::roleNames() const
{
    return {
        { CategoryIdRole, "categoryId" },
        { HeadlineRole, "headline" },
        { IsReadRole, "isRead" },
    };
}

void TNewsSubEntryListModel::setItems(QVector<news::NewsItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    Q_EMIT countChanged();
}

const news::NewsItem* TNewsSubEntryListModel::itemByCategoryId(const uint id) const
{
    for (const news::NewsItem& item : m_items) {
        if (categoryIdOf(item) == id)
            return &item;
    }
    return nullptr;
}

TNewsEntryListModel::TNewsEntryListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

TNewsEntryListModel::~TNewsEntryListModel()
{
    clearRows();
}

void TNewsEntryListModel::clearRows()
{
    for (Row& row : m_rows)
        delete row.sub;
    m_rows.clear();
}

int TNewsEntryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TNewsEntryListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case CategoryIdRole:
        return categoryIdOf(row.item);
    case HeadlineRole:
        return row.item.headline;
    case IsReadRole:
        return row.item.isRead;
    case SubEntryModelRole:
        return row.sub && row.sub->rowCount() > 0 ? QVariant::fromValue<QObject*>(row.sub) : QVariant();
    default:
        return {};
    }
}

QHash<int, QByteArray> TNewsEntryListModel::roleNames() const
{
    return {
        { CategoryIdRole, "categoryId" },
        { HeadlineRole, "headline" },
        { IsReadRole, "isRead" },
        { SubEntryModelRole, "subEntryModel" },
    };
}

void TNewsEntryListModel::setRoots(QVector<news::NewsItem> roots, QVector<QVector<news::NewsItem>> children)
{
    beginResetModel();
    clearRows();
    m_rows.reserve(roots.size());
    for (int i = 0; i < roots.size(); ++i) {
        Row row;
        row.item = std::move(roots[i]);
        if (i < children.size() && !children[i].isEmpty()) {
            row.sub = new TNewsSubEntryListModel(this);
            row.sub->setItems(std::move(children[i]));
        }
        m_rows.push_back(std::move(row));
    }
    endResetModel();
}

const news::NewsItem* TNewsEntryListModel::itemByCategoryId(const uint id) const
{
    for (const Row& row : m_rows) {
        if (categoryIdOf(row.item) == id)
            return &row.item;
        if (row.sub) {
            if (const news::NewsItem* child = row.sub->itemByCategoryId(id))
                return child;
        }
    }
    return nullptr;
}

TNewsDialogController::TNewsDialogController(news::TNewsStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("NewsDialog.qml"), parent)
    , m_storage(storage)
    , m_model(this)
{
    m_entryModel = &m_model;
    m_dialogTab = EDialogTab::UsefulInfo;
    if (m_storage) {
        connect(m_storage, &news::TNewsStorage::contentsChanged, this, [this] { rebuildFromStorage(); });
        rebuildFromStorage();
    }
}

void TNewsDialogController::setNewsUploader(news::TNewsRequestUploader* uploader)
{
    m_uploader = uploader;
}

QVector<news::NewsItem> TNewsDialogController::itemsForTab(const EDialogTab tab) const
{
    QVector<news::NewsItem> out;
    if (!m_storage)
        return out;
    for (const news::NewsItem& item : m_storage->items()) {
        if (tabForCategory(item.category) == tab)
            out.push_back(item);
    }
    std::stable_sort(out.begin(), out.end(), [](const news::NewsItem& a, const news::NewsItem& b) {
        if (a.index != b.index)
            return a.index < b.index;
        return a.publishDate > b.publishDate;
    });
    return out;
}

void TNewsDialogController::rebuildFromStorage()
{
    if (!m_storage)
        return;

    const QVector<news::NewsItem> tabItems = itemsForTab(m_dialogTab);
    m_flatLookup = tabItems;

    QVector<news::NewsItem> roots;
    QVector<QVector<news::NewsItem>> children;
    QHash<int, int> headerRow;

    for (const news::NewsItem& item : tabItems) {
        if (item.isGroupHeader()) {
            headerRow.insert(item.id, roots.size());
            roots.push_back(item);
            children.push_back({});
        }
    }

    for (const news::NewsItem& item : tabItems) {
        if (item.isGroupHeader())
            continue;
        if (item.groupHeaderId > 0 && headerRow.contains(item.groupHeaderId)) {
            children[headerRow.value(item.groupHeaderId)].push_back(item);
            continue;
        }
        roots.push_back(item);
        children.push_back({});
    }

    // No GROUP HEADER rows: each article is a root (flat CIP list still works).
    if (roots.isEmpty() && !tabItems.isEmpty()) {
        for (const news::NewsItem& item : tabItems) {
            if (!item.isGroupHeader()) {
                roots.push_back(item);
                children.push_back({});
            }
        }
    }

    m_model.setRoots(std::move(roots), std::move(children));

    if (!m_flatLookup.isEmpty()) {
        const news::NewsItem* first = nullptr;
        for (const news::NewsItem& item : m_flatLookup) {
            if (!item.isGroupHeader()) {
                first = &item;
                break;
            }
        }
        if (!first)
            first = &m_flatLookup.constFirst();
        selectItem(categoryIdOf(*first));
        Q_EMIT selectedCategoryChanged();
    } else {
        m_headline.clear();
        m_contentText = tr("No news is available right now.");
        m_dialogCategory = 0;
        Q_EMIT selectedEntryChanged();
    }
}

void TNewsDialogController::selectItem(const uint categoryId)
{
    m_dialogCategory = categoryId;
    const news::NewsItem* item = m_model.itemByCategoryId(categoryId);
    if (!item) {
        for (const news::NewsItem& candidate : m_flatLookup) {
            if (categoryIdOf(candidate) == categoryId) {
                item = &candidate;
                break;
            }
        }
    }
    if (item) {
        m_headline = item->headline;
        m_contentText = item->message.isEmpty() ? item->headline : item->message;
        m_dialogTab = tabForCategory(item->category);
    }
    Q_EMIT selectedEntryChanged();
}

EDialogTab TNewsDialogController::preferredTab() const
{
    if (!m_storage)
        return EDialogTab::UsefulInfo;

    const news::NewsItem* newestUnread = nullptr;
    const news::NewsItem* newest = nullptr;
    for (const news::NewsItem& item : m_storage->items()) {
        if (item.isGroupHeader())
            continue;
        if (!newest || item.publishDate > newest->publishDate)
            newest = &item;
        if (!item.isRead && (!newestUnread || item.publishDate > newestUnread->publishDate))
            newestUnread = &item;
    }
    if (newestUnread)
        return tabForCategory(newestUnread->category);
    if (newest)
        return tabForCategory(newest->category);
    return EDialogTab::UsefulInfo;
}

void TNewsDialogController::showDialog()
{
    showDialog(EDialogTab::NoTabInitialState);
}

void TNewsDialogController::showDialog(const EDialogTab tab)
{
    m_dialogTab = tab == EDialogTab::NoTabInitialState ? preferredTab() : tab;
    Q_EMIT selectedCategoryChanged();
    rebuildFromStorage();
    dialog()->show();
}

void TNewsDialogController::closeDialog()
{
    requestClose();
}

void TNewsDialogController::requestClose()
{
    dialog()->close();
}

void TNewsDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    if (m_dialogTab == NewTab)
        return;
    m_dialogTab = NewTab;
    Q_EMIT selectedCategoryChanged();
    rebuildFromStorage();
}

void TNewsDialogController::setDialogCategory(const uint ID)
{
    selectItem(ID);
}

void TNewsDialogController::linkClicked(const QString& Url)
{
    const QUrl url(Url);
    if (url.isValid())
        QDesktopServices::openUrl(url);
}

void TNewsDialogController::linkHoverChanged(bool)
{
}

void TNewsDialogController::onNewsQueryFailed(const QString& ErrorMessage)
{
    m_headline = tr("News");
    m_contentText = ErrorMessage.isEmpty() ? tr("Could not load news.") : ErrorMessage;
    Q_EMIT selectedEntryChanged();
}

void TNewsDialogController::requestUpdateNewsFromWebsite()
{
    if (m_uploader)
        m_uploader->requestUpdate();
    else
        rebuildFromStorage();
}

void TNewsDialogController::selectCategoryWithNewestEntryOrNewestUnreadEntryIfDataAvailableAndCategoryChanged()
{
    const EDialogTab tab = preferredTab();
    if (m_dialogTab == tab)
        return;
    m_dialogTab = tab;
    Q_EMIT selectedCategoryChanged();
    rebuildFromStorage();
}

TNewsButtonController::TNewsButtonController(QObject* parent)
    : cip::tibia::gamewindow::TNewsButtonController(parent)
{
    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(600000);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TNewsButtonController::changed);
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

void TNewsButtonController::stopHighlight()
{
    if (!m_highlightTimer.isActive())
        return;
    m_highlightTimer.stop();
    Q_EMIT changed();
}

}
