#include "outfittypes.h"

#include "qmlcomponents/worldmapqmltypes.h"

#include <QCborMap>
#include <QCborValue>
#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QJSEngine>
#include <QJsonDocument>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QSortFilterProxyModel>

namespace tibia::qmlcomponents {

namespace {

const QString KeyHead = QStringLiteral("head");
const QString KeyTorso = QStringLiteral("torso");
const QString KeyLegs = QStringLiteral("legs");
const QString KeyDetail = QStringLiteral("detail");
const QString KeyName = QStringLiteral("name");
const QString KeyId = QStringLiteral("id");
const QString KeyFirstAddOn = QStringLiteral("firstAddOn");
const QString KeySecondAddOn = QStringLiteral("secondAddOn");
const QString KeyColor = QStringLiteral("color");
const QString KeyOutfit = QStringLiteral("outfit");
const QString KeyMount = QStringLiteral("mount");
const QString KeySummon = QStringLiteral("summon");

int clampIndex(const int v)
{
    return v >= 0 && v < outfitpalette::Count ? v : 0;
}

}

namespace outfitpalette {

/// Hue in 18 steps (step 0 is grey), then 7 saturation/value pairs per row.
QColor color(int index)
{
    if (index < 0 || index >= Count)
        index = 0;
    float hue = 0.f;
    float saturation = 0.f;
    float value = 0.f;
    if (index % HueSteps != 0) {
        hue = static_cast<float>(index % HueSteps) / 18.f;
        static constexpr float rows[7][2] = { { 0.25f, 1.f }, { 0.25f, 0.75f }, { 0.5f, 0.75f }, { 0.667f, 0.75f },
                                               { 1.f, 1.f }, { 1.f, 0.75f }, { 1.f, 0.5f } };
        saturation = rows[index / HueSteps][0];
        value = rows[index / HueSteps][1];
    } else {
        value = 1.f - static_cast<float>(index) / HueSteps / 7.f;
    }
    if (value == 0.f)
        return QColor(0, 0, 0, 0);
    if (saturation == 0.f) {
        const int grey = static_cast<int>(value * 255);
        return QColor(grey, grey, grey);
    }
    float red = 0.f;
    float green = 0.f;
    float blue = 0.f;
    if (hue < 1.f / 6.f) {
        red = value;
        blue = value * (1 - saturation);
        green = blue + (value - blue) * 6 * hue;
    } else if (hue < 2.f / 6.f) {
        green = value;
        blue = value * (1 - saturation);
        red = green - (value - blue) * (6 * hue - 1);
    } else if (hue < 3.f / 6.f) {
        green = value;
        red = value * (1 - saturation);
        blue = red + (value - red) * (6 * hue - 2);
    } else if (hue < 4.f / 6.f) {
        blue = value;
        red = value * (1 - saturation);
        green = blue - (value - red) * (6 * hue - 3);
    } else if (hue < 5.f / 6.f) {
        blue = value;
        green = value * (1 - saturation);
        red = green + (value - green) * (6 * hue - 4);
    } else {
        red = value;
        green = value * (1 - saturation);
        blue = red - (value - green) * (6 * hue - 5);
    }
    return QColor(static_cast<int>(red * 255), static_cast<int>(green * 255), static_cast<int>(blue * 255));
}

int index(const QColor& c)
{
    for (int i = 0; i < Count; ++i) {
        if (color(i).rgba() == c.rgba())
            return i;
    }
    return 0;
}

}

TOutfitColorQmlType::TOutfitColorQmlType(QObject* parent)
    : TOutfitColorQmlType(0, 0, 0, 0, parent)
{}

TOutfitColorQmlType::TOutfitColorQmlType(const int head, const int torso, const int legs, const int detail, QObject* parent)
    : cip::tibia::qmlcomponents::TOutfitColorQmlType(parent)
{
    m_headColorIndex = clampIndex(head);
    m_torsoColorIndex = clampIndex(torso);
    m_legsColorIndex = clampIndex(legs);
    m_detailColorIndex = clampIndex(detail);
    m_headColor = outfitpalette::color(m_headColorIndex);
    m_torsoColor = outfitpalette::color(m_torsoColorIndex);
    m_legsColor = outfitpalette::color(m_legsColorIndex);
    m_detailColor = outfitpalette::color(m_detailColorIndex);
}

bool TOutfitColorQmlType::setIndex(int& index, QColor& color, const int v)
{
    if (index == v)
        return false;
    index = v;
    color = outfitpalette::color(v);
    return true;
}

void TOutfitColorQmlType::setHeadColor(const QColor& v) { setHeadColorIndex(outfitpalette::index(v)); }
void TOutfitColorQmlType::setTorsoColor(const QColor& v) { setTorsoColorIndex(outfitpalette::index(v)); }
void TOutfitColorQmlType::setLegsColor(const QColor& v) { setLegsColorIndex(outfitpalette::index(v)); }
void TOutfitColorQmlType::setDetailColor(const QColor& v) { setDetailColorIndex(outfitpalette::index(v)); }

/// CIP 0x1010b05e0..0x1010b06d0: emits only on a change.
void TOutfitColorQmlType::setHeadColorIndex(const int v)
{
    if (setIndex(m_headColorIndex, m_headColor, v))
        Q_EMIT colorChanged();
}

void TOutfitColorQmlType::setTorsoColorIndex(const int v)
{
    if (setIndex(m_torsoColorIndex, m_torsoColor, v))
        Q_EMIT colorChanged();
}

void TOutfitColorQmlType::setLegsColorIndex(const int v)
{
    if (setIndex(m_legsColorIndex, m_legsColor, v))
        Q_EMIT colorChanged();
}

void TOutfitColorQmlType::setDetailColorIndex(const int v)
{
    if (setIndex(m_detailColorIndex, m_detailColor, v))
        Q_EMIT colorChanged();
}

/// CIP 0x1010b0720.
void TOutfitColorQmlType::copyFrom(cip::tibia::qmlcomponents::TOutfitColorQmlType* pOther)
{
    if (!pOther)
        return;
    m_headColorIndex = pOther->headColorIndex();
    m_torsoColorIndex = pOther->torsoColorIndex();
    m_legsColorIndex = pOther->legsColorIndex();
    m_detailColorIndex = pOther->detailColorIndex();
    m_headColor = outfitpalette::color(m_headColorIndex);
    m_torsoColor = outfitpalette::color(m_torsoColorIndex);
    m_legsColor = outfitpalette::color(m_legsColorIndex);
    m_detailColor = outfitpalette::color(m_detailColorIndex);
    Q_EMIT colorChanged();
}

/// CIP 0x1010b0750.
QJsonObject TOutfitColorQmlType::toJsonObject()
{
    QJsonObject json;
    json[KeyHead] = m_headColorIndex;
    json[KeyTorso] = m_torsoColorIndex;
    json[KeyDetail] = m_detailColorIndex;
    json[KeyLegs] = m_legsColorIndex;
    return json;
}

/// CIP 0x1010b08c0.
void TOutfitColorQmlType::fromJsonObject(const QJsonObject& JsonObject)
{
    if (!canBeFilledFromJsonObject(JsonObject))
        return;
    setHeadColorIndex(JsonObject[KeyHead].toInt());
    setTorsoColorIndex(JsonObject[KeyTorso].toInt());
    setDetailColorIndex(JsonObject[KeyDetail].toInt());
    setLegsColorIndex(JsonObject[KeyLegs].toInt());
}

/// CIP 0x1010b0b90.
bool TOutfitColorQmlType::canBeFilledFromJsonObject(const QJsonObject& JsonObject)
{
    for (const QString& key : { KeyHead, KeyTorso, KeyLegs, KeyDetail }) {
        if (!JsonObject[key].isDouble())
            return false;
    }
    return true;
}

/// CIP 0x1010b0d50.
void TOutfitColorQmlType::randomizeColors()
{
    auto* random = QRandomGenerator::global();
    m_headColorIndex = static_cast<int>(random->bounded(outfitpalette::Count));
    m_torsoColorIndex = static_cast<int>(random->bounded(outfitpalette::Count));
    m_legsColorIndex = static_cast<int>(random->bounded(outfitpalette::Count));
    m_detailColorIndex = static_cast<int>(random->bounded(outfitpalette::Count));
    m_headColor = outfitpalette::color(m_headColorIndex);
    m_torsoColor = outfitpalette::color(m_torsoColorIndex);
    m_legsColor = outfitpalette::color(m_legsColorIndex);
    m_detailColor = outfitpalette::color(m_detailColorIndex);
    Q_EMIT colorChanged();
}

TOutfitPresetQmlType::TOutfitPresetQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TOutfitPresetQmlType(parent)
{
    m_outfitColor = new TOutfitColorQmlType(this);
    m_mountColor = new TOutfitColorQmlType(this);
}

void TOutfitPresetQmlType::setName(const QString& v)
{
    if (v != m_name)
        cip::tibia::qmlcomponents::TOutfitPresetQmlType::setName(v);
}

void TOutfitPresetQmlType::setOutfitID(const int v)
{
    if (v != m_outfitID)
        cip::tibia::qmlcomponents::TOutfitPresetQmlType::setOutfitID(v);
}

void TOutfitPresetQmlType::setOutfitFirstAddOn(const bool v)
{
    if (v != m_outfitFirstAddOn)
        cip::tibia::qmlcomponents::TOutfitPresetQmlType::setOutfitFirstAddOn(v);
}

void TOutfitPresetQmlType::setOutfitSecondAddOn(const bool v)
{
    if (v != m_outfitSecondAddOn)
        cip::tibia::qmlcomponents::TOutfitPresetQmlType::setOutfitSecondAddOn(v);
}

void TOutfitPresetQmlType::setMountID(const int v)
{
    if (v != m_mountID)
        cip::tibia::qmlcomponents::TOutfitPresetQmlType::setMountID(v);
}

void TOutfitPresetQmlType::setSummonID(const int v)
{
    if (v != m_summonID)
        cip::tibia::qmlcomponents::TOutfitPresetQmlType::setSummonID(v);
}

void TOutfitPresetQmlType::setIsObjectOutfit(const bool v)
{
    if (v != m_isObjectOutfit)
        cip::tibia::qmlcomponents::TOutfitPresetQmlType::setIsObjectOutfit(v);
}

/// CIP 0x1010b1480: {name, outfit {id, firstAddOn, secondAddOn, color}, mount {id, color}, summon {id}}.
QJsonObject TOutfitPresetQmlType::toJsonObject()
{
    QJsonObject json;
    json[KeyName] = m_name;
    QJsonObject outfit;
    outfit[KeyId] = m_outfitID;
    outfit[KeyFirstAddOn] = m_outfitFirstAddOn;
    outfit[KeySecondAddOn] = m_outfitSecondAddOn;
    outfit.insert(KeyColor, m_outfitColor->toJsonObject());
    json.insert(KeyOutfit, outfit);
    QJsonObject mount;
    mount[KeyId] = m_mountID;
    mount.insert(KeyColor, m_mountColor->toJsonObject());
    json.insert(KeyMount, mount);
    QJsonObject summon;
    summon[KeyId] = m_summonID;
    json.insert(KeySummon, summon);
    return json;
}

/// CIP 0x1010b1f30: fills the fields without notifying (copyFrom notifies).
void TOutfitPresetQmlType::fromJsonObject(const QJsonObject& JsonObject)
{
    if (!canBeFilledFromJsonObject(JsonObject))
        return;
    m_name = JsonObject[KeyName].toString();
    const QJsonObject outfit = JsonObject[KeyOutfit].toObject();
    m_outfitID = outfit[KeyId].toInt();
    m_outfitFirstAddOn = outfit[KeyFirstAddOn].toBool();
    m_outfitSecondAddOn = outfit[KeySecondAddOn].toBool();
    m_outfitColor->fromJsonObject(outfit[KeyColor].toObject());
    const QJsonObject mount = JsonObject[KeyMount].toObject();
    m_mountID = mount[KeyId].toInt();
    m_mountColor->fromJsonObject(mount[KeyColor].toObject());
    m_summonID = JsonObject[KeySummon].toObject()[KeyId].toInt();
}

/// CIP 0x1010b2240: through the JSON form.
void TOutfitPresetQmlType::copyFrom(cip::tibia::qmlcomponents::TOutfitPresetQmlType* pOther)
{
    if (!pOther)
        return;
    fromJsonObject(pOther->toJsonObject());
    m_isObjectOutfit = pOther->isObjectOutfit();
    Q_EMIT presetChanged();
}

/// CIP 0x1010b1840.
bool TOutfitPresetQmlType::canBeFilledFromJsonObject(const QJsonObject& JsonObject)
{
    if (!JsonObject[KeyName].isString())
        return false;
    const QJsonObject outfit = JsonObject[KeyOutfit].toObject();
    const QJsonObject mount = JsonObject[KeyMount].toObject();
    if (!outfit[KeyId].isDouble() || !outfit[KeyFirstAddOn].isBool() || !outfit[KeySecondAddOn].isBool())
        return false;
    if (!mount[KeyId].isDouble() || !JsonObject[KeySummon].toObject()[KeyId].isDouble())
        return false;
    return m_outfitColor->canBeFilledFromJsonObject(outfit[KeyColor].toObject())
        && m_mountColor->canBeFilledFromJsonObject(mount[KeyColor].toObject());
}

/// CIP 0x1010b1e50.
bool TOutfitPresetQmlType::isEqual(cip::tibia::qmlcomponents::TOutfitPresetQmlType* pOther)
{
    if (!pOther)
        return false;
    QJsonObject mine = toJsonObject();
    QJsonObject theirs = pOther->toJsonObject();
    mine.remove(KeyName);
    theirs.remove(KeyName);
    return mine == theirs;
}

cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* TTibiaObjectCreatorQmlService::worldMapSubfieldCoordinate(const int x, const int y, const int z)
{
    auto* coordinate = new TWorldMapSubfieldCoordinateQmlType;
    coordinate->setSubfieldX(x);
    coordinate->setSubfieldY(y);
    coordinate->setZ(z);
    QJSEngine::setObjectOwnership(coordinate, QJSEngine::JavaScriptOwnership);
    return coordinate;
}

cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* TTibiaObjectCreatorQmlService::worldMapCoordinate(const int x, const int y, const int z)
{
    auto* coordinate = new TWorldMapCoordinateQmlType;
    coordinate->setX(x);
    coordinate->setY(y);
    coordinate->setZ(z);
    QJSEngine::setObjectOwnership(coordinate, QJSEngine::JavaScriptOwnership);
    return coordinate;
}

/// CIP 0x1010b01c0.
cip::tibia::qmlcomponents::TOutfitColorQmlType* TTibiaObjectCreatorQmlService::createOutfitColor(const int HeadIndex, const int TorsoIndex, const int LegsIndex, const int DetailIndex)
{
    auto* color = new TOutfitColorQmlType(HeadIndex, TorsoIndex, LegsIndex, DetailIndex);
    QJSEngine::setObjectOwnership(color, QJSEngine::JavaScriptOwnership);
    return color;
}

/// CIP 0x1010b02a0.
cip::tibia::qmlcomponents::TOutfitPresetQmlType* TTibiaObjectCreatorQmlService::outfitPreset()
{
    auto* preset = new TOutfitPresetQmlType;
    QJSEngine::setObjectOwnership(preset, QJSEngine::JavaScriptOwnership);
    return preset;
}

TClipboardHelperQmlService::TClipboardHelperQmlService(QObject* parent)
    : cip::tibia::qmlcomponents::TClipboardHelperQmlService(parent)
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &TClipboardHelperQmlService::newClipboardTextAvailable);
}

