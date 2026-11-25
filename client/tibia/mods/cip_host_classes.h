#pragma once

// Host implementations of CipSoft classes a pack may make through cip_object (host API v9,
// class_supported == 2). Read only by the generated cip_objects_host.cpp
// (tools/cip/gen_cip_objects.py).
//
// The default is the QML binding table (cip_impl.h CIP_IMPLEMENT): a class QML makes as the
// host's implementation is made the same way for a pack. A host controller that is not a QML
// element but that CipSoft's QML embeds in pages a pack serves is added here with
// QTCLIENT_CIP_HOST_CLASS (it must be constructible from a Qt parent alone).

#include <cip_gen.h>

#if __has_include("cip_impl.h")
#include "cip_impl.h"
#endif

#include "gamewindow/characterinfodialogcontroller.h"

#include <QObject>
#include <QQuickItem>

#include <type_traits>

namespace cip {

template <class Base>
struct HostImpl
{
    using type = typename Impl<Base>::type;
};

} // namespace cip

#define QTCLIENT_CIP_HOST_CLASS(BASE, IMPL) \
    template <> struct cip::HostImpl<BASE> { using type = IMPL; }

// CipSoft's character page (the friends account search page embeds it).
QTCLIENT_CIP_HOST_CLASS(::cip::tibia::gamewindow::TCharacterInfoDialogController,
                        ::tibia::gamewindow::TCharacterInfoDialogController);

namespace tibia::mods::cipobjects {

template <class Base>
constexpr bool cipHasHost()
{
    using I = typename ::cip::HostImpl<Base>::type;
    if constexpr (std::is_same_v<I, Base> || std::is_abstract_v<I>)
        return false;
    else
        return std::is_constructible_v<I, QObject*> || std::is_default_constructible_v<I>;
}

template <class Base>
QObject* cipMakeHost(QObject* parent)
{
    using I = typename ::cip::HostImpl<Base>::type;
    if constexpr (!cipHasHost<Base>())
        return nullptr;
    else if constexpr (std::is_base_of_v<QQuickItem, I>) {
        // Quick items take an item parent; the Qt object parent is set afterwards.
        I* object = nullptr;
        if constexpr (std::is_default_constructible_v<I>)
            object = new I;
        else
            object = new I(static_cast<QQuickItem*>(nullptr));
        object->QObject::setParent(parent);
        return object;
    } else if constexpr (std::is_constructible_v<I, QObject*>)
        return new I(parent);
    else {
        I* object = new I;
        object->setParent(parent);
        return object;
    }
}

} // namespace tibia::mods::cipobjects
