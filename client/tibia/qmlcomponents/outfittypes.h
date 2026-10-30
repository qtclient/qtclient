#pragma once

#include <cip_gen.h>

namespace tibia::qmlcomponents {

/// The 133 outfit colours (19 hues x 7 saturation/value steps; CIP 0x10094df40).
namespace outfitpalette {
constexpr int HueSteps = 19;
constexpr int Count = HueSteps * 7;
QColor color(int index);
/// The palette index of `color`, 0 when it is not a palette colour.
int index(const QColor& color);
}

/// Four palette indices (QML OutfitColor; CIP: TOutfitColorQmlType, metacall 0x1010858e0).
/// The colours are derived from the indices.
class TOutfitColorQmlType : public cip::tibia::qmlcomponents::TOutfitColorQmlType
{
    Q_OBJECT
public:
    explicit TOutfitColorQmlType(QObject* parent = nullptr);
    TOutfitColorQmlType(int head, int torso, int legs, int detail, QObject* parent = nullptr);

    void setHeadColor(const QColor& v) override;
    void setTorsoColor(const QColor& v) override;
    void setLegsColor(const QColor& v) override;
    void setDetailColor(const QColor& v) override;
    void setHeadColorIndex(int v) override;
    void setTorsoColorIndex(int v) override;
    void setLegsColorIndex(int v) override;
    void setDetailColorIndex(int v) override;

    void copyFrom(cip::tibia::qmlcomponents::TOutfitColorQmlType* pOther) override;
    QJsonObject toJsonObject() override;
    void fromJsonObject(const QJsonObject& JsonObject) override;
    bool canBeFilledFromJsonObject(const QJsonObject& JsonObject) override;
    void randomizeColors() override;

private:
    bool setIndex(int& index, QColor& color, int v);
};

/// Outfit, mount and summon with their colours and addons (QML OutfitPreset;
/// CIP: TOutfitPresetQmlType, metacall 0x1010871a0).
class TOutfitPresetQmlType : public cip::tibia::qmlcomponents::TOutfitPresetQmlType
{
    Q_OBJECT
public:
    explicit TOutfitPresetQmlType(QObject* parent = nullptr);

    void setName(const QString& v) override;
    void setOutfitID(int v) override;
    void setOutfitFirstAddOn(bool v) override;
    void setOutfitSecondAddOn(bool v) override;
    void setMountID(int v) override;
    void setSummonID(int v) override;
    void setIsObjectOutfit(bool v) override;

    QJsonObject toJsonObject() override;
    void fromJsonObject(const QJsonObject& JsonObject) override;
    void copyFrom(cip::tibia::qmlcomponents::TOutfitPresetQmlType* pOther) override;
    bool canBeFilledFromJsonObject(const QJsonObject& JsonObject) override;
    bool isEqual(cip::tibia::qmlcomponents::TOutfitPresetQmlType* pOther) override;
};

/// QML singleton TibiaObjects (CIP: TTibiaObjectCreatorQmlService, metacall
/// 0x101087670): JS-owned value objects.
class TTibiaObjectCreatorQmlService : public cip::tibia::qmlcomponents::TTibiaObjectCreatorQmlService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TTibiaObjectCreatorQmlService::TTibiaObjectCreatorQmlService;

    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* worldMapSubfieldCoordinate(int x, int y, int z) override;
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* worldMapSubfieldCoordinate(int x, int y) override { return worldMapSubfieldCoordinate(x, y, 0); }
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* worldMapSubfieldCoordinate(int x) override { return worldMapSubfieldCoordinate(x, 0, 0); }
    cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType* worldMapSubfieldCoordinate() override { return worldMapSubfieldCoordinate(0, 0, 0); }
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* worldMapCoordinate(int x, int y, int z) override;
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* worldMapCoordinate(int x, int y) override { return worldMapCoordinate(x, y, 0); }
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* worldMapCoordinate(int x) override { return worldMapCoordinate(x, 0, 0); }
    cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType* worldMapCoordinate() override { return worldMapCoordinate(0, 0, 0); }
    cip::tibia::qmlcomponents::TOutfitColorQmlType* createOutfitColor(int HeadIndex, int TorsoIndex, int LegsIndex, int DetailIndex) override;
    cip::tibia::qmlcomponents::TOutfitColorQmlType* createOutfitColor(int HeadIndex, int TorsoIndex, int LegsIndex) override { return createOutfitColor(HeadIndex, TorsoIndex, LegsIndex, 0); }
    cip::tibia::qmlcomponents::TOutfitColorQmlType* createOutfitColor(int HeadIndex, int TorsoIndex) override { return createOutfitColor(HeadIndex, TorsoIndex, 0, 0); }
    cip::tibia::qmlcomponents::TOutfitColorQmlType* createOutfitColor(int HeadIndex) override { return createOutfitColor(HeadIndex, 0, 0, 0); }
    cip::tibia::qmlcomponents::TOutfitColorQmlType* createOutfitColor() override { return createOutfitColor(0, 0, 0, 0); }
    cip::tibia::qmlcomponents::TOutfitPresetQmlType* outfitPreset() override;
};

/// QML singleton ClipboardHelper (CIP: TClipboardHelperQmlService, metacall
/// 0x101081de0): outfit presets travel as unpadded base64 of their CBOR.
class TClipboardHelperQmlService : public cip::tibia::qmlcomponents::TClipboardHelperQmlService
{
    Q_OBJECT
public:
    explicit TClipboardHelperQmlService(QObject* parent = nullptr);

    void setTextInClipboard(const QString& Text) override;
    QString getTextFromClipboard() override;
    QJsonObject getBase64StringAsJsonObject() override;
    void setJsonObjectAsBase64String(const QJsonObject& JsonObject) override;
    QJsonObject convertBase64StringAsJsonObject(const QString& Base64String) override;
    QString convertJsonObjectToBase64String(const QJsonObject& JsonObject) override;
};

/// Row access by role name for QML (CIP: THelperProxyModel, metacall 0x101080880).
class THelperProxyModel : public cip::tibia::qmlcomponents::THelperProxyModel
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::THelperProxyModel::THelperProxyModel;

    QVariant sourceItemData(const QModelIndex& pSourceModelIndex) override;
    QVariant sourceDataByName(const QModelIndex& pSourceModelIndex, const QString& RoleName) override;
    QVariant sourceItemDataByRowIndex(int RowIndex) override;
};

/// QML singleton AbstractItemModelHelper (CIP: TAbstractItemModelHelper, metacall
/// 0x101080a70).
class TAbstractItemModelHelper : public cip::tibia::qmlcomponents::TAbstractItemModelHelper
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TAbstractItemModelHelper::TAbstractItemModelHelper;

    QObject* wrapInHelperProxyModel(QAbstractItemModel* pAbstractItemModel) override;
    QObject* createSortProxyModel(QAbstractItemModel* pAbstractListModel) override;
    QString loadStringFromResource(const QString& ResourcePath) override;
    QJSValue loadJsonFromResource(const QString& ResourcePath) override;
};

}
