#include "config/configciptypes.h"

namespace tibia::config {

ICharacterConfigurationBase::ICharacterConfigurationBase(QObject* parent)
    : cip::tibia::config::ICharacterConfigurationBase(parent)
{
}

void ICharacterConfigurationBase::notifyConfigurationChangedFromUserInput()
{
    Q_EMIT configurationChangedFromUserInput();
}

ICharacterConfigurationBase* ICharacterConfigurationBase::instance()
{
    static ICharacterConfigurationBase keep;
    return &keep;
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
    (void)ICharacterConfigurationBase::instance();
}

namespace {
const bool kTipICharacterConfigurationBaseLinked =
    (ICharacterConfigurationBase::instance() != nullptr);
}

} // namespace tibia::config
