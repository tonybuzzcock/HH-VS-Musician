// preset_test.cpp
// Checks the preset file: save, reload in a fresh store (another instance or a
// later session), merging slots, folder creation, and damaged files.

#include "../Source/VST2/Presets.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace tbvs;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

int main()
{
    char tmpl[] = "/tmp/tbvs_presets_XXXXXX";
    const char* dir = mkdtemp (tmpl);
    if (! dir) { std::printf ("mkdtemp failed\n"); return 1; }
    const std::string file = std::string (dir) + "/nested/folders/Presets.txt";

    std::printf ("Presets\n");
    PresetStore a;
    a.setPath (file);
    check (! a.load(), "missing file reads as empty");
    for (int s = 0; s < kNumPresetSlots; ++s) check (! a.has (s), "slot starts empty");

    float v1[kNumParams], v2[kNumParams];
    for (int i = 0; i < kNumParams; ++i) { v1[i] = (float) i / kNumParams; v2[i] = 1.0f - v1[i]; }
    a.put (2, v1);
    check (a.save(), "saves, creating the folders");

    PresetStore b;
    b.setPath (file);
    check (b.load() && b.has (2) && ! b.has (0), "a fresh store sees the saved slot");
    float got[kNumParams];
    bool same = b.get (2, got);
    for (int i = 0; i < kNumParams; ++i) if (std::fabs (got[i] - v1[i]) > 1e-5f) same = false;
    check (same, "every value round-trips");

    // Another instance adds a slot; the first one reloads before saving its own
    b.put (7, v2); b.save();
    a.load(); a.put (0, v2); a.save();
    PresetStore c; c.setPath (file); c.load();
    check (c.has (0) && c.has (2) && c.has (7), "slots from different instances are kept");

    // Damaged lines are skipped, out-of-range values clamped, short lines padded
    FILE* f = std::fopen (file.c_str(), "a");
    std::fprintf (f, "slot 99: 0.5 0.5\nslot 5: rubbish\nslot 4: 7.0 -3.0 0.25\n");
    std::fclose (f);
    PresetStore d; d.setPath (file); d.load();
    float s4[kNumParams];
    const bool okDamaged = d.has (2) && ! d.has (4) && d.get (3, s4)
                        && s4[0] == 1.0f && s4[1] == 0.0f && std::fabs (s4[2] - 0.25f) < 1e-6f
                        && s4[kNumParams - 1] == kDefaults[kNumParams - 1];
    check (okDamaged, "damaged lines ignored, values clamped, missing values defaulted");

    // A file that is not ours is ignored rather than misread
    f = std::fopen (file.c_str(), "w"); std::fprintf (f, "hello\nslot 1: 0.1\n"); std::fclose (f);
    PresetStore e; e.setPath (file);
    check (! e.load() && ! e.has (0), "foreign file ignored");

    std::printf ("  default location: %s\n", PresetStore::defaultPath().c_str());
    std::string cmd = std::string ("rm -rf '") + dir + "'";
    if (std::system (cmd.c_str()) != 0) std::printf ("  (could not remove %s)\n", dir);

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PRESET CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
