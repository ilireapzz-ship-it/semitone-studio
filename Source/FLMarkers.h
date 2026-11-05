#pragma once
#include <juce_audio_devices/juce_audio_devices.h>

// Song structure -> FL Studio Playlist markers.
// A plugin can't write markers into FL's Playlist, but FL's own controller scripts can
// (arrangement.addAutoTimeMarker). So Semitone installs a tiny script into FL's Hardware folder,
// opens a virtual MIDI port called "Semitone Studio markers", and sends each marker to the
// script as a SysEx message. One-time setup in FL: MIDI Settings -> enable that input and set
// its controller type to "Semitone Studio".
namespace st::flmarkers
{
    inline const char* portName() { return "Semitone Studio markers"; }

    // The script FL runs. SysEx: F0 7D 'S' 'T' 01 <hex of UTF-8 "beat|name"> F7 (beat = quarter notes from bar 1)
    inline const char* script()
    {
        return R"PY(# name=Semitone Studio
# url=https://github.com/ilireapzz-ship-it/semitone-studio
# semitone-script-version=1
# Receives the song structure from the Semitone Studio plugin and adds Playlist markers.
import arrangement
import general


def OnInit():
    print("Semitone Studio: ready to receive song structure markers")


def OnSysEx(event):
    d = event.sysex
    if d is None or len(d) < 7 or d[1] != 0x7D or d[2] != 0x53 or d[3] != 0x54:
        return
    event.handled = True
    if d[4] != 0x01:
        return
    try:
        payload = bytes.fromhex(bytes(d[5:-1]).decode("ascii")).decode("utf-8")
        beat, name = payload.split("|", 1)
        arrangement.addAutoTimeMarker(int(round(float(beat) * general.getRecPPQ())), name)
    except Exception as e:
        print("Semitone Studio: couldn't add a marker:", e)
)PY";
    }

    // FL's user data folder (default: Documents/Image-Line/FL Studio)
    inline juce::File flUserData()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Image-Line").getChildFile ("FL Studio");
    }

    inline juce::File scriptFile() { return flUserData().getChildFile ("Settings").getChildFile ("Hardware").getChildFile ("Semitone Studio").getChildFile ("device_SemitoneStudio.py"); }

    // returns 1 if it was written now, 0 if already up to date, -1 if FL's folder isn't there or can't be written
    inline int installScript()
    {
        if (! flUserData().isDirectory()) return -1;
        const auto f = scriptFile();
        const juce::String want (script());
        if (f.existsAsFile() && f.loadFileAsString() == want) return 0;
        if (! f.getParentDirectory().createDirectory()) return -1;
        return f.replaceWithText (want, false, false, "\n") ? 1 : -1;
    }

    // one virtual port for every Semitone instance in this process (FL keeps its setting by port name)
    inline juce::MidiOutput* port()
    {
       #if JUCE_MAC
        static std::unique_ptr<juce::MidiOutput> p;
        if (p == nullptr) p = juce::MidiOutput::createNewDevice (portName());
        return p.get();
       #else
        return nullptr;   // Windows has no virtual MIDI ports without an extra driver (loopMIDI)
       #endif
    }

    inline void sendMarker (juce::MidiOutput& out, double beat, const juce::String& name)
    {
        const auto payload = juce::String (beat, 4) + "|" + name.substring (0, 60);
        const auto utf8 = payload.toUTF8();
        juce::String hex = juce::String::toHexString (utf8.getAddress(), (int) std::strlen (utf8.getAddress()), 0);
        juce::MemoryBlock mb;
        const juce::uint8 head[] = { 0x7D, 0x53, 0x54, 0x01 };
        mb.append (head, sizeof (head));
        mb.append (hex.toRawUTF8(), (size_t) hex.length());
        out.sendMessageNow (juce::MidiMessage::createSysExMessage (mb.getData(), (int) mb.getSize()));
    }
}
