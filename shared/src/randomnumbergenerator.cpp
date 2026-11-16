#include "randomnumbergenerator.h"

namespace tibia::shared {

TRandomNumberGenerator::TRandomNumberGenerator()
{
    std::random_device device("/dev/urandom");
    m_engine.seed(device());
}

std::shared_ptr<TRandomNumberGenerator> TRandomNumberGenerator::instance()
{
    static std::shared_ptr<TRandomNumberGenerator> generator;
    if (!generator)
        generator = std::make_shared<TRandomNumberGenerator>();
    return generator;
}

qint64 TRandomNumberGenerator::getRandomNumber(const qint64 min, const qint64 max)
{
    return getRandomLongLong(min, max);
}

int TRandomNumberGenerator::getRandomInt(const int min, const int max)
{
    Q_ASSERT(min <= max);
    return std::uniform_int_distribution<int>(min, max)(m_engine);
}

quint64 TRandomNumberGenerator::getRandomUnsignedLong(const quint64 min, const quint64 max)
{
    Q_ASSERT(min <= max);
    return std::uniform_int_distribution<quint64>(min, max)(m_engine);
}

float TRandomNumberGenerator::getRandomFloat(const float min, const float max)
{
    Q_ASSERT(min <= max);
    return std::uniform_real_distribution<float>(min, max)(m_engine);
}

qint64 TRandomNumberGenerator::getRandomLongLong(const qint64 min, const qint64 max)
{
    Q_ASSERT(min <= max);
    return std::uniform_int_distribution<qint64>(min, max)(m_engine);
}

long TRandomNumberGenerator::getRandomLong(const long min, const long max)
{
    Q_ASSERT(min <= max);
    return std::uniform_int_distribution<long>(min, max)(m_engine);
}

}
