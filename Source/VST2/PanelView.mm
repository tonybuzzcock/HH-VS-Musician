// PanelView.mm
// The custom panel for HH VS Musician (Reverb): an original design laid out
// like the amp's controls. Opaque silver, black lettering, grey knurled knobs
// with a red position line, rocker switches for the channel Reverb switches and
// Speaker, a slide switch for Channel, and a strip of plugin controls along
// the bottom.
//
// Cocoa, manual retain/release, and only APIs that exist in Mac OS X 10.8.
// Everything is drawn in code; no image files.

#import <Cocoa/Cocoa.h>

#include "Editor.h"
#include "Params.h"
#include "Presets.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace tbvs;

namespace
{

enum CtlType { kCtlKnob, kCtlTone, kCtlSelector2, kCtlRocker, kCtlSlide3, kCtlSmallKnob };

struct Ctl
{
    int param;
    int type;
    CGFloat cx, cy, r;
    const char* label;
    const char* posA;      // selector / rocker / slide labels
    const char* posB;
    const char* posC;
};

const CGFloat kStripTop = 250.0;

// Main panel, left to right like the amp; then the plugin strip.
const Ctl kCtls[] = {
    // Channel 1 (Volume channel)
    { kCh1Volume,    kCtlKnob,      80, 112, 22, "VOLUME",   0, 0, 0 },
    { kCh1Bass,      kCtlTone,     170, 112, 22, "BASS",     0, 0, 0 },
    { kCh1Treble,    kCtlTone,     260, 112, 22, "TREBLE",   0, 0, 0 },
    { kCh1Reverb,    kCtlRocker,   170, 205, 0,  "REVERB",   "OFF", "ON", 0 },
    // Channel 2 (Gain channel)
    { kCh2Gain,      kCtlKnob,     393, 112, 22, "GAIN",     0, 0, 0 },
    { kCh2Bass,      kCtlTone,     483, 112, 22, "BASS",     0, 0, 0 },
    { kMiddle,       kCtlTone,     573, 112, 22, "MIDDLE",   0, 0, 0 },
    { kCh2Treble,    kCtlTone,     663, 112, 22, "TREBLE",   0, 0, 0 },
    { kPresence,     kCtlTone,     753, 112, 22, "PRESENCE", 0, 0, 0 },
    { kVoice,        kCtlSelector2,845, 112, 19, "VOICE",    "LIGHT", "HEAVY", 0 },
    { kValveSound,   kCtlSelector2,932, 112, 19, "VALVE SOUND", "OFF", "ON", 0 },
    { kCh2Reverb,    kCtlRocker,   573, 205, 0,  "REVERB",   "OFF", "ON", 0 },
    // Master
    { kMasterReverb, kCtlKnob,    1050, 112, 22, "REVERB",   0, 0, 0 },
    { kMasterVolume, kCtlKnob,    1140, 112, 22, "VOLUME",   0, 0, 0 },
    // Plugin strip
    { kChannel,      kCtlSlide3,   170, 296, 0,  "CHANNEL",  "1", "2", "1+2" },
    { kSpeaker,      kCtlRocker,   330, 296, 0,  "SPEAKER",  "OFF", "ON", 0 },
    { kInput,        kCtlSmallKnob,450, 296, 15, "INPUT",    "-12", "+12", 0 },
    { kOutput,       kCtlSmallKnob,555, 296, 15, "OUTPUT",   "-24", "+24", 0 },
};

// Preset section in the strip: a SAVE button and slots 1..8, then the readout
const NSRect kSaveRect = { { 608.0, 284.0 }, { 54.0, 24.0 } };
const CGFloat kSlotX0 = 672.0, kSlotPitch = 27.0, kSlotW = 24.0, kSlotY = 284.0, kSlotH = 24.0;
const NSRect kReadoutRect = { { 905.0, 276.0 }, { 267.0, 40.0 } };

inline NSRect slotRect (int k) { return NSMakeRect (kSlotX0 + k * kSlotPitch, kSlotY, kSlotW, kSlotH); }
const int kNumCtls = (int) (sizeof (kCtls) / sizeof (kCtls[0]));

// Pointer angle (radians, 0 = up, clockwise positive) for a 0..1 value.
inline CGFloat valueAngle (CGFloat v) { return (CGFloat) ((-150.0 + 300.0 * v) * M_PI / 180.0); }

// Point at angle a (0 = up, clockwise) and radius r around (cx, cy) in flipped coordinates.
inline NSPoint polar (CGFloat cx, CGFloat cy, CGFloat r, CGFloat a)
{
    return NSMakePoint (cx + r * std::sin (a), cy - r * std::cos (a));
}

NSColor* rgb (int r, int g, int b) { return [NSColor colorWithCalibratedRed: r / 255.0 green: g / 255.0 blue: b / 255.0 alpha: 1.0]; }

NSFont* boldFont (CGFloat size)
{
    NSFont* f = [NSFont fontWithName: @"Helvetica-Bold" size: size];
    return f ? f : [NSFont boldSystemFontOfSize: size];
}

NSFont* plainFont (CGFloat size)
{
    NSFont* f = [NSFont fontWithName: @"Helvetica" size: size];
    return f ? f : [NSFont systemFontOfSize: size];
}

// First installed font from a list of PostScript names, else the fallback.
NSFont* firstFont (const char* const* names, int count, CGFloat size, NSFont* fallback)
{
    for (int i = 0; i < count; ++i)
    {
        NSFont* f = [NSFont fontWithName: [NSString stringWithUTF8String: names[i]] size: size];
        if (f) return f;
    }
    return fallback;
}

// Eurostile for the TB mark when it is installed (it cannot be bundled), else Helvetica.
NSFont* markBoldFont (CGFloat size)
{
    const char* const names[] = { "Eurostile-Bold", "EurostileBold", "Eurostile-Heavy", "EurostileLTStd-Bold", "EurostileT-Bold" };
    return firstFont (names, 5, size, boldFont (size));
}

NSFont* markPlainFont (CGFloat size)
{
    const char* const names[] = { "Eurostile", "Eurostile-Regular", "EurostileLTStd", "EurostileT-Regular" };
    return firstFont (names, 4, size, plainFont (size));
}

void drawText (const char* s, NSFont* font, NSColor* color, CGFloat cx, CGFloat cy)
{
    NSString* str = [NSString stringWithUTF8String: s];
    NSDictionary* attrs = [NSDictionary dictionaryWithObjectsAndKeys:
                             font, NSFontAttributeName, color, NSForegroundColorAttributeName, nil];
    const NSSize sz = [str sizeWithAttributes: attrs];
    [str drawAtPoint: NSMakePoint (cx - sz.width * 0.5, cy - sz.height * 0.5) withAttributes: attrs];
}

void drawTextLeft (const char* s, NSFont* font, NSColor* color, CGFloat x, CGFloat cy)
{
    NSString* str = [NSString stringWithUTF8String: s];
    NSDictionary* attrs = [NSDictionary dictionaryWithObjectsAndKeys:
                             font, NSFontAttributeName, color, NSForegroundColorAttributeName, nil];
    const NSSize sz = [str sizeWithAttributes: attrs];
    [str drawAtPoint: NSMakePoint (x, cy - sz.height * 0.5) withAttributes: attrs];
}

} // namespace

