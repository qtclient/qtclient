#pragma once

#include "client/gamecore.h"

#include <QObject>

#include <vector>

namespace tibia::worldmap {
class TAmbientLightStorage;
}

namespace tibia::minimap {
class TMinimapTileStorage;
}

namespace tibia::mods {
class InstanceBackend;
}

namespace tibia::gamewindow {

class TBosstiaryButtonController;
class TConfigureCreaturePodiumDialogController;
class TCyclopediaButtonController;
class TCyclopediaDialogController;
class TEditTextController;
class THighscoresDialogController;
class TPreyDialogController;
class TQuestLogController;
class TSelectOutfitDialogController;
class TServerModalDialogController;

/// Owns the information/character dialogs (outfit, server modal dialogs,
/// text windows, quest log) and feeds them from the game's protocol events.
class TInformationDialogs final : public QObject
{
    Q_OBJECT
public:
    explicit TInformationDialogs(QObject* parent = nullptr);
    ~TInformationDialogs() override;

    TPreyDialogController* prey() const { return m_prey; }
    TQuestLogController* questLog() const { return m_questLog; }
    TCyclopediaButtonController* cyclopediaButton() const { return m_cyclopediaButton; }
    TBosstiaryButtonController* bosstiaryButton() const { return m_bosstiaryButton; }
    void setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight);
    /// The local minimap the Cyclopedia map's LocalMinimap mode draws.
    void setMinimapTileStorage(minimap::TMinimapTileStorage* tiles);
    void setInstanceStorage(mods::InstanceBackend* instance);
    void setControlScheme(int controlScheme);

private:
    void hookOutfitDialog();
    void hookCreaturePodium();
    void hookTextDialogs();
    void hookQuestLog();
    void hookHighscores();
    void hookPrey();

    std::vector<client::gamecore::Hook> m_hooks;
    TSelectOutfitDialogController* m_outfitDialog;
    TConfigureCreaturePodiumDialogController* m_podiumDialog;
    TServerModalDialogController* m_modalDialog;
    TEditTextController* m_editText;
    TQuestLogController* m_questLog;
    THighscoresDialogController* m_highscores;
    TPreyDialogController* m_prey;
    TCyclopediaDialogController* m_cyclopedia;
    TCyclopediaButtonController* m_cyclopediaButton;
    TBosstiaryButtonController* m_bosstiaryButton;
};

}
