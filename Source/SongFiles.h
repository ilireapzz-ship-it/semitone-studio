#pragma once
#include <juce_core/juce_core.h>
#include <cstring>
#include <map>

// Songs as files: .semitone documents (the same JSON the plugin keeps in a DAW project),
// the recent-files list and the standalone app's crash-safe autosave.
namespace st::files
{
    inline juce::File appData()
    {
        // a test copy can work on its own folder and leave the real one alone
        const auto other = juce::SystemStats::getEnvironmentVariable ("SEMITONE_DATA", {});
        auto d = other.isNotEmpty() ? juce::File (other) : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Semitone Studio");
        d.createDirectory();
        return d;
    }
    inline juce::File songsFolder()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Semitone Studio").getChildFile ("Songs");
        d.createDirectory();
        return d;
    }
    inline juce::File takesFolder()
    {
        auto d = songsFolder().getChildFile ("Takes");
        d.createDirectory();
        return d;
    }
    // 25 Nov: a second copy of the app that couldn't hand over to the first (see createPluginFilter) keeps its own autosave, so the two never
    // write over each other's unsaved song
    inline bool& secondCopy() { static bool second = false; return second; }
    inline juce::File autosaveFile() { return appData().getChildFile (secondCopy() ? "Autosave 2.semitone" : "Autosave.semitone"); }
    inline juce::File previousFile() { return appData().getChildFile (secondCopy() ? "Autosave 2.previous.semitone" : "Autosave.previous.semitone"); }
    inline juce::File comeForwardFile() { return appData().getChildFile ("come-forward.txt"); }   // a second copy asks the first to show itself
    inline juce::File recentFile()   { return appData().getChildFile ("recent.txt"); }

    inline juce::StringArray recent()
    {
        juce::StringArray out;
        for (auto& line : juce::StringArray::fromLines (recentFile().loadFileAsString()))
            if (line.isNotEmpty() && juce::File (line).existsAsFile() && ! out.contains (line)) out.add (line);
        return out;
    }
    // 25 Nov: JUCE's replaceWithText and replaceWithData ignore a write that failed (a full disk) and put the cut-off file in place of the good
    // one; with nothing to write they delete it. Here the new file is written beside the old one, flushed and read back, and only then swapped in.
    inline bool writeBytes (const juce::File& f, const void* data, size_t size)
    {
        if (data == nullptr || size == 0 || f == juce::File()) return false;
        juce::TemporaryFile tmp (f);
        {
            juce::FileOutputStream out (tmp.getFile());
            if (! out.openedOk() || ! out.write (data, size)) return false;
            out.flush();
            if (out.getStatus().failed()) return false;
        }
        {
            juce::MemoryBlock back;
            if (! tmp.getFile().loadFileAsData (back) || back.getSize() != size || std::memcmp (back.getData(), data, size) != 0) return false;
        }
        return tmp.overwriteTargetFileWithTemporary();
    }
    inline bool writeSafely (const juce::File& f, const juce::String& text)
    {
        const auto utf8 = text.toStdString();
        return writeBytes (f, utf8.data(), utf8.size());
    }
    // a whole song, not a file cut short
    inline bool isSong (const juce::String& text)
    {
        return text.isNotEmpty() && juce::JSON::parse (text).getProperty ("song", juce::var()).isObject();
    }

    // a file that arrives in chunks: it is built beside the old one and takes its place only once every chunk is in
    struct Arriving { int next; juce::int64 bytes; bool ok; };
    class Chunks
    {
    public:
        // false once this file has failed; done is set when the last chunk is in place
        bool add (const juce::File& target, int i, int n, const void* data, size_t size, bool decoded, bool& done)
        {
            done = false;
            const auto key = target.getFullPathName();
            const auto part = target.getSiblingFile (target.getFileName() + ".part");
            if (i == 0)
            {
                part.deleteFile();
                coming[key] = Arriving { 0, 0, true };
            }
            auto it = coming.find (key);
            if (it == coming.end()) return false;
            auto& c = it->second;
            if (c.ok) c.ok = decoded && i == c.next && (size == 0 || part.appendData (data, size));
            ++c.next; c.bytes += (juce::int64) size;
            const bool last = i >= n - 1;
            bool ok = c.ok;
            if (last)
            {
                ok = ok && c.bytes > 0 && part.getSize() == c.bytes && part.replaceFileIn (target);
                if (! ok) part.deleteFile();
                coming.erase (it);
                done = ok;
            }
            return ok;
        }
    private:
        std::map<juce::String, Arriving> coming;
    };

    inline void addRecent (const juce::File& f)
    {
        auto list = recent();
        list.removeString (f.getFullPathName());
        list.insert (0, f.getFullPathName());
        while (list.size() > 10) list.remove (list.size() - 1);
        writeSafely (recentFile(), list.joinIntoString ("\n"));
    }
    inline juce::var recentVar()
    {
        juce::Array<juce::var> arr;
        for (auto& p : recent())
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("path", p);
            o->setProperty ("name", juce::File (p).getFileNameWithoutExtension());
            arr.add (juce::var (o));
        }
        return arr;
    }
}
