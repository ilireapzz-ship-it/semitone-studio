#include "PluginEditor.h"
#include "BinaryData.h"
#include "MacDrag.h"
#include "SongFiles.h"
#include "FLMarkers.h"
#include "LibraryScan.h"

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
    prepareMarkers();
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
            o->setProperty ("soundMode", proc.getSoundMode());
            o->setProperty ("sr", proc.getSampleRateNow());
            { const juce::File sf (proc.songFile); o->setProperty ("songFileSame", sf.existsAsFile() && sf.loadFileAsString() == proc.getProjectState()); }
            browser.emitEventIfBrowserIsVisible ("st_init", juce::var (o));
        })
        .withEventListener ("st_song_save", [this] (const juce::var& v) { saveSong (v); })
        .withEventListener ("st_song_open", [this] (const juce::var& v) {
            const auto path = v.getProperty ("path", "").toString();
            if (path.isNotEmpty()) openSongAt (juce::File (path)); else openSong();
        })
        // Semitone's own sounds: the interface sends the section layout, renders what is missing, sends it in chunks
        .withEventListener ("st_soundmode", [this] (const juce::var& v) { proc.setSoundMode ((int) v.getProperty ("mode", 0)); })
        .withEventListener ("st_audio_layout", [this] (const juce::var& v) {
            const auto need = proc.setAudioLayout (v.getProperty ("items", juce::var()));
            auto* o = new juce::DynamicObject();
            juce::Array<juce::var> ids; for (auto& s : need) ids.add (s);
            o->setProperty ("ids", ids);
            o->setProperty ("sr", proc.getSampleRateNow());
            o->setProperty ("seq", v.getProperty ("seq", 0));
            browser.emitEventIfBrowserIsVisible ("st_audio_need", juce::var (o));
        })
        .withEventListener ("st_audio_clip", [this] (const juce::var& v) { clipChunk (v); })
        .withEventListener ("st_title", [this] (const juce::var& v) { setTitle (v.getProperty ("title", "Semitone Studio").toString()); })
        .withEventListener ("st_timeline", [this] (const juce::var& v) { proc.setTimelineFromVar (v); })
        .withEventListener ("st_state", [this] (const juce::var& v) { proc.setProjectState (v.getProperty ("json", "").toString()); })
        .withEventListener ("st_transport", [this] (const juce::var& v) {
            proc.setStandaloneTransport ((bool) v.getProperty ("play", false), (double) v.getProperty ("beat", 0.0), (double) v.getProperty ("bpm", 120.0));
        })
        .withEventListener ("st_save", [this] (const juce::var& v) { saveFile (v.getProperty ("name", "Semitone export").toString(), v.getProperty ("b64", "").toString()); })
        // song structure -> FL Studio Playlist markers, through Semitone's controller script
        .withEventListener ("st_markers", [this] (const juce::var& v) { sendMarkers (v); })
        .withEventListener ("st_scan", [this] (const juce::var&) { scanLibrary(); })
        .withEventListener ("st_scan_load", [this] (const juce::var&) { loadLibraryScan(); })
        .withEventListener ("st_drag_audio", [this] (const juce::var& v) { dragAudio (v); })
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

void SemitoneEditor::clipChunk (const juce::var& v)
{
    // {id, sr, frames, marks:[beat, sec, ...], part, parts, b64 (interleaved stereo int16, little endian)}
    const auto id = v.getProperty ("id", "").toString();
    const int part = (int) v.getProperty ("part", 0), parts = (int) v.getProperty ("parts", 1);
    if (id.isEmpty()) return;
    if (part == 0 || ! building || buildingId != id)
    {
        building = std::make_shared<st::Clip>();
        buildingId = id;
        building->sr = (int) v.getProperty ("sr", 48000);
        building->frames = juce::jlimit (0, 384000 * 900, (int) v.getProperty ("frames", 0));
        building->pcm.assign ((size_t) building->frames * 2, 0);
        building->mb.clear(); building->ms.clear();
        if (auto* mk = v.getProperty ("marks", juce::var()).getArray())
            for (int i = 0; i + 1 < mk->size(); i += 2) { building->mb.push_back ((double) (*mk)[i]); building->ms.push_back ((double) (*mk)[i + 1]); }
        buildingAt = 0;
    }
    const auto data = decode (v.getProperty ("b64", "").toString());
    const size_t n = juce::jmin ((size_t) data.getSize() / sizeof (int16_t), building->pcm.size() - (size_t) buildingAt);
    for (size_t i = 0; i < n; ++i)
    {
        const auto* b = static_cast<const juce::uint8*> (data.getData()) + i * 2;
        building->pcm[(size_t) buildingAt + i] = (int16_t) (b[0] | (b[1] << 8));
    }
    buildingAt += (juce::int64) n;
    if (part >= parts - 1)
    {
        proc.addClip (id, building);
        building.reset(); buildingId.clear();
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", id);
        browser.emitEventIfBrowserIsVisible ("st_audio_got", juce::var (o));
    }
}

void SemitoneEditor::timerCallback()
{
    if ((ticks % 60) == 0) proc.releaseRetired();
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
    // write the part to a temp file, then start a real OS drag the DAW accepts (FL Studio: drop MIDI on the Channel Rack)
    const auto f = dragDir().getChildFile (juce::File::createLegalFileName (name));
    const auto data = decode (base64);
    if (! f.replaceWithData (data.getData(), data.getSize())) { dragDone (false, "write"); return; }
    startDrag (f);
}