void TClipboardHelperQmlService::setTextInClipboard(const QString& Text)
{
    QGuiApplication::clipboard()->setText(Text);
}

QString TClipboardHelperQmlService::getTextFromClipboard()
{
    return QGuiApplication::clipboard()->text();
}

QJsonObject TClipboardHelperQmlService::getBase64StringAsJsonObject()
{
    return convertBase64StringAsJsonObject(getTextFromClipboard());
}

void TClipboardHelperQmlService::setJsonObjectAsBase64String(const QJsonObject& JsonObject)
{
    setTextInClipboard(convertJsonObjectToBase64String(JsonObject));
}

/// CIP 0x1008f4f00: base64 (no padding) of CBOR.
QJsonObject TClipboardHelperQmlService::convertBase64StringAsJsonObject(const QString& Base64String)
{
    const QByteArray cbor = QByteArray::fromBase64(Base64String.toLocal8Bit(), QByteArray::OmitTrailingEquals);
    return QCborValue::fromCbor(cbor).toMap().toJsonObject();
}

/// CIP 0x1008f5170.
QString TClipboardHelperQmlService::convertJsonObjectToBase64String(const QJsonObject& JsonObject)
{
    return QString::fromLocal8Bit(QCborValue::fromJsonValue(JsonObject).toCbor().toBase64(QByteArray::OmitTrailingEquals));
}

