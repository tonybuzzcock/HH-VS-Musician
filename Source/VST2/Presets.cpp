// Presets.cpp

#include "Presets.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

namespace tbvs
{

namespace
{
    const char* const kHeader = "TBVS-PRESETS 1";

    // mkdir -p for the folder containing 'file'
    void makeParentDirs (const std::string& file)
    {
        const std::string::size_type last = file.rfind ('/');
        if (last == std::string::npos || last == 0) return;
        const std::string dir = file.substr (0, last);
        for (std::string::size_type i = 1; i <= dir.size(); ++i)
            if (i == dir.size() || dir[i] == '/')
                mkdir (dir.substr (0, i).c_str(), 0755);   // existing folders just fail harmlessly
    }
}

PresetStore::PresetStore()
    : path (defaultPath())
{
    for (int s = 0; s < kNumPresetSlots; ++s)
    {
        used[s] = false;
        for (int i = 0; i < kNumParams; ++i) vals[s][i] = kDefaults[i];
    }
}

std::string PresetStore::defaultPath()
{
    const char* over = std::getenv ("TBVS_PRESET_FILE");
    if (over && *over) return over;
    const char* home = std::getenv ("HOME");
    std::string base = (home && *home) ? home : "/tmp";
#if defined(__APPLE__)
    return base + "/Library/Application Support/TB/HH VS Musician (Reverb)/Presets.txt";
#else
    return base + "/.config/TB/HH VS Musician (Reverb)/Presets.txt";
#endif
}

bool PresetStore::load()
{
    for (int s = 0; s < kNumPresetSlots; ++s) used[s] = false;

    FILE* f = std::fopen (path.c_str(), "r");
    if (! f) return false;

    char line[1024];
    bool headerOk = false;
    while (std::fgets (line, sizeof (line), f))
    {
        if (! headerOk)
        {
            headerOk = std::strncmp (line, kHeader, std::strlen (kHeader)) == 0;
            if (! headerOk) break;
            continue;
        }
        int slot = -1, used0 = 0;
        if (std::sscanf (line, "slot %d:%n", &slot, &used0) != 1 || slot < 1 || slot > kNumPresetSlots)
            continue;

        float v[kNumParams];
        const char* p = line + used0;
        int got = 0;
        for (; got < kNumParams; ++got)
        {
            char* end = 0;
            const double d = std::strtod (p, &end);
            if (end == p) break;
            v[got] = (d == d) ? (float) (d < 0.0 ? 0.0 : (d > 1.0 ? 1.0 : d)) : kDefaults[got];
            p = end;
        }
        if (got == 0) continue;
        for (int i = got; i < kNumParams; ++i) v[i] = kDefaults[i];   // older files: fill the rest
        for (int i = 0; i < kNumParams; ++i) vals[slot - 1][i] = v[i];
        used[slot - 1] = true;
    }
    std::fclose (f);
    return headerOk;
}

bool PresetStore::save() const
{
    makeParentDirs (path);
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen (tmp.c_str(), "w");
    if (! f) return false;
    std::fprintf (f, "%s\n", kHeader);
    std::fprintf (f, "# HH VS Musician (Reverb) presets. One line per slot; values 0..1 in parameter order.\n");
    for (int s = 0; s < kNumPresetSlots; ++s)
    {
        if (! used[s]) continue;
        std::fprintf (f, "slot %d:", s + 1);
        for (int i = 0; i < kNumParams; ++i) std::fprintf (f, " %.6f", vals[s][i]);
        std::fprintf (f, "\n");
    }
    const bool ok = std::fclose (f) == 0;
    if (! ok) { std::remove (tmp.c_str()); return false; }
    return std::rename (tmp.c_str(), path.c_str()) == 0;
}

bool PresetStore::has (int slot) const
{
    return slot >= 0 && slot < kNumPresetSlots && used[slot];
}

bool PresetStore::get (int slot, float values[kNumParams]) const
{
    if (! has (slot)) return false;
    for (int i = 0; i < kNumParams; ++i) values[i] = vals[slot][i];
    return true;
}

void PresetStore::put (int slot, const float values[kNumParams])
{
    if (slot < 0 || slot >= kNumPresetSlots) return;
    for (int i = 0; i < kNumParams; ++i) vals[slot][i] = values[i];
    used[slot] = true;
}

} // namespace tbvs
