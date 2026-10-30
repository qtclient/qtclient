#pragma once

// CIP-named authentication surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::authentication {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TAuthenticationAndEncryptionInfo final : public ::tibia::CipTyped {};

class TAuthenticationJsonDocumentHelper final : public ::tibia::CipTyped {};

class TAuthenticationProcessController final : public ::tibia::CipTyped {};

class TEmailCodeRequestUploader final : public cip::tibia::authentication::TEmailCodeRequestUploader
{
    Q_OBJECT
public:
    using cip::tibia::authentication::TEmailCodeRequestUploader::TEmailCodeRequestUploader;
};

class TGameserverLoginProcessController final : public cip::tibia::authentication::TGameserverLoginProcessController
{
    Q_OBJECT
public:
    using cip::tibia::authentication::TGameserverLoginProcessController::TGameserverLoginProcessController;
};

class TLoginConfirmationCodeRequestUploader final : public cip::tibia::authentication::TLoginConfirmationCodeRequestUploader
{
    Q_OBJECT
public:
    using cip::tibia::authentication::TLoginConfirmationCodeRequestUploader::TLoginConfirmationCodeRequestUploader;
};

class TLoginProtocolMessageHandler final : public cip::tibia::authentication::TLoginProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::authentication::TLoginProtocolMessageHandler::TLoginProtocolMessageHandler;
};

} // namespace tibia::authentication