//==============================================================================
@interface TBVSMusicianPanelView : NSView
{
    EditorHost* host;
    float shown[kNumParams];
    NSTimer* timer;
    int dragCtl;
    CGFloat dragStartY, dragStartValue;
    int readoutParam;
    bool saveMode;
    int activePreset;
    bool presetUsed[kNumPresetSlots];
    char status[64];
}
- (id) initWithFrame: (NSRect) frame host: (EditorHost*) h;
- (void) shutdown;
@end

@implementation TBVSMusicianPanelView

- (id) initWithFrame: (NSRect) frame host: (EditorHost*) h
{
    self = [super initWithFrame: frame];
    if (self)
    {
        host = h;
        for (int i = 0; i < kNumParams; ++i) shown[i] = host->getParam (i);
        dragCtl = -1;
        readoutParam = -1;
        saveMode = false;
        activePreset = -1;
        status[0] = 0;
        host->refreshPresets();
        for (int k = 0; k < kNumPresetSlots; ++k) presetUsed[k] = host->presetExists (k);
        timer = [[NSTimer timerWithTimeInterval: 1.0 / 20.0 target: self selector: @selector (tick:)
                                       userInfo: nil repeats: YES] retain];
        [[NSRunLoop currentRunLoop] addTimer: timer forMode: NSDefaultRunLoopMode];
        [[NSRunLoop currentRunLoop] addTimer: timer forMode: NSEventTrackingRunLoopMode];
    }
    return self;
}