juce::File SemitoneEditor::dragDir()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SemitoneStudioDrag");
    dir.createDirectory();
    return dir;
}

void SemitoneEditor::dragDone (bool ok, const juce::String& why)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("ok", ok);
    o->setProperty ("why", why);
    browser.emitEventIfBrowserIsVisible ("st_dragdone", juce::var (o));
}

void SemitoneEditor::startDrag (const juce::File& f)
{
    bool ok = false;
    juce::String why;
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
    dragDone (ok, why);
}

// Song sections as audio: the clips the plugin already plays, cut to each section's exact length
// (no pre-roll, no tail) so sections dropped side by side on the DAW timeline line up bar for bar.
void SemitoneEditor::dragAudio (const juce::var& v)
{
    const auto name = juce::File::createLegalFileName (v.getProperty ("name", "Semitone section").toString()) + ".wav";
    std::vector<std::pair<std::shared_ptr<const st::Clip>, double>> parts;
    int sr = 0;
    if (auto* items = v.getProperty ("items", juce::var()).getArray())
        for (const auto& it : *items)
        {
            auto c = proc.findClip (it.getProperty ("id", "").toString());
            if (! c || (sr != 0 && c->sr != sr)) { dragDone (false, "notready"); return; }
            sr = c->sr;
            parts.push_back ({ c, (double) it.getProperty ("beats", 0.0) });
        }
    if (parts.empty() || sr <= 0) { dragDone (false, "notready"); return; }

    const double bpm = juce::jmax (20.0, (double) v.getProperty ("bpm", 120.0));
    std::vector<std::pair<int, int>> spans;   // [from, to) frames of each clip
    int total = 0;
    for (auto& [c, beats] : parts)
    {
        const int a = juce::jlimit (0, c->frames, (int) std::lround (c->secondsAt (0.0, bpm) * c->sr));
        const int b = juce::jlimit (a, c->frames, (int) std::lround (c->secondsAt (beats, bpm) * c->sr));
        spans.push_back ({ a, b });
        total += b - a;
    }
    juce::AudioBuffer<float> buf (2, juce::jmax (1, total));
    int at = 0;
    constexpr float k = 1.0f / 32768.0f;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const auto* d = parts[i].first->pcm.data();
        for (int f = spans[i].first; f < spans[i].second; ++f, ++at)
        {
            buf.setSample (0, at, d[(size_t) f * 2] * k);
            buf.setSample (1, at, d[(size_t) f * 2 + 1] * k);
        }
    }
    // a few milliseconds of fade at each end so a cut never clicks
    const int fade = juce::jmin (total / 2, (int) (0.004 * sr));
    if (fade > 0) { buf.applyGainRamp (0, fade, 0.0f, 1.0f); buf.applyGainRamp (total - fade, fade, 1.0f, 0.0f); }

    const auto f = dragDir().getChildFile (name);
    f.deleteFile();
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
        if (os == nullptr) { dragDone (false, "write"); return; }
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
        if (w == nullptr) { dragDone (false, "write"); return; }
        os.release();   // the writer owns the stream now
        if (! w->writeFromAudioSampleBuffer (buf, 0, total)) { dragDone (false, "write"); return; }
    }
    startDrag (f);
}

// Runs on its own thread: walking plug-in folders can take a few seconds. Only starts when the user asks.
void SemitoneEditor::scanLibrary()
{
    juce::Component::SafePointer<SemitoneEditor> self (this);
    juce::Thread::launch ([self]
    {
        const auto json = st::libscan::scanToJson();
        juce::MessageManager::callAsync ([self, json] { if (self != nullptr) self->scanDone (json); });
    });
}

void SemitoneEditor::loadLibraryScan()
{
    if (st::libscan::file().existsAsFile())
        scanDone (st::libscan::file().loadFileAsString());
}

void SemitoneEditor::scanDone (const juce::String& json)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("json", json);
    browser.emitEventIfBrowserIsVisible ("st_scan_done", juce::var (o));
}

void SemitoneEditor::sendMarkers (const juce::var& v)
{
    auto* o = new juce::DynamicObject();
    const int inst = st::flmarkers::installScript();
    o->setProperty ("script", inst);                       // 1 installed now, 0 already there, -1 FL folder not found
    o->setProperty ("scriptPath", st::flmarkers::scriptFile().getFullPathName());
    auto* out = st::flmarkers::port();
    int sent = 0;
    if (out != nullptr)
        if (auto* arr = v.getProperty ("markers", juce::var()).getArray())
            for (const auto& m : *arr)
                if (auto* a = m.getArray(); a != nullptr && a->size() >= 2)
                {
                    st::flmarkers::sendMarker (*out, (double) (*a)[0], (*a)[1].toString());
                    ++sent;
                    juce::Thread::sleep (2);   // let FL's MIDI input keep up
                }
    o->setProperty ("port", out != nullptr);
    o->setProperty ("sent", sent);
    browser.emitEventIfBrowserIsVisible ("st_markers_done", juce::var (o));
}

// make the port and the script exist as soon as Semitone opens in FL, so FL's MIDI settings list them
void SemitoneEditor::prepareMarkers()
{
    if (! juce::PluginHostType().isFruityLoops()) return;
    st::flmarkers::installScript();
    st::flmarkers::port();
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
