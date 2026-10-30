#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace tibia::gamewindow {

class TMessageDialogController;

/// One entry of the account service's "Worlds" (CIP 0x100e13260).
struct CreationWorld
{
    QString name;
    QString region;
    QString pvpType;
    int playersOnline = 0;
    QString creationDate;
    QString battleEyeStatus;
    bool premiumOnly = false;
    QString transferType;
};

/// The worlds of WorldSelection.qml (CIP: WorldSelectionModel); one role,
/// "WorldName".
class TWorldSelectionModel final : public cip::tibia::gamewindow::WorldSelectionModel
{
    Q_OBJECT
public:
    enum Role { WorldNameRole = Qt::UserRole + 1 };

    explicit TWorldSelectionModel(QObject* parent = nullptr);

    void setWorlds(const QList<CreationWorld>& worlds);
    const QList<CreationWorld>& worlds() const { return m_worlds; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<CreationWorld> m_worlds;
};

/// One option of WorldSelection.qml's region and PvP type combo boxes.
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

/// CreateCharacterDialog.qml's worldsModel (CIP: TWorldSelectionSortProxyModel):
/// the worlds by name, filtered by region and PvP type.
class TWorldSelectionSortProxyModel final : public cip::tibia::gamewindow::TWorldSelectionSortProxyModel
{
    Q_OBJECT
public:
    explicit TWorldSelectionSortProxyModel(TWorldSelectionModel* source, QObject* parent = nullptr);

    /// The worlds changed: rebuilds the filter options and clears the filters.
    void reload(const QString& recommendedWorld);

    void setWorldRegionFilter(const QString& v) override;
    void setWorldPvPTypeFilter(const QString& v) override;

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

private:
    const CreationWorld* worldAt(const QModelIndex& index) const;
    QList<QObject*> filterOptions(const QStringList& values);

    TWorldSelectionModel* m_source;
};

/// CreateCharacterDialog.qml's controller (CIP: TCreateCharacterDialogController,
/// slots 0x100815990): character name checks, name suggestions and the
/// character creation, posted as JSON to the account service.
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

    /// SHOW_CREATE_CHARACTER_DIALOG (CIP 0x100b73500 publishes it from the
    /// character list; the parameter: the account has no characters, so the
    /// dialog cannot be cancelled). Ignored while a request is pending.
    void open(bool accountHasNoCharacters);

    /// CIP 0x100e158f0: shows the dialog and asks for the worlds (0x100e15910).
    void showDialog() override;
    /// CIP 0x100e15830: resets the form and closes the dialog.
    Q_INVOKABLE void cancel() override;
    /// CIP 0x100e17160: GenerateCharacterName.
    Q_INVOKABLE void suggestCharacterName() override;
    /// CIP 0x100e18200: CheckCharacterName for a non-empty name (0x100e173e0).
    Q_INVOKABLE void updateCharacterName(const QString& Name) override;
    /// CIP 0x100e182e0: createCharacter.
    Q_INVOKABLE void createCharacter(const QString& CharacterName, bool CharacterSexMale, bool PlayTutorial, const QString& WorldName) override;

Q_SIGNALS:
    /// The service created `name`; `reply` carries the account's new
    /// session and character list (CIP 0x100980e50 applies it).
    void characterCreated(const QString& name, const QJsonObject& reply);

protected:
    /// Extra createCharacter fields of a product dialog.
    virtual void addCreateCharacterFields(QJsonObject& request) const { Q_UNUSED(request); }
    /// Back to the defaults (CIP 0x100e15830 before closing).
    virtual void resetForm();

private:
    using ReplyHandler = std::function<void(const QJsonObject&)>;

    /// CIP 0x100e15b90: POSTs `request`; asyncOperationInProgress while any is pending.
    void post(const QJsonObject& request, ReplyHandler onReply);
    void setPending(int pending);
    /// CIP 0x100e16a60: the error message box.
    void showError(const QString& message);
    void onStatus(const QJsonObject& reply);

    QUrl m_serviceUrl;
    QString m_sessionKey;
    QNetworkAccessManager* m_network;
    TWorldSelectionModel* m_worlds;
    TWorldSelectionSortProxyModel* m_proxy;
    TMessageDialogController* m_error;
    /// The last GenerateCharacterName answer (CIP +0x108): NameIsGenerated.
    QString m_generatedName;
    bool m_noCharacters = false;
    bool m_creating = false;
    int m_pending = 0;
};

}
