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

    // One line of printable characters, trimmed, at most kMaxPresetName long
    std::string cleanName (const std::string& in)
    {
        std::string out;
        for (std::string::size_type i = 0; i < in.size(); ++i)
        {
            const unsigned char c = (unsigned char) in[i];
            if (c == '\n' || c == '\r' || c == '\t') out += ' ';
            else if (c >= 32 && c != 127) out += (char) c;      // UTF-8 bytes pass through
        }
        std::string::size_type a = out.find_first_not_of (' ');
        if (a == std::string::npos) return std::string();
        std::string::size_type b = out.find_last_not_of (' ');
        out = out.substr (a, b - a + 1);
        if (out.size() > (std::string::size_type) kMaxPresetName)
        {
            std::string::size_type n = kMaxPresetName;
            while (n > 0 && (((unsigned char) out[n]) & 0xC0) == 0x80) --n;   // don't split a UTF-8 character
            out = out.substr (0, n);
        }
        return out;
    }
}

PresetStore::PresetStore()
    : path (defaultPath()), legacy (legacyPath())
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
    return base + "/Library/Application Support/TB/TB VS Musician/Presets.txt";
#else
    return base + "/.config/TB/TB VS Musician/Presets.txt";
#endif
}

// Where presets were kept when the plugin was called "HH VS Musician (Reverb)".
// They are read from there until the first save writes the new file.
std::string PresetStore::legacyPath()
{
    const char* over = std::getenv ("TBVS_PRESET_FILE");
    if (over && *over) return std::string();
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
    for (int s = 0; s < kNumPresetSlots; ++s) { used[s] = false; names[s].clear(); }

    FILE* f = std::fopen (path.c_str(), "r");
    if (! f && ! legacy.empty()) f = std::fopen (legacy.c_str(), "r");
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
        if (std::sscanf (line, "name %d:%n", &slot, &used0) == 1 && slot >= 1 && slot <= kNumPresetSlots && used0 > 0)
        {
            std::string n (line + used0);
            if (! n.empty() && n[0] == ' ') n.erase (0, 1);
            names[slot - 1] = cleanName (n);
            continue;
        }
        slot = -1; used0 = 0;
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
    std::fprintf (f, "# TB VS Musician presets. One line per slot; values 0..1 in parameter order; then its name.\n");
    for (int s = 0; s < kNumPresetSlots; ++s)
    {
        if (! used[s]) continue;
        std::fprintf (f, "slot %d:", s + 1);
        for (int i = 0; i < kNumParams; ++i) std::fprintf (f, " %.6f", vals[s][i]);
        std::fprintf (f, "\n");
        if (! names[s].empty()) std::fprintf (f, "name %d: %s\n", s + 1, names[s].c_str());
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

std::string PresetStore::name (int slot) const
{
    return has (slot) ? names[slot] : std::string();
}

void PresetStore::setName (int slot, const std::string& n)
{
    if (slot < 0 || slot >= kNumPresetSlots) return;
    names[slot] = cleanName (n);
}

} // namespace tbvs
