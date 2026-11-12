#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtQml/qqml.h>

class WebChannelAttached : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString id READ id WRITE setId NOTIFY idChanged)

public:
    using QObject::QObject;

    QString id() const { return m_id; }
    void setId(const QString& id)
    {
        if (id == m_id)
            return;
        m_id = id;
        Q_EMIT idChanged();
    }

Q_SIGNALS:
    void idChanged();

private:
    QString m_id;
};

// Inert QtWebChannel.WebChannel: there is no web view to bridge to.
class WebChannel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_ATTACHED(WebChannelAttached)
    Q_PROPERTY(QVariantList registeredObjects READ registeredObjects WRITE setRegisteredObjects NOTIFY registeredObjectsChanged)

public:
    using QObject::QObject;

    static WebChannelAttached* qmlAttachedProperties(QObject* object) { return new WebChannelAttached(object); }

    QVariantList registeredObjects() const { return m_objects; }
    void setRegisteredObjects(const QVariantList& objects)
    {
        m_objects = objects;
        Q_EMIT registeredObjectsChanged();
    }

    Q_INVOKABLE void registerObject(const QString&, QObject*) {}
    Q_INVOKABLE void deregisterObject(QObject*) {}

Q_SIGNALS:
    void registeredObjectsChanged();

private:
    QVariantList m_objects;
};
