juce_add_plugin(TchiskibidiBasse
    COMPANY_NAME "Tchiskibidi"
    PRODUCT_NAME "Tchiskibidi Basse"
    BUNDLE_ID com.tchiskibidi.basse
    PLUGIN_MANUFACTURER_CODE Tchk
    PLUGIN_CODE Tbas
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(TchiskibidiBasse PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(TchiskibidiBasse PRIVATE ${TCHISKIBIDI_SHARED_DIR})
target_compile_definitions(TchiskibidiBasse PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TchiskibidiBasse
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
