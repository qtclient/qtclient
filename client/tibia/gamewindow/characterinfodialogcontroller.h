#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QList>
#include <QSet>

#include <vector>

namespace tibia::gamewindow {

class TMessageDialogController;

/// The Cyclopedia's character page (CIP: TCharacterInfoDialogController,
/// metacall 0x10080bbc0, vtable 0x10262e4d0). Always the own character: the
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
    ~TCharacterInfoDialogController() override;

    int dataType() const { return m_dataType; }
    int inspectionState() const { return m_inspectionState; }

    /// CIP 0x100d0ed50.
    EDialogCategory dialogCategory() const override;
    /// CIP 0x100d0ed90.
    bool dataIsLoading() const override;

    /// The page the next showDialogTab opens (CIP: the open action's sub-page,
    /// or the backlog context restored by 0x100d11760); general stats after it.
    void setNextDataType(int dataType) { m_nextDataType = dataType; }
    /// CIP 0x100d0f600 (+0x118): whose data is asked for, 0 for the own character.
    void setCharacterId(uint32_t characterId) { m_characterId = characterId; }
    /// CIP 0x100d10ef0.
    void showDialogTab() override;
    /// CIP 0x100d11540.
    void hideDialogTab() override;

    /// CIP 0x100d0f230.
    Q_INVOKABLE void setDialogCategory(EDialogCategory Category) override;
    /// CIP 0x100d0f610.
    Q_INVOKABLE void openStoreWithXpBoostCategory() override;
    /// CIP 0x100d0f870.
    Q_INVOKABLE void openBlessingsDialog() override;
    /// CIP 0x100d0f8f0.
    Q_INVOKABLE void openItemInformation(int TypeID) override;
    /// CIP 0x100d0fa20 / 0x100d0fbe0.
    Q_INVOKABLE void previousPaginatedPage() override;
    Q_INVOKABLE void nextPaginatedPage() override;
    /// CIP 0x100d10df0.
    Q_INVOKABLE void openWeaponProficiencyDialog(uint AppearanceTypeID) override;
    /// characterinfomodels.cpp.
    Q_INVOKABLE void resetCharacterTitle() override;
    Q_INVOKABLE void showContextMenuForCharacterTitleID(uint CharacerTitleId) override;
    /// product: Cmd/Ctrl+click Set/Reset (same actions as the title context menu).
    Q_INVOKABLE void setOrToggleCharacterTitle(uint characterTitleId);

private:
    using Args = std::vector<std::any>;

    void hookEvents();
    /// The item summary, appearances, titles and achievements pages
    /// (characterinfomodels.cpp).
    void hookModelEvents();
    /// CIP 0x100d0f260: shows `dataType`, asking the server unless it is
    /// already there.
    void request(int dataType, bool force);
    void sendRequest(int dataType, int page);
    void received(int dataType);
    void onErrorReceived(int errorCode);
    void rebuildStatsBlocks();

    template <class T>
    void replace(T*& member, T* object);
    void replace(QList<QObject*>& member, QList<QObject*> objects);

    std::vector<client::gamecore::Hook> m_hooks;
    TMessageDialogController* m_errorMessage;
    bool m_shown = false;
    int m_dataType = NoData;
    int m_nextDataType = GeneralStats;
    uint32_t m_characterId = 0;
    QSet<int> m_received;
    /// Pagination of the recent deaths / kills (CIP: storage vfunc 0xd8 / 0xe0).
    int m_page[2] = { 1, 1 };
    int m_pages[2] = { 1, 1 };
    int m_inspectionState = 0;

Q_SIGNALS:
    void inspectionStateChanged();
};

}
