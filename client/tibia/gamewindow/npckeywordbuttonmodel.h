#pragma once

#include "chat/chatgameactionhandler.h"
#include "shared/simplevectorabstractlistmodel.h"

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace tibia {
namespace gamewindow {

using TKeywordButton = chat::TKeywordButton;

/// NpcDialog.qml's keyword buttons (CIP: the anonymous-namespace TKeywordButtonModel, a
/// shared::TSimpleVectorAbstractListModel<chat::TKeywordButton>).
class TNpcKeywordButtonModel final : public shared::TSimpleVectorAbstractListModel<TKeywordButton>
{
    Q_OBJECT
public:
    using Button = TKeywordButton;

    enum Role
    {
        ButtonImageRole = 0x101,
        KeywordRole = 0x102,
    };

    using TSimpleVectorAbstractListModel::TSimpleVectorAbstractListModel;

    QHash<int, QByteArray> roleNames() const override;

    /// CIP : the button's cell of the "icons-npcdialog" strip; empty for an icon
    /// CIP does not know (its map::at throws).
    static QString imageOf(int icon);

protected:
    QVariant itemData(const TKeywordButton& button, int role) const override;
};

}
}
