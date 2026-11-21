# Kick Fit: a separate effect plug-in for the kick channel (VST3, and AU on the Mac).
# The processing is kickfit/KickFit.h (no JUCE, tested by tests/kickfit_test.cpp); Plugin.cpp is the plug-in and its window.
if(APPLE)
    set(KF_FORMATS VST3 AU)
else()
    set(KF_FORMATS VST3)
endif()

juce_add_plugin(KickFit
    COMPANY_NAME "Semitone Studio"
    BUNDLE_ID com.semitonestudio.kickfit
    PLUGIN_MANUFACTURER_CODE Smtn
    PLUGIN_CODE Kfit
    FORMATS ${KF_FORMATS}
    PRODUCT_NAME "Kick Fit"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(KickFit PRIVATE Plugin.cpp)

target_compile_definitions(KickFit PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(KickFit
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
