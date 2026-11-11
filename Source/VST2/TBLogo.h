// TBLogo.h
// The user's own TB electronic logo (TB_HH_ELECTRONIC_LOGO.pdf, Adobe Illustrator),
// drawn as vector paths in its own colours. The white shapes in the artwork (the
// holes in the B) are cut out, so the panel shows through; nothing white is drawn.
// Generated from the PDF; works on Mac OS X 10.8 and current macOS.
//
// drawTBLogo (box): draws the logo as large as fits in box, keeping its shape,
// centred. The view must be flipped (y down), as the TB panels are.

#ifndef TB_LOGO_H
#define TB_LOGO_H

#import <Cocoa/Cocoa.h>

namespace tblogo
{

// Artwork bounds (points, y down)
const CGFloat kX0 = 66.473, kY0 = 16.773, kW = 777.137, kH = 488.559;

inline NSPoint P (CGFloat x, CGFloat y) { return NSMakePoint (x, y); }

inline void drawTBLogo (NSRect box)
{
    const CGFloat s = (box.size.width / kW < box.size.height / kH) ? box.size.width / kW : box.size.height / kH;
    const CGFloat ox = box.origin.x + (box.size.width - kW * s) * 0.5;
    const CGFloat oy = box.origin.y + (box.size.height - kH * s) * 0.5;

    [NSGraphicsContext saveGraphicsState];
    NSAffineTransform* t = [NSAffineTransform transform];
    [t translateXBy: ox yBy: oy];
    [t scaleBy: s];
    [t translateXBy: -kX0 yBy: -kY0];
    [t concat];

    // Red: T, the two bars, and the B with its two holes cut out (even-odd)
    NSBezierPath* red = [NSBezierPath bezierPath];
    [red moveToPoint: P (250.379, 360.062)];
    [red lineToPoint: P (173.891, 359.992)];
    [red lineToPoint: P (173.875, 147.957)];
    [red curveToPoint: P (171.703, 144.344) controlPoint1: P (173.664, 146.133) controlPoint2: P (173.316, 145.168)];
    [red lineToPoint: P (66.473, 144.398)];
    [red lineToPoint: P (66.504, 78.133)];
    [red lineToPoint: P (357.590, 78.152)];
    [red lineToPoint: P (357.633, 144.383)];
    [red lineToPoint: P (253.719, 144.371)];
    [red curveToPoint: P (250.371, 147.375) controlPoint1: P (251.625, 144.402) controlPoint2: P (250.652, 145.348)];
    [red closePath];
    [red moveToPoint: P (406.254, 16.773)];
    [red lineToPoint: P (430.812, 16.781)];
    [red lineToPoint: P (430.789, 431.816)];
    [red lineToPoint: P (406.227, 431.809)];
    [red closePath];
    [red moveToPoint: P (482.234, 16.773)];
    [red lineToPoint: P (506.793, 16.781)];
    [red lineToPoint: P (506.770, 431.816)];
    [red lineToPoint: P (482.207, 431.809)];
    [red closePath];
    [red moveToPoint: P (819.906, 221.844)];
    [red lineToPoint: P (828.613, 228.543)];
    [red lineToPoint: P (838.562, 236.559)];
    [red curveToPoint: P (843.566, 280.586) controlPoint1: P (842.199, 251.098) controlPoint2: P (843.516, 265.652)];
    [red lineToPoint: P (843.609, 293.816)];
    [red lineToPoint: P (840.691, 326.746)];
    [red curveToPoint: P (814.410, 357.629) controlPoint1: P (839.414, 341.180) controlPoint2: P (830.211, 354.109)];
    [red curveToPoint: P (793.855, 360.035) controlPoint1: P (807.520, 359.035) controlPoint2: P (801.195, 359.883)];
    [red lineToPoint: P (552.402, 359.996)];
    [red lineToPoint: P (552.410, 78.203)];
    [red lineToPoint: P (812.410, 78.137)];
    [red curveToPoint: P (832.117, 88.492) controlPoint1: P (820.375, 79.648) controlPoint2: P (827.043, 83.090)];
    [red curveToPoint: P (838.875, 104.527) controlPoint1: P (836.555, 93.004) controlPoint2: P (838.648, 98.387)];
    [red lineToPoint: P (839.391, 118.598)];
    [red lineToPoint: P (839.379, 182.238)];
    [red lineToPoint: P (838.574, 200.270)];
    [red lineToPoint: P (826.527, 211.004)];
    [red lineToPoint: P (820.133, 216.266)];
    [red curveToPoint: P (818.270, 218.816) controlPoint1: P (819.141, 217.082) controlPoint2: P (818.332, 217.746)];
    [red curveToPoint: P (819.906, 221.844) controlPoint1: P (818.195, 220.039) controlPoint2: P (818.660, 220.887)];
    [red moveToPoint: P (762.141, 180.945)];
    [red lineToPoint: P (753.949, 187.051)];
    [red lineToPoint: P (629.441, 187.035)];
    [red lineToPoint: P (629.547, 140.066)];
    [red lineToPoint: P (754.613, 140.043)];
    [red lineToPoint: P (762.078, 145.059)];
    [red closePath];
    [red moveToPoint: P (762.156, 293.566)];
    [red lineToPoint: P (753.797, 299.695)];
    [red lineToPoint: P (629.434, 299.730)];
    [red lineToPoint: P (629.430, 252.648)];
    [red lineToPoint: P (754.465, 252.625)];
    [red lineToPoint: P (762.012, 257.898)];
    [red closePath];
    [red setWindingRule: NSEvenOddWindingRule];
    [[NSColor colorWithCalibratedRed: 1.000000 green: 0.034988 blue: 0.011993 alpha: 1.0] set];
    [red fill];

    // Black: "electronic"
    NSBezierPath* black = [NSBezierPath bezierPath];
    [black moveToPoint: P (130.738, 469.699)];
    [black curveToPoint: P (113.688, 460.410) controlPoint1: P (130.621, 461.941) controlPoint2: P (127.797, 460.410)];
    [black curveToPoint: P (99.344, 463.000) controlPoint1: P (104.984, 460.410) controlPoint2: P (101.457, 461.000)];
    [black curveToPoint: P (96.871, 468.762) controlPoint1: P (97.695, 464.410) controlPoint2: P (96.871, 466.410)];
    [black lineToPoint: P (96.871, 469.699)];
    [black closePath];
    [black moveToPoint: P (150.375, 486.516)];
    [black curveToPoint: P (142.027, 501.688) controlPoint1: P (150.258, 494.629) controlPoint2: P (147.906, 498.980)];
    [black curveToPoint: P (115.453, 504.980) controlPoint1: P (136.852, 504.156) controlPoint2: P (130.270, 504.980)];
    [black curveToPoint: P (92.992, 503.566) controlPoint1: P (106.398, 504.980) controlPoint2: P (97.930, 504.391)];
    [black curveToPoint: P (78.879, 493.219) controlPoint1: P (86.641, 502.391) controlPoint2: P (81.352, 498.395)];
    [black curveToPoint: P (76.293, 477.344) controlPoint1: P (77.000, 489.105) controlPoint2: P (76.293, 484.871)];
    [black curveToPoint: P (88.055, 451.004) controlPoint1: P (76.293, 461.941) controlPoint2: P (79.586, 454.531)];
    [black curveToPoint: P (114.039, 448.184) controlPoint1: P (93.227, 448.887) controlPoint2: P (99.344, 448.184)];
    [black curveToPoint: P (135.793, 449.594) controlPoint1: P (123.566, 448.184) controlPoint2: P (131.324, 448.652)];
    [black curveToPoint: P (149.082, 461.824) controlPoint1: P (142.145, 450.887) controlPoint2: P (147.438, 455.590)];
    [black curveToPoint: P (150.375, 474.523) controlPoint1: P (150.023, 465.352) controlPoint2: P (150.375, 468.406)];
    [black lineToPoint: P (150.375, 480.520)];
    [black lineToPoint: P (96.871, 480.520)];
    [black lineToPoint: P (96.871, 483.105)];
    [black curveToPoint: P (100.754, 491.219) controlPoint1: P (96.871, 487.105) controlPoint2: P (98.285, 489.809)];
    [black curveToPoint: P (115.453, 492.750) controlPoint1: P (102.633, 492.160) controlPoint2: P (107.691, 492.750)];
    [black curveToPoint: P (130.504, 486.516) controlPoint1: P (127.328, 492.750) controlPoint2: P (130.031, 491.574)];
    [black closePath];
    [black moveToPoint: P (165.070, 425.605)];
    [black lineToPoint: P (185.297, 425.605)];
    [black lineToPoint: P (185.297, 504.039)];
    [black lineToPoint: P (165.070, 504.039)];
    [black closePath];
    [black moveToPoint: P (254.672, 469.699)];
    [black curveToPoint: P (237.621, 460.410) controlPoint1: P (254.555, 461.941) controlPoint2: P (251.734, 460.410)];
    [black curveToPoint: P (223.277, 463.000) controlPoint1: P (228.922, 460.410) controlPoint2: P (225.395, 461.000)];
    [black curveToPoint: P (220.805, 468.762) controlPoint1: P (221.629, 464.410) controlPoint2: P (220.805, 466.410)];
    [black lineToPoint: P (220.805, 469.699)];
    [black closePath];
    [black moveToPoint: P (274.309, 486.516)];
    [black curveToPoint: P (265.961, 501.688) controlPoint1: P (274.191, 494.629) controlPoint2: P (271.840, 498.980)];
    [black curveToPoint: P (239.387, 504.980) controlPoint1: P (260.789, 504.156) controlPoint2: P (254.203, 504.980)];
    [black curveToPoint: P (216.926, 503.566) controlPoint1: P (230.332, 504.980) controlPoint2: P (221.863, 504.391)];
    [black curveToPoint: P (202.816, 493.219) controlPoint1: P (210.578, 502.391) controlPoint2: P (205.285, 498.395)];
    [black curveToPoint: P (200.227, 477.344) controlPoint1: P (200.934, 489.105) controlPoint2: P (200.227, 484.871)];
    [black curveToPoint: P (211.988, 451.004) controlPoint1: P (200.227, 461.941) controlPoint2: P (203.520, 454.531)];
    [black curveToPoint: P (237.977, 448.184) controlPoint1: P (217.160, 448.887) controlPoint2: P (223.277, 448.184)];
    [black curveToPoint: P (259.730, 449.594) controlPoint1: P (247.500, 448.184) controlPoint2: P (255.262, 448.652)];
    [black curveToPoint: P (273.016, 461.824) controlPoint1: P (266.078, 450.887) controlPoint2: P (271.371, 455.590)];
    [black curveToPoint: P (274.309, 474.523) controlPoint1: P (273.957, 465.352) controlPoint2: P (274.309, 468.406)];
    [black lineToPoint: P (274.309, 480.520)];
    [black lineToPoint: P (220.805, 480.520)];
    [black lineToPoint: P (220.805, 483.105)];
    [black curveToPoint: P (224.688, 491.219) controlPoint1: P (220.805, 487.105) controlPoint2: P (222.219, 489.809)];
    [black curveToPoint: P (239.387, 492.750) controlPoint1: P (226.570, 492.160) controlPoint2: P (231.625, 492.750)];
    [black curveToPoint: P (254.438, 486.516) controlPoint1: P (251.262, 492.750) controlPoint2: P (253.969, 491.574)];
    [black closePath];
    [black moveToPoint: P (342.156, 470.172)];
    [black curveToPoint: P (324.520, 462.766) controlPoint1: P (341.688, 464.176) controlPoint2: P (338.277, 462.766)];
    [black curveToPoint: P (307.234, 476.520) controlPoint1: P (310.289, 462.766) controlPoint2: P (307.234, 465.234)];
    [black curveToPoint: P (312.289, 488.750) controlPoint1: P (307.234, 483.223) controlPoint2: P (308.762, 486.871)];
    [black curveToPoint: P (325.340, 490.281) controlPoint1: P (314.172, 489.809) controlPoint2: P (318.051, 490.281)];
    [black curveToPoint: P (342.629, 482.637) controlPoint1: P (338.629, 490.281) controlPoint2: P (342.039, 488.750)];
    [black lineToPoint: P (363.324, 482.637)];
    [black curveToPoint: P (326.164, 505.215) controlPoint1: P (363.090, 500.277) controlPoint2: P (355.094, 505.215)];
    [black curveToPoint: P (294.770, 499.570) controlPoint1: P (308.879, 505.215) controlPoint2: P (300.059, 503.684)];
    [black curveToPoint: P (286.891, 476.992) controlPoint1: P (289.359, 495.570) controlPoint2: P (286.891, 488.516)];
    [black curveToPoint: P (295.828, 452.535) controlPoint1: P (286.891, 464.176) controlPoint2: P (289.711, 456.531)];
    [black curveToPoint: P (325.227, 447.832) controlPoint1: P (300.883, 449.242) controlPoint2: P (309.938, 447.832)];
    [black curveToPoint: P (362.734, 470.172) controlPoint1: P (354.738, 447.832) controlPoint2: P (362.500, 452.418)];
    [black closePath];
    [black moveToPoint: P (438.344, 480.520)];
    [black lineToPoint: P (438.344, 484.871)];
    [black curveToPoint: P (434.344, 500.273) controlPoint1: P (438.344, 493.809) controlPoint2: P (437.402, 497.336)];
    [black curveToPoint: P (414.000, 505.332) controlPoint1: P (430.465, 503.918) controlPoint2: P (424.703, 505.332)];
    [black curveToPoint: P (390.246, 500.629) controlPoint1: P (399.773, 505.332) controlPoint2: P (394.715, 504.273)];
    [black curveToPoint: P (384.840, 485.223) controlPoint1: P (386.133, 497.219) controlPoint2: P (384.840, 493.570)];
    [black lineToPoint: P (384.840, 462.883)];
    [black lineToPoint: P (374.371, 462.883)];
    [black lineToPoint: P (374.371, 449.121)];
    [black lineToPoint: P (384.723, 449.121)];
    [black lineToPoint: P (384.723, 436.188)];
    [black lineToPoint: P (405.418, 436.188)];
    [black lineToPoint: P (405.418, 449.121)];
    [black lineToPoint: P (438.344, 449.121)];
    [black lineToPoint: P (438.344, 462.883)];
    [black lineToPoint: P (405.418, 462.883)];
    [black lineToPoint: P (405.418, 480.992)];
    [black curveToPoint: P (414.238, 490.867) controlPoint1: P (405.418, 488.867) controlPoint2: P (407.180, 490.867)];
    [black curveToPoint: P (422.938, 482.754) controlPoint1: P (421.172, 490.867) controlPoint2: P (422.938, 489.223)];
    [black lineToPoint: P (422.938, 480.520)];
    [black closePath];
    [black moveToPoint: P (453.156, 449.121)];
    [black lineToPoint: P (472.559, 449.121)];
    [black lineToPoint: P (472.559, 461.352)];
    [black curveToPoint: P (478.086, 452.062) controlPoint1: P (474.793, 455.473) controlPoint2: P (475.734, 453.945)];
    [black curveToPoint: P (495.137, 447.832) controlPoint1: P (481.492, 449.359) controlPoint2: P (487.727, 447.832)];
    [black curveToPoint: P (513.242, 453.945) controlPoint1: P (503.602, 447.832) controlPoint2: P (509.953, 449.945)];
    [black curveToPoint: P (516.891, 469.820) controlPoint1: P (515.711, 457.004) controlPoint2: P (516.891, 461.824)];
    [black lineToPoint: P (516.891, 473.348)];
    [black lineToPoint: P (497.957, 473.348)];
    [black curveToPoint: P (497.840, 472.523) controlPoint1: P (497.840, 472.996) controlPoint2: P (497.840, 472.641)];
    [black lineToPoint: P (497.840, 471.820)];
    [black curveToPoint: P (487.141, 463.703) controlPoint1: P (497.840, 466.172) controlPoint2: P (494.664, 463.703)];
    [black curveToPoint: P (475.379, 468.523) controlPoint1: P (481.375, 463.703) controlPoint2: P (477.613, 465.234)];
    [black curveToPoint: P (473.379, 478.754) controlPoint1: P (473.852, 470.879) controlPoint2: P (473.379, 473.230)];
    [black lineToPoint: P (473.379, 504.039)];
    [black lineToPoint: P (453.156, 504.039)];
    [black closePath];
    [black moveToPoint: P (547.695, 476.406)];
    [black curveToPoint: P (566.859, 490.398) controlPoint1: P (547.695, 488.867) controlPoint2: P (549.809, 490.398)];
    [black curveToPoint: P (585.324, 476.875) controlPoint1: P (583.207, 490.398) controlPoint2: P (585.324, 488.867)];
    [black curveToPoint: P (566.156, 462.766) controlPoint1: P (585.324, 464.176) controlPoint2: P (583.441, 462.766)];
    [black curveToPoint: P (547.695, 476.406) controlPoint1: P (550.164, 462.766) controlPoint2: P (547.695, 464.645)];
    [black moveToPoint: P (589.910, 449.359)];
    [black curveToPoint: P (603.668, 458.531) controlPoint1: P (596.375, 450.535) controlPoint2: P (601.551, 453.945)];
    [black curveToPoint: P (605.664, 475.109) controlPoint1: P (605.195, 461.941) controlPoint2: P (605.664, 466.055)];
    [black curveToPoint: P (597.785, 500.746) controlPoint1: P (605.664, 492.160) controlPoint2: P (604.254, 496.867)];
    [black curveToPoint: P (583.559, 504.625) controlPoint1: P (594.023, 503.098) controlPoint2: P (590.848, 503.918)];
    [black curveToPoint: P (565.332, 505.332) controlPoint1: P (578.855, 504.980) controlPoint2: P (570.977, 505.332)];
    [black curveToPoint: P (533.816, 499.922) controlPoint1: P (547.105, 505.332) controlPoint2: P (538.992, 503.918)];
    [black curveToPoint: P (527.352, 476.285) controlPoint1: P (528.523, 495.922) controlPoint2: P (527.352, 491.457)];
    [black curveToPoint: P (536.875, 451.359) controlPoint1: P (527.352, 460.293) controlPoint2: P (529.469, 454.770)];
    [black curveToPoint: P (566.273, 447.828) controlPoint1: P (542.402, 448.887) controlPoint2: P (551.457, 447.828)];
    [black curveToPoint: P (589.910, 449.359) controlPoint1: P (575.328, 447.828) controlPoint2: P (584.266, 448.418)];
    [black moveToPoint: P (620.598, 449.121)];
    [black lineToPoint: P (640.352, 449.121)];
    [black lineToPoint: P (640.352, 461.824)];
    [black curveToPoint: P (665.633, 447.832) controlPoint1: P (643.055, 451.121) controlPoint2: P (649.051, 447.832)];
    [black curveToPoint: P (690.090, 455.590) controlPoint1: P (678.215, 447.832) controlPoint2: P (685.387, 450.062)];
    [black curveToPoint: P (695.148, 474.875) controlPoint1: P (693.738, 459.824) controlPoint2: P (695.148, 465.117)];
    [black lineToPoint: P (695.148, 504.039)];
    [black lineToPoint: P (674.922, 504.039)];
    [black lineToPoint: P (674.922, 475.699)];
    [black curveToPoint: P (658.227, 463.586) controlPoint1: P (674.922, 466.055) controlPoint2: P (671.512, 463.586)];
    [black curveToPoint: P (640.820, 479.227) controlPoint1: P (644.703, 463.586) controlPoint2: P (640.820, 467.113)];
    [black lineToPoint: P (640.820, 504.039)];
    [black lineToPoint: P (620.598, 504.039)];
    [black closePath];
    [black moveToPoint: P (712.430, 449.125)];
    [black lineToPoint: P (732.656, 449.125)];
    [black lineToPoint: P (732.656, 504.039)];
    [black lineToPoint: P (712.430, 504.039)];
    [black closePath];
    [black moveToPoint: P (712.430, 425.605)];
    [black lineToPoint: P (732.656, 425.605)];
    [black lineToPoint: P (732.656, 440.305)];
    [black lineToPoint: P (712.430, 440.305)];
    [black closePath];
    [black moveToPoint: P (802.855, 470.172)];
    [black curveToPoint: P (785.215, 462.766) controlPoint1: P (802.383, 464.176) controlPoint2: P (798.973, 462.766)];
    [black curveToPoint: P (767.930, 476.520) controlPoint1: P (770.984, 462.766) controlPoint2: P (767.930, 465.234)];
    [black curveToPoint: P (772.984, 488.750) controlPoint1: P (767.930, 483.223) controlPoint2: P (769.457, 486.871)];
    [black curveToPoint: P (786.039, 490.281) controlPoint1: P (774.867, 489.809) controlPoint2: P (778.746, 490.281)];
    [black curveToPoint: P (803.324, 482.637) controlPoint1: P (799.324, 490.281) controlPoint2: P (802.734, 488.750)];
    [black lineToPoint: P (824.020, 482.637)];
    [black curveToPoint: P (786.863, 505.215) controlPoint1: P (823.785, 500.277) controlPoint2: P (815.789, 505.215)];
    [black curveToPoint: P (755.465, 499.570) controlPoint1: P (769.574, 505.215) controlPoint2: P (760.758, 503.684)];
    [black curveToPoint: P (747.586, 476.992) controlPoint1: P (750.055, 495.570) controlPoint2: P (747.586, 488.516)];
    [black curveToPoint: P (756.523, 452.535) controlPoint1: P (747.586, 464.176) controlPoint2: P (750.410, 456.531)];
    [black curveToPoint: P (785.922, 447.832) controlPoint1: P (761.578, 449.242) controlPoint2: P (770.633, 447.832)];
    [black curveToPoint: P (823.434, 470.172) controlPoint1: P (815.438, 447.832) controlPoint2: P (823.195, 452.418)];
    [black closePath];
    [[NSColor blackColor] set];
    [black fill];

    [NSGraphicsContext restoreGraphicsState];
}

} // namespace tblogo

#endif
