#pragma once

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace tibia {
namespace gamewindow {

/// NpcDialog.qml's keyword buttons (CIP: the anonymous-namespace TKeywordButtonModel, a
/// shared::TSimpleVectorAbstractListModel<chat::TKeywordButton>; roleNames ,
/// data , update).
class TNpcKeywordButtonModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    /// chat::TKeywordButton: the KEYWORDBUTTONICON_* value and the keyword the button says.
    struct Button
    {
        int icon = 0;
        QString keyword;
    };

    enum Role
    {
        ButtonImageRole = 0x101,
        KeywordRole = 0x102,
    };

    using QAbstractListModel::QAbstractListModel;

    /// CIP : rows removed or inserted to the new count, then every row changed.
    void setButtons(std::vector<Button> buttons);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// CIP : the button's cell of the "icons-npcdialog" strip; empty for an icon
    /// CIP does not know (its map::at throws).
    static QString imageOf(int icon);

private:
    std::vector<Button> m_buttons;
};

}
}
