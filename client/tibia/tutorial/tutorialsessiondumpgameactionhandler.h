#pragma once

#include "input/gameaction.h"

#include <QObject>
#include <QString>

#include <memory>

namespace tibia {
namespace tutorial {

class TTutorialProgressUploader;

/// CIP `tutorial::TTutorialSessiondumpGameActionHandler`: uploads tutorial
/// progress (and finishes the tutorial at "TutorialEnd") and turns the tutorial
/// mode switch into the client options mode and context menu switches.
class TTutorialSessiondumpGameActionHandler : public QObject
{
    Q_OBJECT
public:
    TTutorialSessiondumpGameActionHandler(std::shared_ptr<TTutorialProgressUploader> uploader, const QString& characterName);
    ~TTutorialSessiondumpGameActionHandler() override;

Q_SIGNALS:
    void publishGameAction(tibia::input::TGameActionPtr pGameAction);

public Q_SLOTS:
    virtual void handleGameAction(tibia::input::TGameActionPtr pGameAction);

private:
    std::shared_ptr<TTutorialProgressUploader> m_uploader;
    QString m_characterName;
};

}
}
