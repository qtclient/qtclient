#pragma once

namespace tibia::cip_ports {

/// Marker base for CIP RTTI classes that have no moc/`cip::` codegen surface.
/// Ledger `ported` requires a handwritten `class TFoo :` under `client/tibia/`.
class CipNativePort
{
public:
    CipNativePort() = default;
};

}
