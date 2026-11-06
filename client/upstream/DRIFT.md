# CIP drift

Absorbed for qtclient: tip QML lives under `client/upstream/qml/` with **no local QML
forks** of the files below. product pack pack-only skins (StoreDialog / podium) stay
in `the product pack`. Next `make tibia-sync` refreshes this tree; qtclient needs
no rebase for these paths.

## Last tip delta (`15.33` bb38aac95812 → `15.33` 83cf58bd3701)

**Added (2)** — already in upstream

- `qmlcomponents/qml/qmldir`
- `qmlcomponents/qmldir`

**Changed (4)** — qtclient uses upstream tip as-is

- `qmlcomponents/qml/NpcTrade.qml`
- `qmlcomponents/qml/StoreOfferPage.qml`
- `qmlcomponents/qml/outfitdialog/OutfitDialog.qml`
- `qmlcomponents/qml/outfitdialog/outfitdialog.js`
