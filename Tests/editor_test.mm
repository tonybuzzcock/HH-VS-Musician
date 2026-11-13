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

// Does the preset file hold this name line?
static bool fileHasLine (const std::string& path, const char* needle)
{
    bool found = false;
    FILE* nf = std::fopen (path.c_str(), "r");
    if (nf)
    {
        char ln[256];
        while (std::fgets (ln, sizeof (ln), nf)) if (std::strstr (ln, needle)) found = true;
        std::fclose (nf);
    }
    return found;
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: %s <plugin binary> <png dir>\n", argv[0]); return 2; }
    setvbuf (stdout, 0, _IONBF, 0);   // unbuffered, so a crash still shows how far it got
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    const std::string dir = argv[2];

    // Keep the test's presets away from any real ones
    const std::string presetFile = dir + "/test_presets/Presets.txt";
    setenv ("TBVS_PRESET_FILE", presetFile.c_str(), 1);
    std::remove (presetFile.c_str());

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
    mouse (win, panel, NSLeftMouseDown,    NSMakePoint (393, 112));
    mouse (win, panel, NSLeftMouseDragged, NSMakePoint (393, 87));
    mouse (win, panel, NSLeftMouseDragged, NSMakePoint (393, 62));
    mouse (win, panel, NSLeftMouseUp,      NSMakePoint (393, 62));
    const float g1 = e->getParameter (e, tbvs::kCh2Gain);
    std::printf ("  Gain %.2f -> %.2f\n", g0, g1);
    check (std::fabs (g1 - (g0 + 0.25f)) < 0.02f, "dragging a knob up turns it up");
    check (automateCalls > 0 && beginCalls == 1 && endCalls == 1, "host gets begin / automate / end");

    // Double-click returns it to default
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (393, 112), 2);
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (393, 112), 2);
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
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (845, 112));
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (845, 112));
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

    std::printf ("\nPresets\n");
    // Slot k (0-based) centre
    #define SLOT(k) NSMakePoint (672.0 + (k) * 27.0 + 12.0, 296.0)
    e->setParameter (e, tbvs::kCh2Gain, 0.3f);
    e->setParameter (e, tbvs::kValveSound, 1.0f);
    pump (0.1);
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (635, 296));    // SAVE
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (635, 296));
    mouse (win, panel, NSLeftMouseDown, SLOT (2));                   // slot 3
    mouse (win, panel, NSLeftMouseUp,   SLOT (2));
    FILE* pf = std::fopen (presetFile.c_str(), "r");
    check (pf != 0, "SAVE then a slot writes the preset file");
    if (pf) std::fclose (pf);

    // Saving opens a name box over the readout
    NSTextField* field = nil;
    for (NSView* sv in [panel subviews]) if ([sv isKindOfClass: [NSTextField class]]) field = (NSTextField*) sv;
    check (field != nil, "saving opens a box to name the preset");
    renderPNG (panel, dir + "/panel_naming.png");
    if (field)
    {
        [field setStringValue: @"A forty character preset name for tests"];   // 40 characters
        [(id) panel controlTextDidChange: nil];
        renderPNG (panel, dir + "/panel_naming_long.png");
        [field setStringValue: @"Big slow flange"];
        [field sendAction: [field action] to: [field target]];       // Return
        pump (0.2);
    }
    check (fileHasLine (presetFile, "name 3: Big slow flange"), "Return saves the name with the preset");
    bool gone = true;
    for (NSView* sv in [panel subviews]) if ([sv isKindOfClass: [NSTextField class]]) gone = false;
    check (gone, "and the name box goes away");
    renderPNG (panel, dir + "/panel_preset_saved.png");

    e->setParameter (e, tbvs::kCh2Gain, 0.9f);
    e->setParameter (e, tbvs::kValveSound, 0.0f);
    pump (0.1);
    automateCalls = 0;
    mouse (win, panel, NSLeftMouseDown, SLOT (2));
    mouse (win, panel, NSLeftMouseUp,   SLOT (2));
    check (std::fabs (e->getParameter (e, tbvs::kCh2Gain) - 0.3f) < 1e-5f
           && e->getParameter (e, tbvs::kValveSound) >= 0.5f, "clicking the slot recalls the saved settings");
    check (automateCalls >= tbvs::kNumParams, "recall tells the host about every parameter");
    renderPNG (panel, dir + "/panel_preset_loaded.png");

    // Clicking the readout while the preset is in use renames it; Esc keeps the old name
    mouse (win, panel, NSLeftMouseDown, NSMakePoint (1038.5, 296));
    mouse (win, panel, NSLeftMouseUp,   NSMakePoint (1038.5, 296));
    field = nil;
    for (NSView* sv in [panel subviews]) if ([sv isKindOfClass: [NSTextField class]]) field = (NSTextField*) sv;
    check (field != nil && [[field stringValue] isEqualToString: @"Big slow flange"], "clicking the readout opens the name to edit");
    if (field)
    {
        [field setStringValue: @"Something else"];
        [(id) panel control: field textView: nil doCommandBySelector: @selector (cancelOperation:)];   // Esc
        pump (0.2);
    }
    check (fileHasLine (presetFile, "name 3: Big slow flange"), "Esc keeps the old name");

    const float before = e->getParameter (e, tbvs::kCh2Gain);
    mouse (win, panel, NSLeftMouseDown, SLOT (5));                   // empty slot 6
    mouse (win, panel, NSLeftMouseUp,   SLOT (5));
    check (e->getParameter (e, tbvs::kCh2Gain) == before, "an empty slot changes nothing");

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
    [pool release];      // drain before unloading the plugin's code
    dlclose (lib);

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL EDITOR CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
