#include "PluginEditor.h"
#include "BinaryData.h"
#include "MacDrag.h"
#include "SongFiles.h"

SemitoneEditor::SemitoneEditor (SemitoneProcessor& p)
    : AudioProcessorEditor (p), proc (p), browser (makeOptions())
{
    addAndMakeVisible (browser);
    setResizable (true, true);
    setResizeLimits (720, 480, 4096, 3000);
    // fit the window on smaller laptop screens (the host adds its own title bar)
    int w = 1280, h = 820;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        w = juce::jmin (w, d->userArea.getWidth() - 40);
        h = juce::jmin (h, d->userArea.getHeight() - 140);
    }
    setSize (juce::jmax (720, w), juce::jmax (480, h));
    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    incoming.reserve (1024);
    startTimerHz (60);
}

SemitoneEditor::~SemitoneEditor() { stopTimer(); }

void SemitoneEditor::resized() { browser.setBounds (getLocalBounds()); }

juce::WebBrowserComponent::Options SemitoneEditor::makeOptions()
{
    using Opt = juce::WebBrowserComponent::Options;
    const auto dataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Semitone Studio").getChildFile ("WebView");
    dataDir.createDirectory();

    auto o = Opt{};
   #if JUCE_WINDOWS
    o = o.withBackend (Opt::Backend::webview2);
   #endif
    return o
        .withWinWebView2Options (Opt::WinWebView2{}.withUserDataFolder (dataDir).withBackgroundColour (juce::Colour (0xffE8ECE5)))
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        // the interface is ready: tell it where it runs and hand back the song saved in the DAW project
        .withEventListener ("st_ready", [this] (const juce::var&) {
            auto* o = new juce::DynamicObject();
            o->setProperty ("plugin", ! proc.isStandalone());
            o->setProperty ("host", juce::PluginHostType().getHostDescription());
            o->setProperty ("project", proc.getProjectState());
            o->setProperty ("recent", st::files::recentVar());
            o->setProperty ("songFile", proc.songFile);
            { const juce::File sf (proc.songFile); o->setProperty ("songFileSame", sf.existsAsFile() && sf.loadFileAsString() == proc.getProjectState()); }
            browser.emitEventIfBrowserIsVisible ("st_init", juce::var (o));
        })
        .withEventListener ("st_song_save", [this] (const juce::var& v) { saveSong (v); })
        .withEventListener ("st_song_open", [this] (const juce::var& v) {
            const auto path = v.getProperty ("path", "").toString();
            if (path.isNotEmpty()) openSongAt (juce::File (path)); else openSong();
        })
        .withEventListener ("st_title", [this] (const juce::var& v) { setTitle (v.getProperty ("title", "Semitone Studio").toString()); })
        .withEventListener ("st_timeline", [this] (const juce::var& v) { proc.setTimelineFromVar (v); })
        .withEventListener ("st_state", [this] (const juce::var& v) { proc.setProjectState (v.getProperty ("json", "").toString()); })
        .withEventListener ("st_transport", [this] (const juce::var& v) {
            proc.setStandaloneTransport ((bool) v.getProperty ("play", false), (double) v.getProperty ("beat", 0.0), (double) v.getProperty ("bpm", 120.0));
        })
        .withEventListener ("st_save", [this] (const juce::var& v) { saveFile (v.getProperty ("name", "Semitone export").toString(), v.getProperty ("b64", "").toString()); })
        .withEventListener ("st_drag", [this] (const juce::var& v) { dragFile (v.getProperty ("name", "Semitone part.mid").toString(), v.getProperty ("b64", "").toString()); });
}

std::optional<juce::WebBrowserComponent::Resource> SemitoneEditor::getResource (const juce::String& url)
{
    const auto path = url.fromFirstOccurrenceOf ("/", false, false).upToFirstOccurrenceOf ("?", false, false);
    if (path.isEmpty() || path == "index.html")
    {
        const auto* d = reinterpret_cast<const std::byte*> (BinaryData::index_html);
        return juce::WebBrowserComponent::Resource { std::vector<std::byte> (d, d + BinaryData::index_htmlSize), "text/html" };
    }
    return std::nullopt;
}

void SemitoneEditor::timerCallback()
{
    // standalone app: keep a crash-safe autosave of the song, every couple of seconds while it changes
    if (proc.isStandalone() && (ticks % 120) == 0)
    {
        const auto json = proc.getProjectState();
        if (json.isNotEmpty() && json != lastAutosaved && st::files::writeSafely (st::files::autosaveFile(), json))
            lastAutosaved = json;
    }

    // notes played into the plugin
    incoming.clear();
    if (proc.popIncomingMidi (incoming) > 0)
    {
        juce::Array<juce::var> arr;
        for (auto v : incoming) { arr.add ((int) (v & 0xff)); arr.add ((int) ((v >> 8) & 0xff)); arr.add ((int) ((v >> 16) & 0xff)); }
        auto* m = new juce::DynamicObject();
        m->setProperty ("m", arr);
        browser.emitEventIfBrowserIsVisible ("st_midiin", juce::var (m));
    }

    // transport for the interface: playhead, tempo, whether the host is playing
    auto* o = new juce::DynamicObject();
    o->setProperty ("playing", proc.uiPlaying.load());
    o->setProperty ("ppq", proc.uiPpq.load());
    o->setProperty ("bpm", proc.uiBpm.load());
    o->setProperty ("hostTransport", proc.uiHostHasTransport.load());
    const bool playing = proc.uiPlaying.load();
    ++ticks;
    if ((playing && (ticks % 2) == 0) || playing != lastPlaying || (ticks % 30) == 0)
        browser.emitEventIfBrowserIsVisible ("st_host", juce::var (o));
    else
        delete o;
    lastPlaying = playing;
}