- (void) shutdown
{
    if (timer) { [timer invalidate]; [timer release]; timer = nil; }
    host = 0;
}

- (void) dealloc
{
    [self shutdown];
    [super dealloc];
}

- (BOOL) isFlipped { return YES; }
- (BOOL) isOpaque { return YES; }
- (BOOL) acceptsFirstMouse: (NSEvent*) e { (void) e; return YES; }

// Host-side changes (automation, presets) redraw the panel.
- (void) tick: (NSTimer*) t
{
    (void) t;
    if (! host) return;
    bool changed = false;
    for (int i = 0; i < kNumParams; ++i)
    {
        const float v = host->getParam (i);
        if (v != shown[i]) { shown[i] = v; changed = true; }
    }
    if (changed) [self setNeedsDisplay: YES];
}

//------------------------------------------------------------------------------
// Drawing

- (void) drawBackground
{
    const NSRect b = [self bounds];

    // Opaque silver faceplate
    NSGradient* face = [[[NSGradient alloc] initWithStartingColor: rgb (222, 224, 227)
                                                      endingColor: rgb (184, 188, 192)] autorelease];
    [face drawInRect: NSMakeRect (0, 0, b.size.width, kStripTop) angle: 90.0];

    // Plugin strip, a slightly darker silver
    NSGradient* strip = [[[NSGradient alloc] initWithStartingColor: rgb (196, 199, 203)
                                                       endingColor: rgb (166, 170, 175)] autorelease];
    [strip drawInRect: NSMakeRect (0, kStripTop, b.size.width, b.size.height - kStripTop) angle: 90.0];

    [rgb (40, 40, 42) set];
    NSRectFill (NSMakeRect (0, kStripTop, b.size.width, 2.0));
    [rgb (238, 240, 242) set];
    NSRectFill (NSMakeRect (0, kStripTop + 2.0, b.size.width, 1.0));

    // Fine border around the faceplate
    NSBezierPath* border = [NSBezierPath bezierPathWithRect: NSInsetRect (NSMakeRect (0, 0, b.size.width, kStripTop), 6.5, 6.5)];
    [rgb (70, 72, 75) set];
    [border setLineWidth: 1.0];
    [border stroke];

    // Section dividers and titles
    NSColor* ink = [NSColor blackColor];
    const CGFloat dividers[] = { 335.0, 985.0 };
    for (int i = 0; i < 2; ++i)
    {
        NSBezierPath* p = [NSBezierPath bezierPath];
        [p moveToPoint: NSMakePoint (dividers[i] + 0.5, 22)];
        [p lineToPoint: NSMakePoint (dividers[i] + 0.5, 232)];
        [rgb (60, 62, 65) set];
        [p setLineWidth: 1.0];
        [p stroke];
    }
    drawText ("CHANNEL 1", boldFont (12.0), ink, 170, 22);
    drawText ("CHANNEL 2", boldFont (12.0), ink, 660, 22);
    drawText ("MASTER", boldFont (12.0), ink, 1095, 22);

    // Master section nameplate
    drawText ("REVERB AMPLIFIER", boldFont (10.0), ink, 1095, 198);
    drawText ("100 WATTS", plainFont (10.0), ink, 1095, 213);

    // Owner's mark in the strip's corner: red TB over "Electronic", straight on the silver
    {
        const NSRect plate = NSMakeRect (14.5, 262.5, 100.0, 56.0);
        drawText ("TB", markBoldFont (28.0), rgb (200, 30, 30), NSMidX (plate), plate.origin.y + 21.0);
        drawText ("Electronic", markPlainFont (12.0), ink, NSMidX (plate), plate.origin.y + 44.0);
    }
}

