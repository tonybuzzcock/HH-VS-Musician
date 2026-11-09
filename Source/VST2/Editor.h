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
};

#if defined(__APPLE__)
// parentView is the host's NSView*. Returns an opaque handle, or 0 on failure.
void* editorOpen (void* parentView, EditorHost* host);
void  editorClose (void* handle);
#endif

} // namespace tbvs

#endif
