#pragma once

// "Forme libre": on the Mac, the plugin window of the host (Ableton Live) is made transparent and
// its title bar hidden, so only the plugin's own shape is visible (crystal, planet, cassette...).
// Done with the Objective-C runtime from C++ (no .mm file needed). Does nothing on other systems.

#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_MAC
 #include <objc/message.h>
 #include <objc/runtime.h>
 #include <CoreGraphics/CoreGraphics.h>
#endif

namespace tchk::macwin
{
#if JUCE_MAC
namespace detail
{
    template <typename R = id, typename... A>
    inline R send (id obj, const char* sel, A... args)
    {
        if (obj == nil) return R();
        return reinterpret_cast<R (*) (id, SEL, A...)> (objc_msgSend) (obj, sel_registerName (sel), args...);
    }
    inline bool responds (id obj, const char* sel)
    {
        return obj != nil && send<BOOL> (obj, "respondsToSelector:", sel_registerName (sel));
    }
    inline id cls (const char* name) { return (id) objc_getClass (name); }
    inline id windowOf (void* nsView) { return nsView != nullptr ? send ((id) nsView, "window") : nil; }
}

// on = transparent window without title bar; off = everything back as the host made it
inline bool setFreeShape (void* nsView, bool on)
{
    using namespace detail;
    id view = (id) nsView;
    id win = windowOf (nsView);
    if (win == nil) return false;
    id clear = send (cls ("NSColor"), "clearColor");
    id normal = send (cls ("NSColor"), "windowBackgroundColor");
    send<void> (win, "setOpaque:", (BOOL) (on ? NO : YES));
    send<void> (win, "setBackgroundColor:", on ? clear : normal);
    send<void> (win, "setHasShadow:", (BOOL) (on ? NO : YES));
    if (responds (win, "setTitlebarAppearsTransparent:")) send<void> (win, "setTitlebarAppearsTransparent:", (BOOL) (on ? YES : NO));
    if (responds (win, "setTitleVisibility:")) send<void> (win, "setTitleVisibility:", (long) (on ? 1 : 0));
    const unsigned long fullSize = 1ul << 15;
    const unsigned long mask = send<unsigned long> (win, "styleMask");
    send<void> (win, "setStyleMask:", on ? (mask | fullSize) : (mask & ~fullSize));
    for (long b = 0; b < 3; ++b)
        if (id button = send (win, "standardWindowButton:", b))
            send<void> (button, "setHidden:", (BOOL) (on ? YES : NO));
    // the views between ours and the window: no background
    for (id v = view; v != nil; v = send (v, "superview"))
    {
        if (on) send<void> (v, "setWantsLayer:", (BOOL) YES);
        if (id layer = send (v, "layer"))
        {
            send<void> (layer, "setOpaque:", (BOOL) (on ? NO : YES));
            if (v != view) send<void> (layer, "setBackgroundColor:", on ? CGColorGetConstantColor (kCGColorClear) : (CGColorRef) nullptr);
        }
    }
    send<void> (win, "invalidateShadow");
    send<void> (send (win, "contentView"), "setNeedsDisplay:", (BOOL) YES);
    return true;
}

// move the window with the mouse (call from a mouseDown on the plugin's background)
inline void startDrag (void* nsView)
{
    using namespace detail;
    id win = windowOf (nsView);
    id app = send (cls ("NSApplication"), "sharedApplication");
    id ev = send (app, "currentEvent");
    if (win != nil && ev != nil && responds (win, "performWindowDragWithEvent:"))
        send<void> (win, "performWindowDragWithEvent:", ev);
}

// close the plugin window (our own ✕, the host's one is hidden)
inline void close (void* nsView)
{
    using namespace detail;
    if (id win = windowOf (nsView)) send<void> (win, "performClose:", (id) nil);
}
#else
inline bool setFreeShape (void*, bool) { return false; }
inline void startDrag (void*) {}
inline void close (void*) {}
#endif
} // namespace tchk::macwin
