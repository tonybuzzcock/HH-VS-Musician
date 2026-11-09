// editor_test.mm  (macOS only)
// Loads the built plugin, opens its panel in an offscreen window the way a host
// does, renders screenshots, drives the controls with synthetic mouse events,
// checks host automation messages, and closes it again.
//
//   editor_test <plugin binary> <output dir for PNGs>

#import <Cocoa/Cocoa.h>

#include "../Source/VST2/vst2_abi.h"
#include "../Source/VST2/Params.h"
#include "../Source/VST2/Editor.h"

#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

static int automateCalls = 0, beginCalls = 0, endCalls = 0;
static VstIntPtr VST_CALL hostCallback (AEffect*, VstInt32 opcode, VstInt32, VstIntPtr, void*, float)
{
    switch (opcode)
    {
        case audioMasterVersion:   return 2400;
        case audioMasterAutomate:  ++automateCalls; return 0;
        case audioMasterBeginEdit: ++beginCalls; return 1;
        case audioMasterEndEdit:   ++endCalls; return 1;
        default:                   return 0;
    }
}

typedef AEffect* (*EntryProc) (audioMasterCallback);

static VstIntPtr call (AEffect* e, VstInt32 op, VstInt32 idx = 0, VstIntPtr val = 0, void* ptr = 0, float opt = 0.0f)
{
    return e->dispatcher (e, op, idx, val, ptr, opt);
}

static NSData* renderPNG (NSView* v, const std::string& path)
{
    const NSRect b = [v bounds];
    NSBitmapImageRep* rep = [v bitmapImageRepForCachingDisplayInRect: b];
    [v cacheDisplayInRect: b toBitmapImageRep: rep];
    NSData* png = [rep representationUsingType: NSPNGFileType properties: [NSDictionary dictionary]];
    [png writeToFile: [NSString stringWithUTF8String: path.c_str()] atomically: YES];
    std::printf ("  wrote %s (%lu bytes)\n", path.c_str(), (unsigned long) [png length]);
    return png;
}

// Send a mouse event at a point in the panel's (flipped) coordinates.
static void mouse (NSWindow* w, NSView* v, NSEventType type, NSPoint panelPt, int clicks = 1)
{
    const NSPoint wp = [v convertPoint: panelPt toView: nil];
    NSEvent* e = [NSEvent mouseEventWithType: type location: wp modifierFlags: 0 timestamp: 0
                                windowNumber: [w windowNumber] context: nil eventNumber: 0
                                  clickCount: clicks pressure: 1.0f];
    if (type == NSLeftMouseDown) [v mouseDown: e];
    else if (type == NSLeftMouseDragged) [v mouseDragged: e];
    else [v mouseUp: e];
}

