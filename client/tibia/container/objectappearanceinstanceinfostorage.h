#pragma once

#include "appearances/objectappearanceinstancetypeid.h"

#include <cip_gen.h>

#include <QString>
#include <QTimer>

#include <map>

namespace tibia::container {

/// What the client knows about object instances it asked the server about
/// (CIP: TObjectAppearanceInstanceInfoStorage): a count and the server's name
/// for each. Changes are announced once, on the next event loop turn.
class TObjectAppearanceInstanceInfoStorage : public cip::tibia::container::TObjectAppearanceInstanceInfoStorage
{
    Q_OBJECT
public:
    using TObjectAppearanceInstanceTypeID = appearances::TObjectAppearanceInstanceTypeID;

    /// One instance's entry.
    struct TInfo
    {
        int count = 0;
        QString name;

        /// No count and no name.
        bool isEmpty() const { return count == 0 && name.isEmpty(); }
    };

    /// Orders by type, then liquid, then tier.
    struct TCompare
    {
        bool operator()(const TObjectAppearanceInstanceTypeID& left, const TObjectAppearanceInstanceTypeID& right) const;
    };

    explicit TObjectAppearanceInstanceInfoStorage(QObject* parent = nullptr);

    /// Zeroes every count and drops the entries without a name.
    virtual void resetCounts();
    virtual void setCount(const TObjectAppearanceInstanceTypeID& instance, int count);
    /// 0 for an unknown instance.
    virtual int count(const TObjectAppearanceInstanceTypeID& instance) const;
    virtual void setName(const TObjectAppearanceInstanceTypeID& instance, const QString& name);
    /// Empty for an unknown instance.
    virtual QString name(const TObjectAppearanceInstanceTypeID& instance) const;
    /// The instance's entry; an empty one when it is unknown.
    TInfo info(const TObjectAppearanceInstanceTypeID& instance) const;

public Q_SLOTS:
    void onAsyncEmitObjectInfosChanged();

private:
    QTimer m_emitTimer;
    std::map<TObjectAppearanceInstanceTypeID, TInfo, TCompare> m_infos;
};

} // namespace tibia::container
