#pragma once

#include "gamewindow/dialogmanager.h"
#include "news/newsstorage.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVector>

namespace tibia::news {
class TNewsRequestUploader;
}

namespace tibia::gamewindow {

/// Child rows under a GROUP HEADER (NewsDialog.qml `subEntryModel`).
class TNewsSubEntryListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int categoryIdEnumValue READ categoryIdEnumValue CONSTANT)
    Q_PROPERTY(int headlineEnumValue READ headlineEnumValue CONSTANT)
    Q_PROPERTY(int isReadEnumValue READ isReadEnumValue CONSTANT)
public:
    enum Roles { CategoryIdRole = Qt::UserRole + 1, HeadlineRole, IsReadRole };
    explicit TNewsSubEntryListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int categoryIdEnumValue() const { return CategoryIdRole; }
    int headlineEnumValue() const { return HeadlineRole; }
    int isReadEnumValue() const { return IsReadRole; }

    void setItems(QVector<news::NewsItem> items);
    const news::NewsItem* itemByCategoryId(uint id) const;

Q_SIGNALS:
    void countChanged();

private:
    QVector<news::NewsItem> m_items;
};

/// Tab-scoped entry list for NewsDialog.qml (categoryId / headline / isRead / subEntryModel).
class TNewsEntryListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { CategoryIdRole = Qt::UserRole + 1, HeadlineRole, IsReadRole, SubEntryModelRole };
    explicit TNewsEntryListModel(QObject* parent = nullptr);
    ~TNewsEntryListModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRoots(QVector<news::NewsItem> roots, QVector<QVector<news::NewsItem>> children);
    const news::NewsItem* itemByCategoryId(uint id) const;

private:
    struct Row {
        news::NewsItem item;
        TNewsSubEntryListModel* sub = nullptr;
    };
    void clearRows();

    QVector<Row> m_rows;
};

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
    TNewsEntryListModel m_model;
    QVector<news::NewsItem> m_flatLookup;
};

/// Button bar Compendium / News button (CIP: TNewsButtonController).
class TNewsButtonController final : public cip::tibia::gamewindow::TNewsButtonController
{
    Q_OBJECT
public:
    explicit TNewsButtonController(QObject* parent = nullptr);

    bool highlighted() const override { return m_highlightTimer.isActive(); }
    void onOpenNewsButtonClicked() override;
    void onHighlightNewsButtonEnds() override;

    /// Highlight for ten minutes (CIP ShowGameNews / unread push).
    void highlight();

private:
    void stopHighlight();

    QTimer m_highlightTimer;
};

}
