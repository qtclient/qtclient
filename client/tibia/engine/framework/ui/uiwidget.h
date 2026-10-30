#pragma once
#include "declarations.h"
#include <framework/luaengine/luaobject.h>
#include <framework/util/rect.h>

/// Stub: OTUI retired; tibia uses QML. Kept so gamecore types that still name widgets compile.
class UIWidget : public LuaObject
{
public:
    virtual ~UIWidget() = default;

    std::string getId() { return {}; }
    std::string getSource() { return {}; }
    void setDraggable(bool) {}
    void setParent(const UIWidgetPtr&) {}
    void setRootChild(bool v) { m_rootChild = v; }
    bool isRootChild() const { return m_rootChild; }
    void addOnDestroyCallback(std::string_view, std::function<void()>) {}
    void removeOnDestroyCallback(std::string_view) {}

    UIWidgetList getChildren() { return {}; }
    virtual void draw(const Rect&, DrawPoolType) {}
    virtual void draw(DrawPoolType) {}
    virtual void setShader(std::string_view) {}
    virtual void setShader(std::string_view, float, float) {}
    virtual void drawSelf(DrawPoolType) {}

private:
    bool m_rootChild{ false };
};