static void pump (double secs)
{
    [[NSRunLoop currentRunLoop] runUntilDate: [NSDate dateWithTimeIntervalSinceNow: secs]];
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: %s <plugin binary> <png dir>\n", argv[0]); return 2; }
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    const std::string dir = argv[2];

    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (! lib) { std::printf ("dlopen failed: %s\n", dlerror()); return 1; }
    EntryProc entry = (EntryProc) dlsym (lib, "VSTPluginMain");
    if (! entry) { std::printf ("no entry point\n"); return 1; }
    AEffect* e = entry (hostCallback);
    call (e, effOpen);
    call (e, effSetSampleRate, 0, 0, 0, 44100.0f);
    call (e, effMainsChanged, 0, 1);

    std::printf ("Editor\n");
    check ((e->flags & effFlagsHasEditor) != 0, "plugin reports a custom editor");
    ERect* r = 0;
    call (e, effEditGetRect, 0, 0, &r);
    check (r && r->right - r->left == tbvs::kEditorWidth && r->bottom - r->top == tbvs::kEditorHeight,
           "editor size 1200 x 330");

    NSWindow* win = [[NSWindow alloc] initWithContentRect: NSMakeRect (0, 0, tbvs::kEditorWidth, tbvs::kEditorHeight)
                                                styleMask: NSBorderlessWindowMask
                                                  backing: NSBackingStoreBuffered defer: NO];
    NSView* parent = [win contentView];
    check (call (e, effEditOpen, 0, 0, parent) == 1, "effEditOpen succeeds");
    check ([[parent subviews] count] == 1, "panel view added to the host's view");
    NSView* panel = [[parent subviews] count] ? [[parent subviews] objectAtIndex: 0] : nil;
    if (! panel) return 1;
    check (NSEqualSizes ([panel frame].size, NSMakeSize (tbvs::kEditorWidth, tbvs::kEditorHeight)), "panel has the declared size");

    NSData* pngDefault = renderPNG (panel, dir + "/panel_default.png");
    check ([pngDefault length] > 10000, "panel renders");

    std::printf ("\nControls\n");
    // Drag the Channel 2 Gain knob up 50 px: +0.25
    const float g0 = e->getParameter (e, tbvs::kCh2Gain);
    automateCalls = beginCalls = endCalls = 0;
    mouse (win, panel, NSLeftMouseDown,    NSMakePoint (400, 112));
    mouse (win, panel, NSLeftMouseDragged, NSMakePoint (400, 87));
    mouse (win, panel, NSLeftMouseDragged, NSMakePoint (400, 62));
    mouse (win, panel, NSLeftMouseUp,      NSMakePoint (400, 62));
    const float g1 = e->getParameter (e, tbvs::kCh2Gain);
    std::printf ("  Gain %.2f -> %.2f\n", g0, g1);
    check (std::fabs (g1 - (g0 + 0.25f)) < 0.02f, "dragging a knob up turns it up");
    check (automateCalls > 0 && beginCalls == 1 && endCalls == 1, "host gets begin / automate / end");

    // Double-click returns it to default
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (400, 112), 2);
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (400, 112), 2);
    check (std::fabs (e->getParameter (e, tbvs::kCh2Gain) - tbvs::kDefaults[tbvs::kCh2Gain]) < 1e-6f, "double-click resets a knob");

    // Rocker: Channel 1 Reverb, press the top half for ON
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (170, 196));
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (170, 196));
    check (e->getParameter (e, tbvs::kCh1Reverb) >= 0.5f, "Ch 1 Reverb rocker switches on");
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (170, 214));
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (170, 214));
    check (e->getParameter (e, tbvs::kCh1Reverb) < 0.5f, "and off again");

    // Voice selector toggles
    const float vz = e->getParameter (e, tbvs::kVoice);
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (830, 112));
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (830, 112));
    check ((e->getParameter (e, tbvs::kVoice) >= 0.5f) != (vz >= 0.5f), "Voice selector changes position");

    // Channel slide switch to "1+2", then "1"
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (199, 296));
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (199, 296));
    check (tbvs::channelSlot (e->getParameter (e, tbvs::kChannel)) == 2, "Channel slide switch to 1+2");
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (141, 296));
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (141, 296));
    check (tbvs::channelSlot (e->getParameter (e, tbvs::kChannel)) == 0, "Channel slide switch to 1");

    // Speaker rocker off
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (330, 306));
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (330, 306));
    check (e->getParameter (e, tbvs::kSpeaker) < 0.5f, "Speaker rocker switches off");

    std::printf ("\nHost automation shows on the panel\n");
    e->setParameter (e, tbvs::kMasterVolume, 0.9f);
    e->setParameter (e, tbvs::kCh2Reverb, 0.0f);
    pump (0.3);
    NSData* pngAfter = renderPNG (panel, dir + "/panel_changed.png");
    check (! [pngAfter isEqualToData: pngDefault], "panel redraws after host changes");

    // A showcase render: Gain channel set up for sustain
    e->setParameter (e, tbvs::kCh2Gain, 0.8f);
    e->setParameter (e, tbvs::kVoice, 1.0f);
    e->setParameter (e, tbvs::kValveSound, 1.0f);
    e->setParameter (e, tbvs::kCh2Reverb, 1.0f);
    e->setParameter (e, tbvs::kChannel, 0.5f);
    e->setParameter (e, tbvs::kSpeaker, 1.0f);
    e->setParameter (e, tbvs::kMasterVolume, 0.35f);
    pump (0.3);
    renderPNG (panel, dir + "/panel_showcase.png");

    std::printf ("\nClose\n");
    call (e, effEditClose);
    check ([[parent subviews] count] == 0, "effEditClose removes the panel");
    check (call (e, effEditOpen, 0, 0, parent) == 1 && [[parent subviews] count] == 1, "reopens");
    call (e, effEditClose);
    call (e, effMainsChanged, 0, 0);
    call (e, effClose);
    [win release];
    dlclose (lib);
    [pool release];

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL EDITOR CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
