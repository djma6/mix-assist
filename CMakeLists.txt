cmake_minimum_required(VERSION 3.22)
project(MixAssistant VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MixAssistant
  COMPANY_NAME "Masih"
  PLUGIN_MANUFACTURER_CODE Msih
  PLUGIN_CODE Mxas
  FORMATS VST3 Standalone
  PRODUCT_NAME "Mix Assistant"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE)

target_sources(MixAssistant PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(MixAssistant PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
  target_compile_options(MixAssistant PRIVATE /utf-8)
endif()

target_link_libraries(MixAssistant
  PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags
         juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)
