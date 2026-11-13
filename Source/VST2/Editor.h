// Editor.h
// Plain C++ interface between the VST wrapper and the Cocoa editor
// (PanelView.mm), so the wrapper itself stays free of Objective-C.
// The editor only exists on the Mac; elsewhere the plugin reports no editor.

#ifndef TBVS_EDITOR_H
#define TBVS_EDITOR_H

namespace tbvs
{

enum { kEditorWidth = 1200, kEditorHeight = 330 };

// What the editor needs from the plugin.
class EditorHost
{
public:
    virtual ~EditorHost() {}
    virtual float getParam (int index) const = 0;
    virtual void  getParamDisplay (int index, char* text8) const = 0;   // up to 8 chars + NUL
    virtual void  guiBeginEdit (int index) = 0;
    virtual void  guiSetParam (int index, float value) = 0;           // also notifies the host
    virtual void  guiEndEdit (int index) = 0;

    // User presets (slots 0..kNumPresetSlots-1), stored in a file shared by every instance
    virtual void  refreshPresets() = 0;                 // re-read the file
    virtual bool  presetExists (int slot) const = 0;
    virtual bool  savePreset (int slot) = 0;            // current settings -> slot
    virtual bool  loadPreset (int slot) = 0;            // slot -> every parameter (host is told)
    virtual void  presetName (int slot, char* text, int size) const = 0;   // "" if none
    virtual bool  renamePreset (int slot, const char* name) = 0;           // saved to the file
};

#if defined(__APPLE__)
// parentView is the host's NSView*. Returns an opaque handle, or 0 on failure.
void* editorOpen (void* parentView, EditorHost* host);
void  editorClose (void* handle);
#endif

} // namespace tbvs

#endif
