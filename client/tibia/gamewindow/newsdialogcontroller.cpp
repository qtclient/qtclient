#include "newsdialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "news/newsrequestuploader.h"
#include "news/newsstorage.h"

#include <QDesktopServices>
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

TNewsButtonController* TNewsButtonController::s_instance = nullptr;

TNewsButtonController::TNewsButtonController(QObject* parent)
    : cip::tibia::gamewindow::TNewsButtonController(parent)
{
    s_instance = this;
    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(600000);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TNewsButtonController::changed);

    // Keep-alive; ProtocolGame::parseGameNews prefers instance()->onShowGameNews.
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
