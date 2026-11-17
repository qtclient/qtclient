#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "news/newsentrymodels.h"
#include "news/newsstorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVector>

#include <vector>

namespace tibia {
namespace news {
class TNewsRequestUploader;
}
}

namespace tibia {
namespace gamewindow {

/// NewsDialog.qml (CIP: TNewsDialogController): the news storage's entries of
/// one category (0 client features .. 4 support; the tab is category + 1) and
/// the selected entry. Without a choice of the player the dialog opens on the
/// category of the newest unread entry.
class TNewsDialogController final : public TDialogController<cip::tibia::gamewindow::TNewsDialogController>
{
    Q_OBJECT
public:
    explicit TNewsDialogController(news::TNewsStorage* storage, QObject* parent = nullptr);

    void setNewsUploader(news::TNewsRequestUploader* uploader);

    /// CIP: OPEN_NEWS_DIALOG (an entry id in parameter selects that entry),
    /// OPEN_NEWS_CATEGORY (the category in parameter) and REFRESH_COMPENDIUM_NEWS.
    void handleGameAction(const input::IGameAction& action);

    EDialogTab dialogTab() const override;
    QString headline() const override;
    QString contentText() const override;

    void showDialog() override;
    /// CIP: closes the dialog and forgets category and selection.
    void closeDialog() override;
    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE void requestTabSwitch(cip::tibia::gamewindow::TNewsDialogController::EDialogTab NewTab) override;
    Q_INVOKABLE void setDialogCategory(uint ID) override;
    Q_INVOKABLE void linkClicked(const QString& Url) override;
    Q_INVOKABLE void linkHoverChanged(bool IsOverLink) override;
    void onNewsQueryFailed(const QString& ErrorMessage) override;
    void requestUpdateNewsFromWebsite() override;
    /// CIP slot: the category of the newest (unread) entry once there is data.
    void selectCategoryWithNewestEntryOrNewestUnreadEntryIfDataAvailableAndCategoryChanged();
    void onNewsStorageContentsChanged();

private:
    /// CIP: the player picked `category`.
    void setCategory(int category);
    /// CIP: the entry `entryId` and its category are shown when the dialog opens.
    bool selectEntryForOpening(uint entryId);
    /// CIP: the model shows the current category's entries.
    void refresh();
    /// CIP: the model takes `entries`; the selection stays when it is in the
    /// category, a pending entry wins, else the first row is selected.
    void setEntries(const QVector<news::NewsItem>& entries);
    void resetCategoryAndSelection();

    QPointer<news::TNewsStorage> m_storage;
    QPointer<news::TNewsRequestUploader> m_uploader;
    TNewsEntryModel m_model;
    int m_category = news::NewsItem::UsefulInfo;
    uint m_pendingEntry = 0;
    bool m_userChoseCategory = false;
};

/// Button bar Compendium / News button (CIP: TNewsButtonController).
class TNewsButtonController final : public cip::tibia::gamewindow::TNewsButtonController
{
    Q_OBJECT
public:
    explicit TNewsButtonController(QObject* parent = nullptr);
    ~TNewsButtonController() override;

    static TNewsButtonController* instance();

    bool highlighted() const override { return m_highlightTimer.isActive(); }
    void onOpenNewsButtonClicked() override;
    void onHighlightNewsButtonEnds() override;

    /// Highlight for ten minutes (CIP ShowGameNews / unread push).
    void highlight();
    /// CIP onShowGameNewsMessage — open dialog or highlight from S 0x98 / onGameNews.
    void onShowGameNews(int newsType, bool notificationOnly);

private:
    void stopHighlight();

    static TNewsButtonController* s_instance;
    QTimer m_highlightTimer;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
