#pragma once
#include <juce_core/juce_core.h>

namespace st
{
    enum class DragResult { started, released, refused, failed };

   #if JUCE_MAC
    // Start a real macOS file drag from the plugin window at the mouse position.
    // Used because the drag is requested by the web view (via a message), so there is no
    // AppKit mouse event in flight for JUCE's performExternalDragDropOfFiles to use.
    DragResult startMacFileDrag (void* nsView, const juce::String& path);
   #endif
}
