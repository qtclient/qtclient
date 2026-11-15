#pragma once

// CIP highscores: the server answer's storage and the message handler filling it.

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>
#include <QString>
#include <QStringList>

#include <utility>
#include <vector>

namespace tibia {
namespace highscores {

/// Creates the session's highscores storage and handler under `parent`.
void ownProductSurface(QObject* parent);

/// One row of the highscore table (CIP: 0x78-byte entries).
struct THighscoreEntry
{
    uint rank = 0;
    QString character;
    QString loyaltyTitle;
    QString vocation;
    QString world;
    quint16 level = 0;
    bool isOwnCharacter = false;
    qulonglong score = 0;
};

/// The last highscores answer of the server (CIP: THighscoresStorage, held by
/// the game session). highScoresError when the server has no data,
/// highScoresUpdated after a new answer.
class THighscoresStorage final : public cip::tibia::highscores::THighscoresStorage
{
    Q_OBJECT
public:
    using Vocation = std::pair<int, QString>;
    using Category = std::pair<int, QString>;

    explicit THighscoresStorage(QObject* parent = nullptr);
    ~THighscoresStorage() override;

    static THighscoresStorage* liveInstance();

    /// The answer's error state; 1: the server has no data.
    int state() const { return m_state; }
    const QStringList& worlds() const { return m_worlds; }
    const QString& selectedWorld() const { return m_selectedWorld; }
    /// The world types the answer was filtered by.
    const std::vector<int>& selectedWorldTypes() const { return m_selectedWorldTypes; }
    int battlEyeType() const { return m_battlEyeType; }
    /// All vocations; the first is the server's "all vocations" entry.
    const std::vector<Vocation>& vocations() const { return m_vocations; }
    int selectedVocation() const { return m_selectedVocation; }
    const std::vector<Category>& categories() const { return m_categories; }
    int selectedCategory() const { return m_selectedCategory; }
    const std::vector<THighscoreEntry>& entries() const { return m_entries; }
    int page() const { return m_page; }
    int totalPages() const { return m_totalPages; }
    int selectedIndex() const { return m_selectedIndex; }
    bool showLoyaltyTitle() const { return m_showLoyaltyTitle; }
    int valueType() const { return m_valueType; }
    /// Unix seconds of the server's last highscore update.
    uint lastUpdate() const { return m_lastUpdate; }

private:
    friend class THighscoresProtocolMessageHandler;

    int m_state = 0;
    QStringList m_worlds;
    QString m_selectedWorld;
    std::vector<int> m_selectedWorldTypes;
    int m_battlEyeType = -1;
    std::vector<Vocation> m_vocations;
    int m_selectedVocation = -1;
    std::vector<Category> m_categories;
    int m_selectedCategory = 0;
    std::vector<THighscoreEntry> m_entries;
    int m_page = 0;
    int m_totalPages = 0;
    int m_selectedIndex = -1;
    bool m_showLoyaltyTitle = false;
    int m_valueType = 1;
    uint m_lastUpdate = 0;
};

class THighscoresProtocolMessageHandler final : public cip::tibia::highscores::THighscoresProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::highscores::THighscoresProtocolMessageHandler::THighscoresProtocolMessageHandler;

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode);
};

}
}
