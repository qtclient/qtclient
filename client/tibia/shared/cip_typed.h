#pragma once

namespace tibia {

/// Base for handwritten CIP RTTI types that have no `cip::` moc/codegen class.
/// Prefer `public cip::tibia::…::TFoo` whenever codegen exists. This exists only
/// so the obligation ledger can require inheritance (`class TFoo :`) without
/// inventing empty moc wrappers.
class CipTyped
{
protected:
    CipTyped() = default;
    ~CipTyped() = default;
    CipTyped(const CipTyped&) = default;
    CipTyped& operator=(const CipTyped&) = default;
};

}
