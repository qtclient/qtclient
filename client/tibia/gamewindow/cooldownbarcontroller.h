#pragma once

#include "game/cooldownstorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>
#include <QVariantList>

class QQuickItem;

namespace tibia::gamewindow {

/// One cooldown icon for CooldownBar.qml (CIP: TCooldownProgressDataForQML).
class TCooldownProgressDataForQML final : public cip::tibia::gamewindow::TCooldownProgressDataForQML
{
    Q_OBJECT
public:
    TCooldownProgressDataForQML(QString sourceUrl, int remainingMs, int totalMs, QString tooltip, QObject* parent = nullptr);
};

/// CooldownBar under the map (CIP: TCooldownBarController, ctor 0x100c0bae0 /
/// refresh 0x100c0c0f0). Option 0x77 cooldownBarEnabled (options-page
/// showCooldownBar) toggles the QML item; spell/group rows come from
/// TCooldownStorage.
class TCooldownBarController final : public cip::tibia::gamewindow::TCooldownBarController
{
    Q_OBJECT
public:
    explicit TCooldownBarController(QQuickItem* gameWindowItem, QObject* parent = nullptr);
    ~TCooldownBarController() override;

private:
    void applyVisibility();
    void rebuildIdleGroups();
    void refresh();
    static QString groupTooltip(int cipGroup);
    static QString groupIconUrl(int cipGroup);

    QPointer<QQuickItem> m_bar;
    QTimer m_tick;
    QVariantList m_groupModels;
    QVariantList m_spellModels;
};

}