juce::MemoryBlock SemitoneEditor::decode (const juce::String& base64)
{
    juce::MemoryOutputStream mo;
    juce::Base64::convertFromBase64 (mo, base64);
    return mo.getMemoryBlock();
}

void SemitoneEditor::saveFile (const juce::String& name, const juce::String& base64)
{
    const auto data = decode (base64);
    const auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Semitone Studio").getChildFile ("Exports");
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save " + name, folder.getChildFile (juce::File::createLegalFileName (name)), "*" + juce::File (name).getFileExtension());
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, data] (const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              auto* o = new juce::DynamicObject();
                              if (f != juce::File() && f.replaceWithData (data.getData(), data.getSize()))
                              {
                                  o->setProperty ("ok", true);
                                  o->setProperty ("path", f.getFullPathName());
                                  f.revealToUser();
                              }
                              else o->setProperty ("ok", false);
                              browser.emitEventIfBrowserIsVisible ("st_saved", juce::var (o));
                          });
}

void SemitoneEditor::dragFile (const juce::String& name, const juce::String& base64)
{
    // write the part to a temp file, then start a real OS drag the DAW accepts (FL Studio: drop on the Channel Rack)
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SemitoneStudioDrag");
    dir.createDirectory();
    const auto f = dir.getChildFile (juce::File::createLegalFileName (name));
    const auto data = decode (base64);
    auto* o = new juce::DynamicObject();
    juce::String why;
    bool ok = false;

    if (! f.replaceWithData (data.getData(), data.getSize()))
        why = "write";
    else
    {
       #if JUCE_MAC
        void* view = getPeer() != nullptr ? getPeer()->getNativeHandle() : nullptr;
        const auto r = st::startMacFileDrag (view, f.getFullPathName());
        ok = r == st::DragResult::started;
        if (! ok) why = r == st::DragResult::released ? "released" : r == st::DragResult::refused ? "refused" : "failed";
       #else
        // Windows: the drag only works while the button is still held (the web view asked for it by message)
        if (! juce::ModifierKeys::getCurrentModifiersRealtime().isLeftButtonDown())
            why = "released";
        else
        {
            ok = juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, true, this);
            if (! ok) why = "failed";
        }
       #endif
    }

    o->setProperty ("ok", ok);
    o->setProperty ("why", why);
    browser.emitEventIfBrowserIsVisible ("st_dragdone", juce::var (o));
}

// ---------- songs as files ----------
void SemitoneEditor::saveSong (const juce::var& v)
{
    const auto json = v.getProperty ("json", "").toString();
    const bool saveAs = (bool) v.getProperty ("saveAs", false);
    auto name = juce::File::createLegalFileName (v.getProperty ("name", "Untitled song").toString().trim());
    if (name.isEmpty()) name = "Untitled song";
    const juce::File current (proc.songFile);

    auto write = [this, json] (juce::File f) {
        if (f.getFileExtension().isEmpty()) f = f.withFileExtension ("semitone");
        auto* o = new juce::DynamicObject();
        const bool ok = st::files::writeSafely (f, json);
        o->setProperty ("ok", ok);
        if (ok)
        {
            proc.songFile = f.getFullPathName();
            st::files::addRecent (f);
            o->setProperty ("path", f.getFullPathName());
            o->setProperty ("name", f.getFileNameWithoutExtension());
            o->setProperty ("recent", st::files::recentVar());
        }
        browser.emitEventIfBrowserIsVisible ("st_song_saved", juce::var (o));
    };

    if (! saveAs && current.existsAsFile()) { write (current); return; }

    const auto start = current.existsAsFile() ? current : st::files::songsFolder().getChildFile (name + ".semitone");
    chooser = std::make_unique<juce::FileChooser> ("Save song", start, "*.semitone");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, write] (const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f == juce::File()) { auto* o = new juce::DynamicObject(); o->setProperty ("ok", false); o->setProperty ("cancelled", true); browser.emitEventIfBrowserIsVisible ("st_song_saved", juce::var (o)); return; }
                              write (f);
                          });
}

void SemitoneEditor::openSong()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a song", st::files::songsFolder(), "*.semitone");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f.existsAsFile()) openSongAt (f); });
}

void SemitoneEditor::openSongAt (const juce::File& f)
{
    if (! f.existsAsFile())
    {
        auto* o = new juce::DynamicObject(); o->setProperty ("ok", false); o->setProperty ("why", "missing");
        o->setProperty ("recent", st::files::recentVar());
        browser.emitEventIfBrowserIsVisible ("st_song_opened", juce::var (o));
        return;
    }
    songOpened (f, f.loadFileAsString());
}

void SemitoneEditor::songOpened (const juce::File& f, const juce::String& json)
{
    proc.songFile = f.getFullPathName();
    st::files::addRecent (f);
    auto* o = new juce::DynamicObject();
    o->setProperty ("ok", true);
    o->setProperty ("json", json);
    o->setProperty ("path", f.getFullPathName());
    o->setProperty ("name", f.getFileNameWithoutExtension());
    o->setProperty ("recent", st::files::recentVar());
    browser.emitEventIfBrowserIsVisible ("st_song_opened", juce::var (o));
}

void SemitoneEditor::setTitle (const juce::String& t)
{
    // only the standalone app owns its window title; inside a DAW the host names the window
    if (! proc.isStandalone()) return;
    if (auto* top = getTopLevelComponent()) if (top != this) top->setName (t);
}
