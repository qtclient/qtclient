#include "sidebaroptions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <array>
#include <cmath>

namespace tibia::config {

namespace {

const QString PanelsKey = QStringLiteral("sidebarPanelsOptions");
const QString ZoomKey = QStringLiteral("minimapZoomLevel");
const QString ControlButtonsKey = QStringLiteral("controlButtonsVisible");
const QString MinimizeInventoryKey = QStringLiteral("minimizeInventory");
const QString ChaseModeKey = QStringLiteral("chaseModeEnabled");
const QString PanelsManagerKey = QStringLiteral("sidebarPanelsMangerOptions");
const QString PanelsOrderKey = QStringLiteral("panelsOrder");
const QString WidgetsManagerKey = QStringLiteral("sidebarWidgetsMangerOptions");
const QString LeftSidebarCountKey = QStringLiteral("leftSidebarCount");
const QString OpenWidgetsKey = QStringLiteral("openWidgetsOrderPerSidebar");
const QString ContainersKey = QStringLiteral("containersOptions");
const QString ContentHeightKey = QStringLiteral("contentHeight");
const QString ContentMaximizedKey = QStringLiteral("contentMaximized");

const std::array<QLatin1StringView, 4> PanelNames = {
    QLatin1StringView("minimap"),
    QLatin1StringView("inventoryStatus"),
    QLatin1StringView("hitpointManabar"),
    QLatin1StringView("buttons"),
};

QJsonObject serialize(const SidebarPanelsOptions& o)
{
    QJsonObject json;
    json.insert(ZoomKey, static_cast<int>(std::round(o.minimapZoomLevel * 100.0)));
    json.insert(ControlButtonsKey, o.controlButtonsVisible);
    json.insert(MinimizeInventoryKey, o.minimizeInventory);
    json.insert(ChaseModeKey, o.chaseModeEnabled);
    return json;
}

SidebarPanelsOptions parse(const QJsonObject& json)
{
    SidebarPanelsOptions o;
    o.minimapZoomLevel = std::clamp(json.value(ZoomKey).toInt(0), -100, 200) / 100.0f;
    o.controlButtonsVisible = json.value(ControlButtonsKey).toBool(false);
    o.minimizeInventory = json.value(MinimizeInventoryKey).toBool(false);
    o.chaseModeEnabled = json.value(ChaseModeKey).toBool(true);
    return o;
}

QList<SidebarPanelType> parsePanelsOrder(const QJsonObject& json)
{
    QList<SidebarPanelType> order;
    for (const QJsonValue& name : json.value(PanelsOrderKey).toArray()) {
        const auto it = std::find(PanelNames.begin(), PanelNames.end(), name.toString());
        if (it != PanelNames.end())
            order.append(static_cast<SidebarPanelType>(it - PanelNames.begin()));
    }
    return order;
}

QJsonObject serializePanelsOrder(const QList<SidebarPanelType>& order)
{
    QJsonArray names;
    for (const SidebarPanelType type : order)
        names.append(QString(PanelNames[static_cast<size_t>(type)]));
    return { { PanelsOrderKey, names } };
}

SidebarWidgetsManagerOptions parseWidgetsManager(const QJsonObject& json)
{
    SidebarWidgetsManagerOptions o;
    o.openWidgetsOrderPerSidebar = json.value(OpenWidgetsKey).toArray();
    o.leftSidebarCount = std::clamp(json.value(LeftSidebarCountKey).toInt(0), 0, static_cast<int>(o.openWidgetsOrderPerSidebar.size()));
    return o;
}

QJsonObject serializeWidgetsManager(const SidebarWidgetsManagerOptions& o)
{
    return { { LeftSidebarCountKey, o.leftSidebarCount }, { OpenWidgetsKey, o.openWidgetsOrderPerSidebar } };
}

ContainersOptions parseContainers(const QJsonObject& json)
{
    ContainersOptions containers;
    for (auto it = json.begin(); it != json.end(); ++it) {
        bool numeric = false;
        const int id = it.key().toInt(&numeric);
        if (!numeric)
            continue;
        const QJsonObject entry = it.value().toObject();
        containers[id] = { entry.value(ContentHeightKey).toInt(0), entry.value(ContentMaximizedKey).toBool(true) };
    }
    return containers;
}

QJsonObject serializeContainers(const ContainersOptions& containers)
{
    QJsonObject json;
    for (const auto& [id, o] : containers)
        json.insert(QString::number(id), QJsonObject{ { ContentHeightKey, o.contentHeight }, { ContentMaximizedKey, o.contentMaximized } });
    return json;
}

}

TSidebarOptions::TSidebarOptions(QObject* parent)
    : cip::tibia::config::TSidebarOptions(parent)
{}

QString TSidebarOptions::filePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/characterdata/%1/sidebars.json").arg(m_characterId);
}

void TSidebarOptions::loadForCharacter(const quint32 characterId)
{
    if (characterId == m_characterId)
        return;
    save();
    m_characterId = characterId;
    m_root = {};
    if (characterId != 0) {
        QFile file(filePath());
        if (file.open(QIODevice::ReadOnly))
            m_root = QJsonDocument::fromJson(file.readAll()).object();
    }
    assign(parse(m_root.value(PanelsKey).toObject()));
    m_widgetsManager = parseWidgetsManager(m_root.value(WidgetsManagerKey).toObject());
    Q_EMIT sidebarWidgetsManagerOptionsChanged();
    m_panelsOrder = parsePanelsOrder(m_root.value(PanelsManagerKey).toObject());
    Q_EMIT sidebarPanelsManagerOptionsChanged();
    m_containers = parseContainers(m_root.value(ContainersKey).toObject());
    Q_EMIT optionsForAllContainersChanged();
    m_dirty = false;
}

void TSidebarOptions::save()
{
    if (!m_dirty || m_characterId == 0)
        return;
    m_root.insert(PanelsKey, serialize(m_panels));
    m_root.insert(PanelsManagerKey, serializePanelsOrder(m_panelsOrder));
    m_root.insert(WidgetsManagerKey, serializeWidgetsManager(m_widgetsManager));
    m_root.insert(ContainersKey, serializeContainers(m_containers));
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(m_root).toJson());
        if (file.commit())
            m_dirty = false;
    }
}

void TSidebarOptions::setSidebarPanelsOptions(const SidebarPanelsOptions& options)
{
    if (options == m_panels)
        return;
    assign(options);
    changed();
}

void TSidebarOptions::setPanelsOrder(const QList<SidebarPanelType>& order)
{
    if (order == m_panelsOrder)
        return;
    m_panelsOrder = order;
    changed();
}

void TSidebarOptions::setSidebarWidgetsManagerOptions(const SidebarWidgetsManagerOptions& options)
{
    if (options == m_widgetsManager)
        return;
    m_widgetsManager = options;
    changed();
}

void TSidebarOptions::setContainersOptions(const ContainersOptions& options)
{
    if (options == m_containers)
        return;
    m_containers = options;
    changed();
}

void TSidebarOptions::setSection(const QString& key, const QJsonValue& value)
{
    if (m_root.value(key) == value)
        return;
    m_root.insert(key, value);
    changed();
}

void TSidebarOptions::assign(const SidebarPanelsOptions& options)
{
    m_panels = options;
    Q_EMIT sidebarPanelsOptionsChanged();
}

void TSidebarOptions::changed()
{
    m_dirty = true;
    Q_EMIT configurationChangedFromUserInput();
}

}