- (void) drawScaleForCtl: (const Ctl&) c
{
    NSColor* ink = [NSColor blackColor];
    const bool tone = (c.type == kCtlTone);
    const int majors = 11;
    for (int i = 0; i < majors; ++i)
    {
        const CGFloat a = valueAngle (i / 10.0);
        NSBezierPath* t = [NSBezierPath bezierPath];
        [t moveToPoint: polar (c.cx, c.cy, c.r + 4.0, a)];
        [t lineToPoint: polar (c.cx, c.cy, c.r + 9.0, a)];
        [t setLineWidth: 1.2];
        [ink set];
        [t stroke];

        char num[4];
        const int n = tone ? (i < 5 ? 5 - i : i - 5) : i;
        std::snprintf (num, sizeof (num), "%d", n);
        const NSPoint p = polar (c.cx, c.cy, c.r + 15.0, a);
        drawText (num, plainFont (8.5), ink, p.x, p.y);
    }
    if (tone)
    {
        drawText ("-", boldFont (11.0), ink, c.cx - 8.0, c.cy - c.r - 33.0);
        drawText ("+", boldFont (10.0), ink, c.cx + 8.0, c.cy - c.r - 33.0);
    }
}

// Grey knurled knob with a red position line.
- (void) drawKnobAt: (NSPoint) ctr radius: (CGFloat) r value: (CGFloat) v
{
    // Drop shadow
    [NSGraphicsContext saveGraphicsState];
    NSShadow* sh = [[[NSShadow alloc] init] autorelease];
    [sh setShadowOffset: NSMakeSize (1.5, -2.5)];
    [sh setShadowBlurRadius: 4.0];
    [sh setShadowColor: [NSColor colorWithCalibratedWhite: 0.0 alpha: 0.45]];
    [sh set];
    [rgb (90, 93, 97) set];
    [[NSBezierPath bezierPathWithOvalInRect: NSMakeRect (ctr.x - r, ctr.y - r, 2 * r, 2 * r)] fill];
    [NSGraphicsContext restoreGraphicsState];

    // Knurled skirt: alternating ridges around the rim
    const int ridges = 40;
    for (int k = 0; k < ridges; ++k)
    {
        const CGFloat a0 = (CGFloat) (2.0 * M_PI * k / ridges);
        const CGFloat a1 = (CGFloat) (2.0 * M_PI * (k + 0.5) / ridges);
        NSBezierPath* w = [NSBezierPath bezierPath];
        [w moveToPoint: ctr];
        [w lineToPoint: polar (ctr.x, ctr.y, r, a0)];
        [w lineToPoint: polar (ctr.x, ctr.y, r, a1)];
        [w closePath];
        // light from the top left
        const CGFloat lit = 0.5 + 0.5 * std::cos (a0 + 0.8);
        [[NSColor colorWithCalibratedWhite: 0.40 + 0.22 * lit alpha: 1.0] set];
        [w fill];
    }

    // Top cap
    const CGFloat rc = r * 0.78;
    NSGradient* cap = [[[NSGradient alloc] initWithStartingColor: rgb (172, 175, 179)
                                                     endingColor: rgb (104, 107, 111)] autorelease];
    NSBezierPath* capPath = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (ctr.x - rc, ctr.y - rc, 2 * rc, 2 * rc)];
    [cap drawInBezierPath: capPath angle: -60.0];
    [rgb (70, 72, 75) set];
    [capPath setLineWidth: 0.8];
    [capPath stroke];

    // Red position line
    const CGFloat a = valueAngle (v);
    NSBezierPath* line = [NSBezierPath bezierPath];
    [line moveToPoint: polar (ctr.x, ctr.y, r * 0.18, a)];
    [line lineToPoint: polar (ctr.x, ctr.y, r * 0.98, a)];
    [line setLineWidth: r > 18.0 ? 3.0 : 2.4];
    [line setLineCapStyle: NSRoundLineCapStyle];
    [rgb (200, 30, 30) set];
    [line stroke];
}

// Two-position rotary selector: same knob, two marked positions.
- (void) drawSelector2: (const Ctl&) c
{
    NSColor* ink = [NSColor blackColor];
    const bool on = isOn (shown[c.param]);
    const CGFloat aA = (CGFloat) (-45.0 * M_PI / 180.0), aB = (CGFloat) (45.0 * M_PI / 180.0);
    for (int i = 0; i < 2; ++i)
    {
        const CGFloat a = i ? aB : aA;
        NSBezierPath* t = [NSBezierPath bezierPath];
        [t moveToPoint: polar (c.cx, c.cy, c.r + 4.0, a)];
        [t lineToPoint: polar (c.cx, c.cy, c.r + 10.0, a)];
        [t setLineWidth: 1.4];
        [ink set];
        [t stroke];
    }
    drawText (c.posA, boldFont (9.0), ink, c.cx - c.r - 6.0, c.cy - c.r - 18.0);
    drawText (c.posB, boldFont (9.0), ink, c.cx + c.r + 6.0, c.cy - c.r - 18.0);
    // Map the knob's pointer onto the two positions
    const CGFloat v = on ? (CGFloat) ((45.0 + 150.0) / 300.0) : (CGFloat) ((-45.0 + 150.0) / 300.0);
    [self drawKnobAt: NSMakePoint (c.cx, c.cy) radius: c.r value: v];
}

