juce_add_plugin(TchiskibidiMaster
    COMPANY_NAME "Tchiskibidi"
    PRODUCT_NAME "Tchiskibidi Master"
    BUNDLE_ID com.tchiskibidi.master
    PLUGIN_MANUFACTURER_CODE Tchk
    PLUGIN_CODE Tmst
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mastering Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(TchiskibidiMaster PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(TchiskibidiMaster PRIVATE ${TCHISKIBIDI_SHARED_DIR})
target_compile_definitions(TchiskibidiMaster PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TchiskibidiMaster
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
