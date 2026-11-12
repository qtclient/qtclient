#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
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

/// NewsDialog.qml (CIP: TNewsDialogController) — CIP gamenews trees via TNewsStorage.
/// Button-bar label is "Compendium"; QML caption stays CIP's news_dialog_caption.
class TNewsDialogController final : public TDialogController<cip::tibia::gamewindow::TNewsDialogController>
{
    Q_OBJECT
public:
    explicit TNewsDialogController(news::TNewsStorage* storage, QObject* parent = nullptr);

    void setNewsUploader(news::TNewsRequestUploader* uploader);

    void showDialog() override;
    /// Open on `tab` (CIP OPEN_NEWS_CATEGORY / EDialogTab). NoTabInitialState picks
    /// the tab with the newest unread entry when data is available.
    void showDialog(cip::tibia::gamewindow::TNewsDialogController::EDialogTab tab);
    void closeDialog() override;
    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE void requestTabSwitch(cip::tibia::gamewindow::TNewsDialogController::EDialogTab NewTab) override;
    Q_INVOKABLE void setDialogCategory(uint ID) override;
    Q_INVOKABLE void linkClicked(const QString& Url) override;
    Q_INVOKABLE void linkHoverChanged(bool IsOverLink) override;
    void onNewsQueryFailed(const QString& ErrorMessage) override;
    void requestUpdateNewsFromWebsite() override;
    /// CIP slot (not in generated base); picks newest unread tab when data changes.
    void selectCategoryWithNewestEntryOrNewestUnreadEntryIfDataAvailableAndCategoryChanged();

private:
    void rebuildFromStorage();
    void selectItem(uint categoryId);
    QVector<news::NewsItem> itemsForTab(EDialogTab tab) const;
    EDialogTab preferredTab() const;

    QPointer<news::TNewsStorage> m_storage;
    QPointer<news::TNewsRequestUploader> m_uploader;
    TNewsEntryModel m_model;
    QVector<news::NewsItem> m_flatLookup;
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