// Rocker switch: the pressed-in end is the active position.
- (void) drawRocker: (const Ctl&) c
{
    NSColor* ink = [NSColor blackColor];
    const bool on = isOn (shown[c.param]);
    const CGFloat w = 22.0, h = 36.0;
    const NSRect bezel = NSMakeRect (c.cx - w * 0.5 - 4, c.cy - h * 0.5 - 4, w + 8, h + 8);

    NSBezierPath* bz = [NSBezierPath bezierPathWithRoundedRect: bezel xRadius: 4 yRadius: 4];
    [rgb (35, 36, 38) set];
    [bz fill];

    const NSRect top = NSMakeRect (c.cx - w * 0.5, c.cy - h * 0.5, w, h * 0.5);
    const NSRect bot = NSMakeRect (c.cx - w * 0.5, c.cy, w, h * 0.5);
    // The raised half is lighter; ON = top half pressed in
    NSGradient* raised = [[[NSGradient alloc] initWithStartingColor: rgb (120, 122, 126) endingColor: rgb (66, 68, 71)] autorelease];
    NSGradient* pressed = [[[NSGradient alloc] initWithStartingColor: rgb (40, 41, 43) endingColor: rgb (58, 60, 63)] autorelease];
    [(on ? pressed : raised) drawInRect: top angle: on ? 90.0 : -90.0];
    [(on ? raised : pressed) drawInRect: bot angle: on ? 90.0 : -90.0];

    // Markings on the rocker: a bar for ON, a ring for OFF
    [[NSColor colorWithCalibratedWhite: 0.92 alpha: 1.0] set];
    NSRectFill (NSMakeRect (c.cx - 1.0, c.cy - h * 0.5 + 5, 2.0, 8.0));
    NSBezierPath* ring = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (c.cx - 4, c.cy + h * 0.5 - 13, 8, 8)];
    [ring setLineWidth: 1.6];
    [ring stroke];

    drawText (c.label, boldFont (10.0), ink, c.cx, c.cy - h * 0.5 - 14.0);
    drawTextLeft (c.posB, boldFont (9.0), ink, c.cx + w * 0.5 + 8.0, c.cy - h * 0.25);
    drawTextLeft (c.posA, boldFont (9.0), ink, c.cx + w * 0.5 + 8.0, c.cy + h * 0.25);
}

// Three-position slide switch.
- (void) drawSlide3: (const Ctl&) c
{
    NSColor* ink = [NSColor blackColor];
    const int slot = channelSlot (shown[c.param]);
    const CGFloat len = 84.0, h = 16.0;
    const NSRect slot_r = NSMakeRect (c.cx - len * 0.5, c.cy - h * 0.5, len, h);
    NSBezierPath* track = [NSBezierPath bezierPathWithRoundedRect: slot_r xRadius: 3 yRadius: 3];
    [rgb (35, 36, 38) set];
    [track fill];

    const CGFloat step = (len - 26.0) / 2.0;
    const CGFloat kx = c.cx - len * 0.5 + 13.0 + step * slot;
    NSRect knob = NSMakeRect (kx - 12.0, c.cy - h * 0.5 - 3.0, 24.0, h + 6.0);
    NSGradient* g = [[[NSGradient alloc] initWithStartingColor: rgb (210, 212, 215) endingColor: rgb (130, 133, 137)] autorelease];
    NSBezierPath* kp = [NSBezierPath bezierPathWithRoundedRect: knob xRadius: 3 yRadius: 3];
    [g drawInBezierPath: kp angle: -90.0];
    [rgb (60, 62, 65) set];
    [kp setLineWidth: 0.8];
    [kp stroke];
    for (int i = -1; i <= 1; ++i)
    {
        [rgb (90, 92, 96) set];
        NSRectFill (NSMakeRect (kx + i * 5.0 - 0.5, c.cy - 5.0, 1.0, 10.0));
    }

    const char* labels[3] = { c.posA, c.posB, c.posC };
    for (int i = 0; i < 3; ++i)
        drawText (labels[i], boldFont (9.5), ink, c.cx - len * 0.5 + 13.0 + step * i, c.cy + h * 0.5 + 12.0);
    drawText (c.label, boldFont (10.0), ink, c.cx, c.cy - h * 0.5 - 14.0);
}

