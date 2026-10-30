#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <string>
#include <tuple>
#include <vector>

namespace tibia::gamewindow {

/// A dialog the server opens (ServerModalDialog.qml; CIP:
/// TServerModalDialogController).
class TServerModalDialogController final : public TDialogController<cip::tibia::gamewindow::TServerModalDialogController>
{
    Q_OBJECT
public:
    using Options = std::vector<std::tuple<uint8_t, std::string>>;

    explicit TServerModalDialogController(QObject* parent = nullptr);

    /// CIP 0x100c24730: replaces the shown dialog.
    void open(uint32_t id, const QString& title, const QString& text, const Options& buttons, uint8_t enterButton, uint8_t escapeButton,
              const Options& choices);

    void answerButtonClicked(int ButtonIndex, int SelectionIndex) override;

private:
    uint32_t m_id = 0;
    std::vector<uint8_t> m_buttonIds;
    std::vector<uint8_t> m_choiceIds;
};

}
