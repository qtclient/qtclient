#pragma once

#include <QtGlobal>

#include <memory>
#include <random>

namespace tibia::shared {

/// CIP `shared::TRandomNumberGenerator`: uniform numbers in [min, max] from a
/// minstd_rand engine seeded by the system's random device. Every range
/// asserts min <= max.
class TRandomNumberGenerator
{
public:
    TRandomNumberGenerator();
    virtual ~TRandomNumberGenerator() = default;

    /// The process-wide generator, created on first use (not thread-safe, as in CipSoft).
    static std::shared_ptr<TRandomNumberGenerator> instance();

    /// Same as getRandomLongLong.
    virtual qint64 getRandomNumber(qint64 min, qint64 max);
    virtual int getRandomInt(int min, int max);
    virtual quint64 getRandomUnsignedLong(quint64 min, quint64 max);
    virtual float getRandomFloat(float min, float max);
    virtual qint64 getRandomLongLong(qint64 min, qint64 max);
    virtual long getRandomLong(long min, long max);

private:
    std::minstd_rand m_engine;
};

}
