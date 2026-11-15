#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

#include <functional>
#include <map>
#include <vector>

class QJsonValue;
class QNetworkAccessManager;
class QNetworkReply;

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// One world of the account service's "Worlds" list (CIP: gamewindow::WorldData).
struct WorldData
{
    QString name;
    QString region;
    QString pvpType;
    int playersOnline = 0;
    /// "MMM yyyy" of CreationDate.
    QString creationDate;
    QString battleEyeStatus;
    bool premiumOnly = false;
    QString transferType;
};

/// A "Worlds" entry of the account service as WorldData.
WorldData parseWorldData(const QJsonValue& value);

/// The worlds of WorldSelection.qml (CipSoft name, no T prefix): a
/// shared::TSimpleVectorAbstractListModel<WorldData> with one role per field.
class WorldSelectionModel final : public cip::tibia::gamewindow::WorldSelectionModel
{
    Q_OBJECT
public:
    enum Role
    {
        WorldNameRole = Qt::UserRole + 1,
        PlayersOnlineRole,
        RegionRole,
        PvPTypeRole,
        CreationDateRole,
        BattleEyeStatusRole,
        PremiumOnlyRole,
        TransferTypeRole,
    };

    explicit WorldSelectionModel(QObject* parent = nullptr);

    /// Rows removed or inserted to the new count, then the kept rows changed.
    void setItems(std::vector<WorldData> worlds);
    const std::vector<WorldData>& items() const { return m_worlds; }

    int count() const override { return rowCount(); }
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<WorldData> m_worlds;
};

/// One option of WorldSelection.qml's region and PvP type combo boxes (CIP builds
/// a gameWidnow::TTextValueQmlObject; same text/value properties).
class TWorldFilterEntry final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text MEMBER m_text CONSTANT)
    Q_PROPERTY(QString value MEMBER m_value CONSTANT)
public:
    TWorldFilterEntry(const QString& text, const QString& value, QObject* parent) : QObject(parent), m_text(text), m_value(value) {}

private:
    QString m_text;
    QString m_value;
};

/// The worldsModel of CreateCharacterDialog.qml / CreateAccountDialog.qml (CIP:
/// TWorldSelectionSortProxyModel): owns the WorldSelectionModel, filters it by
/// region and PvP type and sorts the recommended world first, then by name.
class TWorldSelectionSortProxyModel final : public cip::tibia::gamewindow::TWorldSelectionSortProxyModel
{
    Q_OBJECT
public:
    explicit TWorldSelectionSortProxyModel(QObject* parent = nullptr);

    /// New worlds: rebuilds the region and PvP type options and clears the filters.
    void setWorlds(const std::vector<WorldData>& worlds);
    const std::vector<WorldData>& worlds() const { return m_worlds->items(); }
    void setRecommendedWorld(const QString& world);

    void setWorldRegionFilter(const QString& v) override;
    void setWorldPvPTypeFilter(const QString& v) override;
    /// The PvP types of the filtered region, or of every world.
    QList<QObject*> worldPvPTypeFilterModel() const override;

    Q_INVOKABLE QString worldName(const QModelIndex& index) override;
    Q_INVOKABLE QString playersOnline(const QModelIndex& index) override;
    Q_INVOKABLE QString worldPvPType(const QModelIndex& index) override;
    Q_INVOKABLE QString worldRegion(const QModelIndex& index) override;
    Q_INVOKABLE QString worldCreationDate(const QModelIndex& index) override;
    Q_INVOKABLE QString worldBattleEyeStatus(const QModelIndex& index) override;
    Q_INVOKABLE QString worldPremiumOnly(const QModelIndex& index) override;
    Q_INVOKABLE QString worldTransferType(const QModelIndex& index) override;
    Q_INVOKABLE void resetFilter() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString roleText(const QModelIndex& index, int role) const;

    WorldSelectionModel* m_worlds;
    /// Per region: the PvP type options of its worlds.
    std::map<QString, QList<QObject*>> m_worldPvPTypeFilterModelByRegion;
};

/// CreateCharacterDialog.qml's controller (CIP: TCreateCharacterDialogController):
/// asks the account service for the worlds, checks and suggests character names
/// and creates the character, all as JSON posts.
class TCreateCharacterDialogController : public TDialogController<cip::tibia::gamewindow::TCreateCharacterDialogController>
{
    Q_OBJECT
public:
    /// `qmlFile`: see TDialog.
    explicit TCreateCharacterDialogController(const QString& qmlFile = QStringLiteral("createaccountandcharacter/CreateCharacterDialog.qml"),
                                              QObject* parent = nullptr);
    ~TCreateCharacterDialogController() override;

    /// The account service (CIP accountcreationclientservices.php on the portal).
    void setServiceUrl(const QUrl& url) { m_serviceUrl = url; }
    /// The login session the new character goes to.
    void setSessionKey(const QString& sessionKey) { m_sessionKey = sessionKey; }

    /// SHOW_CREATE_CHARACTER_DIALOG; ignored while a request is pending. The
    /// parameter: the account has no characters yet, so the dialog can be neither
    /// cancelled nor skip the tutorial.
    void open(bool accountHasNoCharacters);

    /// Closes the dialog and asks for the worlds; the answer shows it.
    void showDialog() override;
    /// Back to the defaults, worlds dropped, dialog closed.
    Q_INVOKABLE void cancel() override;
    Q_INVOKABLE void suggestCharacterName() override;
    /// Asks whether a non-empty name is available.
    Q_INVOKABLE void updateCharacterName(const QString& Name) override;
    /// Ignored while a request is pending.
    Q_INVOKABLE void createCharacter(const QString& CharacterName, bool CharacterSexMale, bool PlayTutorial, const QString& WorldName) override;

Q_SIGNALS:
    /// The service created `name`; `reply` carries the account's new
    /// session and character list for the host to apply.
    void characterCreated(const QString& name, const QJsonObject& reply);

protected:
    /// Extra createCharacter fields of a product dialog.
    virtual void addCreateCharacterFields(QJsonObject& request) const { Q_UNUSED(request); }

private:
    using ReplyHandler = std::function<void(QNetworkReply*)>;

    /// POSTs `request`; asyncOperationInProgress while any post is unanswered.
    void post(const QJsonObject& request, ReplyHandler onReply);
    void setPending(int pending);
    void setAccountHasNoCharacters(bool noCharacters);
    void requestAccountCreationStatus();
    void onAccountCreationStatus(QNetworkReply* reply);
    void onCharacterNameGenerated(QNetworkReply* reply);
    void onCharacterNameChecked(QNetworkReply* reply);
    void onCharacterCreated(QNetworkReply* reply);
    void showError(const QString& message);

    QUrl m_serviceUrl;
    QString m_sessionKey;
    QNetworkAccessManager* m_network;
    TWorldSelectionSortProxyModel* m_proxy;
    TMessageDialogController* m_error;
    /// The last generated name: createCharacter's NameIsGenerated.
    QString m_generatedName;
    bool m_noCharacters = true;
    int m_pending = 0;
};

}
}