/// CIP 0x1010c7630: every role of the source row, by role name.
QVariant THelperProxyModel::sourceItemData(const QModelIndex& pSourceModelIndex)
{
    QAbstractItemModel* source = sourceModel();
    if (!source || !pSourceModelIndex.isValid())
        return {};
    QVariantMap row;
    const auto roles = source->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        row.insert(QString::fromUtf8(it.value()), source->data(pSourceModelIndex, it.key()));
    return row;
}

/// CIP 0x1010c7b90.
QVariant THelperProxyModel::sourceDataByName(const QModelIndex& pSourceModelIndex, const QString& RoleName)
{
    return sourceItemData(pSourceModelIndex).toMap().value(RoleName);
}

/// CIP 0x1010c7e60.
QVariant THelperProxyModel::sourceItemDataByRowIndex(const int RowIndex)
{
    QAbstractItemModel* source = sourceModel();
    if (!source)
        return {};
    return sourceItemData(source->index(RowIndex, 0));
}

/// CIP 0x1010c4c00.
QObject* TAbstractItemModelHelper::wrapInHelperProxyModel(QAbstractItemModel* pAbstractItemModel)
{
    if (!pAbstractItemModel)
        return nullptr;
    auto* proxy = new THelperProxyModel;
    proxy->setSourceModel(pAbstractItemModel);
    QJSEngine::setObjectOwnership(proxy, QJSEngine::JavaScriptOwnership);
    return proxy;
}

/// CIP 0x1010c4d00.
QObject* TAbstractItemModelHelper::createSortProxyModel(QAbstractItemModel* pAbstractListModel)
{
    if (!pAbstractListModel)
        return nullptr;
    auto* proxy = new QSortFilterProxyModel;
    QJSEngine::setObjectOwnership(proxy, QJSEngine::JavaScriptOwnership);
    proxy->setSourceModel(pAbstractListModel);
    return proxy;
}

/// CIP 0x1010c5190.
QString TAbstractItemModelHelper::loadStringFromResource(const QString& ResourcePath)
{
    QFile file(ResourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << ResourcePath;
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

/// CIP 0x1010c4d80.
QJSValue TAbstractItemModelHelper::loadJsonFromResource(const QString& ResourcePath)
{
    QFile file(ResourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << ResourcePath;
        return QJSValue(QJSValue::UndefinedValue);
    }
    QJSEngine* engine = qjsEngine(this);
    if (!engine)
        return QJSValue(QJSValue::UndefinedValue);
    return engine->toScriptValue(QJsonDocument::fromJson(file.readAll()).toVariant());
}

}
