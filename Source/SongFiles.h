#pragma once
#include <juce_core/juce_core.h>

// Songs as files: .semitone documents (the same JSON the plugin keeps in a DAW project),
// the recent-files list and the standalone app's crash-safe autosave.
namespace st::files
{
    inline juce::File appData()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Semitone Studio");
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
    inline juce::File autosaveFile() { return appData().getChildFile ("Autosave.semitone"); }
    inline juce::File recentFile()   { return appData().getChildFile ("recent.txt"); }

    inline juce::StringArray recent()
    {
        juce::StringArray out;
        for (auto& line : juce::StringArray::fromLines (recentFile().loadFileAsString()))
            if (line.isNotEmpty() && juce::File (line).existsAsFile() && ! out.contains (line)) out.add (line);
        return out;
    }
    inline void addRecent (const juce::File& f)
    {
        auto list = recent();
        list.removeString (f.getFullPathName());
        list.insert (0, f.getFullPathName());
        while (list.size() > 10) list.remove (list.size() - 1);
        recentFile().replaceWithText (list.joinIntoString ("\n"));
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
    // write via a temp file so a crash mid-write never leaves a half-written song
    inline bool writeSafely (const juce::File& f, const juce::String& text)
    {
        juce::TemporaryFile tmp (f);
        if (! tmp.getFile().replaceWithText (text, false, false, "\n")) return false;
        return tmp.overwriteTargetFileWithTemporary();
    }
}
