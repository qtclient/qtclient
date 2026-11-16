#include "tutorial/sessiondumphintbase.h"

namespace tibia::tutorial {

TSessiondumpHintBase::TSessiondumpHintBase(const EType type)
    : m_type(type)
{
}

TSessiondumpHintBase::~TSessiondumpHintBase() = default;

TSessiondumpHintBase::EType TSessiondumpHintBase::type() const { return m_type; }

int64_t TSessiondumpHintBase::timestamp() const { return m_timestamp; }

void TSessiondumpHintBase::setTimestamp(const int64_t& timestamp) { m_timestamp = timestamp; }

bool TSessiondumpHintBase::isDone() const { return m_done; }

void TSessiondumpHintBase::setDone() { m_done = true; }

}
