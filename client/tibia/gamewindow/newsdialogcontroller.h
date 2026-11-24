#pragma once

#include "client/gamecore.h"
#include "highlighttimer.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "input/gameactiondispatch.h"
#include "news/newsentrymodels.h"
#include "news/newsstorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVector>

#include <QUrl>

#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace authentication {
class TAuthenticationProcessController;
}
namespace config {
class TClientOptions;
}
namespace game {
class IGameSession;
}
namespace news {
class TNewsProtocolMessageHandler;
class TNewsRequestUploader;
class TNewsViewsUploader;
}
namespace shared {
class IClockService;
class TTibiaFileSystemHelper;
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
    /// CIP: made by the client with the client services URL, its news storage,
    /// the client options, the authentication process controller, the file
    /// system helper, the clock, the engine and the dialog parent item. The
    /// dialog makes its own news request uploader (it fills the storage) and
    /// news views uploader.
    TNewsDialogController(const QUrl& ClientServicesUrl, std::shared_ptr<news::TNewsStorage> NewsStorage,
                          config::TClientOptions* ClientOptions,
                          std::shared_ptr<authentication::TAuthenticationProcessController> AuthenticationProcessController,
                          std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                          std::shared_ptr<shared::IClockService> ClockService, QQmlEngine* Engine,
                          QQuickItem* DialogParentItem, QObject* parent = nullptr);
    /// CIP: the entry list is emptied.
    ~TNewsDialogController() override;

    /// CIP: closes the dialog, keeps the session, hands the news storage to the
    /// session's news protocol handler and asks the news uploader for an update.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

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
    void setEntries(const std::vector<news::TNewsEntry>& entries);
    void resetCategoryAndSelection();

    std::shared_ptr<game::IGameSession> m_pGameSession;
    std::shared_ptr<news::TNewsStorage> m_storage;
    std::shared_ptr<news::TNewsProtocolMessageHandler> m_pNewsProtocolMessageHandler;
    config::TClientOptions* m_pClientOptions = nullptr;
    std::shared_ptr<authentication::TAuthenticationProcessController> m_pAuthenticationProcessController;
    std::shared_ptr<news::TNewsRequestUploader> m_uploader;
    std::shared_ptr<news::TNewsViewsUploader> m_viewsUploader;
    /// The dialog's entry list over the client's news entry model.
    TNewsEntrySortProxyModel m_entries;
    int m_category = news::NEWS_CATEGORY_USEFUL_INFO;
    uint m_pendingEntry = 0;
    bool m_userChoseCategory = false;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

/// Button bar Compendium / News button (CIP: TNewsButtonController). A game
/// news message keeps the button highlighted and remembers the news it names;
/// a click opens the news dialog on that entry.
class TNewsButtonController final : public cip::tibia::gamewindow::TNewsButtonController
{
    Q_OBJECT
public:
    explicit TNewsButtonController(QObject* parent = nullptr);
    ~TNewsButtonController() override;

    static TNewsButtonController* instance();

    bool highlighted() const override { return m_highlightTimer.isHighlighted(); }
    /// Opens the news dialog on the remembered news, then drops the highlight.
    void onOpenNewsButtonClicked() override;
    void onHighlightNewsButtonEnds() override;

    /// Highlights the button (the news dialog's unread entry check).
    void highlight();
    /// Remembers the news and highlights; unless it is a notification only, a
    /// named news opens the dialog at once.
    void onShowGameNewsMessage(int newsType, bool notificationOnly);

private:
    static TNewsButtonController* s_instance;
    shared::THighlightTimer m_highlightTimer{ 600000 };
    int m_newsType = 0;
};

}
}