- (void) drawReadout
{
    const NSRect box = kReadoutRect;
    NSBezierPath* p = [NSBezierPath bezierPathWithRoundedRect: box xRadius: 4 yRadius: 4];
    // Fluorescent lime green, like the original unit's E.L. panel, glowing brighter at the centre
    NSGradient* glow = [[[NSGradient alloc] initWithStartingColor: rgb (206, 255, 120)
                                                      endingColor: rgb (150, 232, 70)] autorelease];
    [glow drawInBezierPath: p relativeCenterPosition: NSMakePoint (0.0, 0.0)];
    [rgb (60, 62, 65) set];
    [p setLineWidth: 1.0];
    [p stroke];

    if (status[0])
    {
        drawText (status, boldFont (14.0), [NSColor blackColor], NSMidX (box), NSMidY (box));
    }
    else if (readoutParam >= 0 && host)
    {
        char disp[16] = { 0 };
        host->getParamDisplay (readoutParam, disp);
        char text[96];
        const char* name = 0;
        for (int i = 0; i < kNumCtls; ++i)
            if (kCtls[i].param == readoutParam) { name = kCtls[i].label; break; }
        const char* prefix = "";
        if (readoutParam <= kCh1Reverb) prefix = "CH 1  ";
        else if (readoutParam <= kCh2Reverb) prefix = "CH 2  ";
        else if (readoutParam == kMasterReverb || readoutParam == kMasterVolume) prefix = "MASTER  ";
        std::snprintf (text, sizeof (text), "%s%s   %s", prefix, name ? name : "", disp);
        drawText (text, boldFont (14.0), [NSColor blackColor], NSMidX (box), NSMidY (box));
    }
}

- (void) drawPresets
{
    NSColor* ink = [NSColor blackColor];
    drawText ("PRESETS", boldFont (10.0), ink, (NSMinX (kSaveRect) + NSMaxX (slotRect (kNumPresetSlots - 1))) * 0.5, 266);

    // SAVE button: lit red while it is waiting for a slot
    NSBezierPath* sp = [NSBezierPath bezierPathWithRoundedRect: kSaveRect xRadius: 3 yRadius: 3];
    if (saveMode)
    {
        [rgb (200, 30, 30) set];
        [sp fill];
    }
    else
    {
        NSGradient* g = [[[NSGradient alloc] initWithStartingColor: rgb (232, 234, 236) endingColor: rgb (160, 163, 167)] autorelease];
        [g drawInBezierPath: sp angle: -90.0];
    }
    [rgb (40, 41, 43) set];
    [sp setLineWidth: 1.0];
    [sp stroke];
    drawText ("SAVE", boldFont (11.0), saveMode ? [NSColor whiteColor] : ink, NSMidX (kSaveRect), NSMidY (kSaveRect));

    // Slots: empty = silver, saved = charcoal, in use = lime green
    for (int k = 0; k < kNumPresetSlots; ++k)
    {
        const NSRect r = slotRect (k);
        NSBezierPath* bp = [NSBezierPath bezierPathWithRoundedRect: r xRadius: 3 yRadius: 3];
        NSColor* fill = (k == activePreset) ? rgb (178, 245, 96) : (presetUsed[k] ? rgb (46, 47, 50) : rgb (214, 216, 219));
        NSColor* text = (k == activePreset) ? ink : (presetUsed[k] ? [NSColor whiteColor] : rgb (120, 122, 126));
        [fill set];
        [bp fill];
        [rgb (40, 41, 43) set];
        [bp setLineWidth: 1.0];
        [bp stroke];
        char num[4];
        std::snprintf (num, sizeof (num), "%d", k + 1);
        drawText (num, boldFont (12.0), text, NSMidX (r), NSMidY (r));
    }
}

