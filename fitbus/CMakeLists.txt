# Fit Bus: a limiter for the bus the kick, bass and vocal meet on, with a picture coloured by pitch (VST3, and AU on the Mac).
# The processing is fitbus/FitBus.h (no JUCE, tested by tests/fitbus_test.cpp); Plugin.cpp is the plug-in and its window.
if(APPLE)
    set(FB_FORMATS VST3 AU)
else()
    set(FB_FORMATS VST3)
endif()

juce_add_plugin(FitBus
    COMPANY_NAME "Semitone Studio"
    BUNDLE_ID com.semitonestudio.fitbus
    PLUGIN_MANUFACTURER_CODE Smtn
    PLUGIN_CODE Fbus
    FORMATS ${FB_FORMATS}
    PRODUCT_NAME "Fit Bus"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(FitBus PRIVATE Plugin.cpp)

target_compile_definitions(FitBus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(FitBus
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
