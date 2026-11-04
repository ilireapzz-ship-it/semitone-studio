#import <AppKit/AppKit.h>
#include "MacDrag.h"

// Drag source for part files. Implements both the modern and the classic (informal) drag-source
// methods, because the drag is started with the classic file-drag API that older drop targets
// (FL Studio's Playlist among them) understand best.
@interface SemitoneStudioDragSource_2 : NSObject <NSDraggingSource>
@property (nonatomic) NSDragOperation lastOperation;
@end

@implementation SemitoneStudioDragSource_2
- (NSDragOperation) draggingSession: (NSDraggingSession*) session sourceOperationMaskForDraggingContext: (NSDraggingContext) context
{
    (void) session; (void) context;
    return NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationLink | NSDragOperationMove;
}
- (NSDragOperation) draggingSourceOperationMaskForLocal: (BOOL) isLocal
{
    (void) isLocal;
    return NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationLink | NSDragOperationMove;
}
- (BOOL) ignoreModifierKeysWhileDragging { return YES; }
- (BOOL) ignoreModifierKeysForDraggingSession: (NSDraggingSession*) session { (void) session; return YES; }
- (void) draggedImage: (NSImage*) image endedAt: (NSPoint) point operation: (NSDragOperation) operation
{
    (void) image; (void) point;
    self.lastOperation = operation;
}
- (void) draggingSession: (NSDraggingSession*) session endedAtPoint: (NSPoint) point operation: (NSDragOperation) operation
{
    (void) session; (void) point;
    self.lastOperation = operation;
}
@end

namespace st
{
DragResult startMacFileDrag (void* nsViewPtr, const juce::String& path)
{
    @autoreleasepool
    {
        auto* view = (NSView*) nsViewPtr;
        auto* window = view != nil ? [view window] : nil;
        if (window == nil)
            return DragResult::failed;

        // the person must still be holding the mouse button
        if (([NSEvent pressedMouseButtons] & 1) == 0)
            return DragResult::released;

        const NSPoint mouse = [NSEvent mouseLocation];
        const NSPoint inWindow = [window convertRectFromScreen: NSMakeRect (mouse.x, mouse.y, 1.0, 1.0)].origin;

        NSEvent* event = [NSEvent mouseEventWithType: NSEventTypeLeftMouseDragged
                                            location: inWindow
                                       modifierFlags: 0
                                           timestamp: [[NSProcessInfo processInfo] systemUptime]
                                        windowNumber: [window windowNumber]
                                             context: nil
                                         eventNumber: 0
                                          clickCount: 1
                                            pressure: 1.0f];
        if (event == nil)
            return DragResult::failed;

        NSString* nsPath = [NSString stringWithUTF8String: path.toRawUTF8()];
        NSURL* url = [NSURL fileURLWithPath: nsPath];

        // the drag pasteboard carries the file both ways: the classic file list and a file URL
        NSPasteboard* pb = [NSPasteboard pasteboardWithName: NSPasteboardNameDrag];
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
        [pb declareTypes: @[ NSFilenamesPboardType, NSPasteboardTypeFileURL ] owner: nil];
        [pb setPropertyList: @[ nsPath ] forType: NSFilenamesPboardType];
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
        [pb setString: [url absoluteString] forType: NSPasteboardTypeFileURL];

        NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFile: nsPath];
        [icon setSize: NSMakeSize (32.0, 32.0)];

        static SemitoneStudioDragSource_2* source = [[SemitoneStudioDragSource_2 alloc] init];
        source.lastOperation = NSDragOperationNone;

        NSPoint p = [view convertPoint: inWindow fromView: nil];
        p.x -= 16.0;
        p.y += [view isFlipped] ? 16.0 : -16.0;

        // runs until the drop (or cancel)
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
        [view dragImage: icon at: p offset: NSZeroSize event: event pasteboard: pb source: source slideBack: YES];
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE

        return source.lastOperation != NSDragOperationNone ? DragResult::started : DragResult::refused;
    }
}
}
