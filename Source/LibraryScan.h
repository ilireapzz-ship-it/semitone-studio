#pragma once
#include <juce_core/juce_core.h>

// "Scan my plug-ins": lists plug-in and preset NAMES on this computer so sound suggestions can say what you own.
// Names only, never audio. The list stays in this computer's app-data folder and is never sent anywhere.
// tools/scan_library.py is the reference; the page does the matching against data/sound-library.json.
namespace st::libscan
{
    inline juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Semitone Studio").getChildFile ("my-sound-library.json");
    }

    inline void addPlugins (const juce::File& dir, juce::StringArray& out)
    {
        for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
        {
            const auto ext = e.getFile().getFileExtension().toLowerCase();
            if (ext == ".vst3" || ext == ".component" || ext == ".vst")
                out.addIfNotAlreadyThere (e.getFile().getFileNameWithoutExtension());
        }
    }

    inline void addPresets (const juce::File& dir, const juce::String& pattern, juce::Array<juce::var>& out)
    {
        if (! dir.isDirectory()) return;
        for (const auto& e : juce::RangedDirectoryIterator (dir, true, pattern, juce::File::findFiles))
        {
            if (out.size() >= 20000) return;
            auto* o = new juce::DynamicObject();
            o->setProperty ("pack", e.getFile().getParentDirectory().getFileName());
            o->setProperty ("preset", e.getFile().getFileNameWithoutExtension());
            out.add (juce::var (o));
        }
    }

    inline juce::String scanToJson()
    {
        const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::StringArray plugins;
        juce::Array<juce::var> presets;
       #if JUCE_MAC
        for (const auto& root : { juce::File ("/Library/Audio/Plug-Ins"), home.getChildFile ("Library/Audio/Plug-Ins") })
            for (auto* sub : { "VST3", "Components", "VST" })
                addPlugins (root.getChildFile (sub), plugins);
       #else
        addPlugins (juce::File ("C:/Program Files/Common Files/VST3"), plugins);
       #endif
        const auto fl = docs.getChildFile ("Image-Line/FL Studio/Presets/Plugin database");
        for (auto* kind : { "Generators", "Effects" })
            if (fl.getChildFile (kind).isDirectory())
                for (const auto& e : juce::RangedDirectoryIterator (fl.getChildFile (kind), true, "*.fst", juce::File::findFiles))
                    plugins.addIfNotAlreadyThere (e.getFile().getFileNameWithoutExtension());
        for (const auto& d : { docs.getChildFile ("Xfer/Serum 2 Presets"), docs.getChildFile ("Xfer/Serum Presets"), home.getChildFile ("Splice/presets") })
            addPresets (d, "*.SerumPreset;*.fxp", presets);
        plugins.sort (true);

        auto* o = new juce::DynamicObject();
        o->setProperty ("about", "Names only, made on this computer. Never uploaded.");
        o->setProperty ("at", juce::Time::getCurrentTime().toISO8601 (true));
        juce::Array<juce::var> names;
        for (const auto& p : plugins) names.add (p);
        o->setProperty ("plugins", names);
        o->setProperty ("presets", presets);
        const auto json = juce::JSON::toString (juce::var (o), true);
        file().getParentDirectory().createDirectory();
        file().replaceWithText (json);
        return json;
    }
}
