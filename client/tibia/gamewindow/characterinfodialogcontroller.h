#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QList>
#include <QSet>

#include <map>
#include <set>
#include <vector>

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// The Cyclopedia's character page (CIP: TCharacterInfoDialogController,
/// metacall , vtable). Always the own character: the
/// pages Crystal answers for (base information, general / offence / defence /
/// misc stats, badges, recent deaths and kills, store summary, inspection,
/// item summary, appearances, titles, achievements) are filled.
class TCharacterInfoDialogController final : public cip::tibia::gamewindow::TCharacterInfoDialogController
{
    Q_OBJECT
    Q_PROPERTY(int inspectionState READ inspectionState NOTIFY inspectionStateChanged)
public:
    /// CIP's character info data types (TCharacterInfoStorage keys).
    enum EDataType {
        NoData = 0,
        BaseInformation = 0,
        GeneralStats = 1,
        OffenceStats = 2,
        DefenceStats = 3,
        MiscStats = 4,
        RecentDeaths = 5,
        RecentKills = 6,
        Achievements = 7,
        ItemSummary = 8,
        OutfitsMounts = 9,
        StoreSummary = 10,
        Inspection = 11,
        Badges = 12,
        Titles = 13,
    };

    explicit TCharacterInfoDialogController(QObject* parent = nullptr);
    bool clientSettingSmoothFiltering() const override;
    ~TCharacterInfoDialogController() override;

    int dataType() const { return m_dataType; }
    int inspectionState() const { return m_inspectionState; }

    /// CIP .
    EDialogCategory dialogCategory() const override;
    /// CIP .
    bool dataIsLoading() const override;

    /// The page the next showDialogTab opens (CIP: the open action's sub-page,
    /// or the backlog context restored by); general stats after it.
    void setNextDataType(int dataType) { m_nextDataType = dataType; }
    /// CIP (+0x118): whose data is asked for, 0 for the own character.
    void setCharacterId(uint32_t characterId) { m_characterId = characterId; }
    void showDialogTab() override;
    /// Opens on `dataType`, fetching every record anew.
    void showDialogTab(int dataType);
    /// CIP .
    void hideDialogTab() override;

    /// CIP .
    Q_INVOKABLE void setDialogCategory(EDialogCategory Category) override;
    /// CIP .
    Q_INVOKABLE void openStoreWithXpBoostCategory() override;
    /// CIP .
    Q_INVOKABLE void openBlessingsDialog() override;
    /// CIP .
    Q_INVOKABLE void openItemInformation(int TypeID) override;
    /// CIP.
    Q_INVOKABLE void previousPaginatedPage() override;
    Q_INVOKABLE void nextPaginatedPage() override;
    /// The shown character's skill wheel.
    Q_INVOKABLE void openSkillWheelDialog() override;
    Q_INVOKABLE void openWeaponProficiencyDialog(uint AppearanceTypeID) override;
    /// CIP CharacterInfoShowSelection outfit/mount/summon preview.
    Q_INVOKABLE void openOutfitDialog(int OutfitID, bool IsOutfit, bool IsMount, bool IsSummonSkin) override;
    /// characterinfomodels.cpp.
    Q_INVOKABLE void resetCharacterTitle() override;
    Q_INVOKABLE void showContextMenuForCharacterTitleID(uint CharacerTitleId) override;
    /// product: Cmd/Ctrl+click Set/Reset (same actions as the title context menu).
    Q_INVOKABLE void setOrToggleCharacterTitle(uint characterTitleId);

private:
    void hookEvents();
    /// CIP : shows `dataType`, asking the server unless it is
    /// already there.
    void request(int dataType, bool force);
    void sendRequest(int dataType, int page);
    /// Rebuilds the page of `dataType` from its record when it is the
    /// current page (its old models are released either way).
    void onCharacterInfoUpdated(int dataType);
    void clearModels(const std::set<int>& dataTypes);
    void clearAllModels();
    void updateAccountInfos();
    void updateInspection();
    void updateAchievements();
    /// The pages filled from the character info storage (CIP: one per data type).
    void updateBaseInfo();
    void updateOffenceStats();
    void updateDefenceStats();
    void updateMiscStats();
    void updateDeaths();
    void updateKills();
    void updateStorePurchases();
    void updateItemSummary();
    void updateOutfitsMounts();
    void updateTitles();
    void onErrorReceived(int dataType, int errorCode);
    void rebuildStatsBlocks();

    template <class T>
    void replace(T*& member, T* object);
    void replace(QList<QObject*>& member, QList<QObject*> objects);

    std::vector<client::gamecore::Hook> m_hooks;
    TMessageDialogController* m_errorMessage;
    class TSelectOutfitDialogController* m_outfitDialog = nullptr;
    bool m_shown = false;
    int m_dataType = NoData;
    int m_nextDataType = GeneralStats;
    uint32_t m_characterId = 0;
    int m_inspectionState = 0;

Q_SIGNALS:
    void inspectionStateChanged();
};

}
}