- (void) drawRect: (NSRect) dirty
{
    (void) dirty;
    [self drawBackground];
    NSColor* ink = [NSColor blackColor];

    for (int i = 0; i < kNumCtls; ++i)
    {
        const Ctl& c = kCtls[i];
        switch (c.type)
        {
            case kCtlKnob:
            case kCtlTone:
                [self drawScaleForCtl: c];
                [self drawKnobAt: NSMakePoint (c.cx, c.cy) radius: c.r value: shown[c.param]];
                drawText (c.label, boldFont (11.0), ink, c.cx, c.cy - c.r - (c.type == kCtlTone ? 46.0 : 34.0));
                break;
            case kCtlSelector2:
                [self drawSelector2: c];
                drawText (c.label, boldFont (11.0), ink, c.cx, c.cy - c.r - 34.0);
                break;
            case kCtlRocker:
                [self drawRocker: c];
                break;
            case kCtlSlide3:
                [self drawSlide3: c];
                break;
            case kCtlSmallKnob:
            {
                [self drawKnobAt: NSMakePoint (c.cx, c.cy) radius: c.r value: shown[c.param]];
                drawText (c.label, boldFont (10.0), ink, c.cx, c.cy - c.r - 14.0);
                drawText (c.posA, plainFont (8.5), ink, c.cx - c.r - 16.0, c.cy + c.r - 2.0);
                drawText (c.posB, plainFont (8.5), ink, c.cx + c.r + 16.0, c.cy + c.r - 2.0);
                break;
            }
            default: break;
        }
    }
    [self drawPresets];
    [self drawReadout];
}

//------------------------------------------------------------------------------
// Mouse

- (int) ctlAtPoint: (NSPoint) p
{
    for (int i = 0; i < kNumCtls; ++i)
    {
        const Ctl& c = kCtls[i];
        if (c.type == kCtlRocker)
        {
            if (std::fabs (p.x - c.cx) < 20 && std::fabs (p.y - c.cy) < 26) return i;
        }
        else if (c.type == kCtlSlide3)
        {
            if (std::fabs (p.x - c.cx) < 48 && std::fabs (p.y - c.cy) < 22) return i;
        }
        else
        {
            const CGFloat dx = p.x - c.cx, dy = p.y - c.cy;
            if (dx * dx + dy * dy < (c.r + 8) * (c.r + 8)) return i;
        }
    }
    return -1;
}

- (void) setParam: (int) param value: (float) v
{
    if (! host) return;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    host->guiSetParam (param, v);
    shown[param] = host->getParam (param);
    readoutParam = param;
    [self setNeedsDisplay: YES];
}

// SAVE button and preset slots. Returns YES if the click was handled here.
- (BOOL) presetClick: (NSPoint) p
{
    if (NSPointInRect (p, kSaveRect))
    {
        saveMode = ! saveMode;
        std::snprintf (status, sizeof (status), "%s", saveMode ? "SAVE: CLICK A SLOT 1-8" : "");
        [self setNeedsDisplay: YES];
        return YES;
    }
    for (int k = 0; k < kNumPresetSlots; ++k)
    {
        if (! NSPointInRect (p, slotRect (k))) continue;
        if (saveMode)
        {
            const bool ok = host->savePreset (k);
            saveMode = false;
            host->refreshPresets();
            for (int j = 0; j < kNumPresetSlots; ++j) presetUsed[j] = host->presetExists (j);
            if (ok) activePreset = k;
            std::snprintf (status, sizeof (status), ok ? "PRESET %d SAVED" : "COULD NOT SAVE PRESET %d", k + 1);
        }
        else
        {
            host->refreshPresets();
            for (int j = 0; j < kNumPresetSlots; ++j) presetUsed[j] = host->presetExists (j);
            if (presetUsed[k] && host->loadPreset (k))
            {
                for (int j = 0; j < kNumParams; ++j) shown[j] = host->getParam (j);
                activePreset = k;
                std::snprintf (status, sizeof (status), "PRESET %d LOADED", k + 1);
            }
            else
                std::snprintf (status, sizeof (status), "PRESET %d IS EMPTY", k + 1);
        }
        [self setNeedsDisplay: YES];
        return YES;
    }
    return NO;
}

