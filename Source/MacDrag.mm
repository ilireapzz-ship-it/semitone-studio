#import <AppKit/AppKit.h>
#include "MacDrag.h"

@interface SemitoneStudioDragSource_1 : NSObject <NSDraggingSource>
@end

@implementation SemitoneStudioDragSource_1
- (NSDragOperation) draggingSession: (NSDraggingSession*) session sourceOperationMaskForDraggingContext: (NSDraggingContext) context
{
    (void) session; (void) context;
    return NSDragOperationCopy;
}
- (BOOL) ignoreModifierKeysForDraggingSession: (NSDraggingSession*) session { (void) session; return YES; }
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

        NSDraggingItem* item = [[[NSDraggingItem alloc] initWithPasteboardWriter: url] autorelease];
        const NSPoint p = [view convertPoint: inWindow fromView: nil];
        [item setDraggingFrame: NSMakeRect (p.x - 16.0, p.y - 16.0, 32.0, 32.0)
                      contents: [[NSWorkspace sharedWorkspace] iconForFile: nsPath]];

        static SemitoneStudioDragSource_1* source = [[SemitoneStudioDragSource_1 alloc] init];

        NSDraggingSession* session = [view beginDraggingSessionWithItems: @[ item ] event: event source: source];
        if (session == nil)
            return DragResult::failed;

        session.animatesToStartingPositionsOnCancelOrFail = YES;
        return DragResult::started;
    }
}
}