- (void) mouseDown: (NSEvent*) e
{
    const NSPoint p = [self convertPoint: [e locationInWindow] fromView: nil];
    if (! host) return;
    if ([self presetClick: p]) return;
    const int i = [self ctlAtPoint: p];
    if (i < 0) return;
    const Ctl& c = kCtls[i];
    readoutParam = c.param;
    status[0] = 0;
    activePreset = -1;     // settings are being changed away from the preset

    switch (c.type)
    {
        case kCtlRocker:
        {
            // Press the top half for ON, the bottom half for OFF
            host->guiBeginEdit (c.param);
            [self setParam: c.param value: (p.y < c.cy) ? 1.0f : 0.0f];
            host->guiEndEdit (c.param);
            return;
        }
        case kCtlSlide3:
        {
            const CGFloat len = 84.0, step = (len - 26.0) / 2.0;
            int slot = (int) std::floor ((p.x - (c.cx - len * 0.5 + 13.0)) / step + 0.5);
            if (slot < 0) slot = 0;
            if (slot > 2) slot = 2;
            host->guiBeginEdit (c.param);
            [self setParam: c.param value: channelSlotValue (slot)];
            dragCtl = i;
            return;
        }
        case kCtlSelector2:
        {
            host->guiBeginEdit (c.param);
            [self setParam: c.param value: isOn (shown[c.param]) ? 0.0f : 1.0f];
            host->guiEndEdit (c.param);
            return;
        }
        default:
            break;
    }

    host->guiBeginEdit (c.param);
    if ([e clickCount] == 2)
        [self setParam: c.param value: kDefaults[c.param]];   // double-click: back to default
    dragCtl = i;
    dragStartY = p.y;
    dragStartValue = shown[c.param];
    [self setNeedsDisplay: YES];
}

- (void) mouseDragged: (NSEvent*) e
{
    if (dragCtl < 0 || ! host) return;
    const Ctl& c = kCtls[dragCtl];
    const NSPoint p = [self convertPoint: [e locationInWindow] fromView: nil];
    if (c.type == kCtlSlide3)
    {
        const CGFloat len = 84.0, step = (len - 26.0) / 2.0;
        int slot = (int) std::floor ((p.x - (c.cx - len * 0.5 + 13.0)) / step + 0.5);
        if (slot < 0) slot = 0;
        if (slot > 2) slot = 2;
        if (channelSlot (shown[c.param]) != slot)
            [self setParam: c.param value: channelSlotValue (slot)];
        return;
    }
    const bool fine = ([e modifierFlags] & NSShiftKeyMask) != 0;
    const CGFloat perPixel = fine ? 1.0 / 1000.0 : 1.0 / 200.0;
    const CGFloat v = dragStartValue + (dragStartY - p.y) * perPixel;
    [self setParam: c.param value: (float) v];
}

- (void) mouseUp: (NSEvent*) e
{
    (void) e;
    if (dragCtl >= 0 && host) host->guiEndEdit (kCtls[dragCtl].param);
    dragCtl = -1;
}

- (void) scrollWheel: (NSEvent*) e
{
    const NSPoint p = [self convertPoint: [e locationInWindow] fromView: nil];
    const int i = [self ctlAtPoint: p];
    if (i < 0 || ! host) return;
    const Ctl& c = kCtls[i];
    if (c.type != kCtlKnob && c.type != kCtlTone && c.type != kCtlSmallKnob) return;
    const CGFloat d = [e deltaY];
    if (d == 0.0) return;
    status[0] = 0;
    activePreset = -1;
    host->guiBeginEdit (c.param);
    [self setParam: c.param value: (float) (shown[c.param] + d * 0.01)];
    host->guiEndEdit (c.param);
}

@end

//==============================================================================
namespace tbvs
{

// Both calls use their own autorelease pool, so nothing of this plugin's is
// left waiting in the host's pool when the editor closes or the plugin unloads.
void* editorOpen (void* parentView, EditorHost* host)
{
    NSView* parent = (NSView*) parentView;
    if (! parent || ! host) return 0;
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    TBVSMusicianPanelView* v = [[TBVSMusicianPanelView alloc]
                                  initWithFrame: NSMakeRect (0, 0, kEditorWidth, kEditorHeight) host: host];
    if (v) [parent addSubview: v];
    [pool release];
    return (void*) v;    // retained until editorClose
}

void editorClose (void* handle)
{
    TBVSMusicianPanelView* v = (TBVSMusicianPanelView*) handle;
    if (! v) return;
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [v shutdown];
    [v removeFromSuperview];
    [v release];
    [pool release];
}

} // namespace tbvs
