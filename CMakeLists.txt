cmake_minimum_required(VERSION 3.22)

# static MSVC runtime -> no VC++ redistributable needed on the target PC
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(SishhinHZMachine VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# JUCE's JS bridge (window.__JUCE__ helpers) is embedded next to our UI, served at /js/juce/index.js
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/uigen)
file(COPY ${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/index.js DESTINATION ${CMAKE_BINARY_DIR}/uigen)

# Windows: JUCE links WebView2 statically but does not download the SDK itself, so fetch the NuGet package here.
if(WIN32 AND NOT JUCE_WEBVIEW2_PACKAGE_LOCATION)
    set(WV2_VERSION 1.0.1901.177)
    set(WV2_ROOT ${CMAKE_BINARY_DIR}/webview2-nuget)
    set(WV2_DIR  ${WV2_ROOT}/Microsoft.Web.WebView2.${WV2_VERSION})
    if(NOT EXISTS ${WV2_DIR}/build/native/include/WebView2.h)
        message(STATUS "Downloading Microsoft.Web.WebView2 ${WV2_VERSION} from NuGet...")
        file(MAKE_DIRECTORY ${WV2_DIR})
        file(DOWNLOAD https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WV2_VERSION}
             ${WV2_ROOT}/webview2.zip SHOW_PROGRESS STATUS wv2_status TLS_VERIFY ON)
        list(GET wv2_status 0 wv2_code)
        if(NOT wv2_code EQUAL 0)
            message(FATAL_ERROR "WebView2 NuGet download failed: ${wv2_status}")
        endif()
        file(ARCHIVE_EXTRACT INPUT ${WV2_ROOT}/webview2.zip DESTINATION ${WV2_DIR})
    endif()
    set(JUCE_WEBVIEW2_PACKAGE_LOCATION ${WV2_ROOT} CACHE PATH "WebView2 package dir" FORCE)
endif()

juce_add_binary_data(UIData
    HEADER_NAME UIData.h
    NAMESPACE   UIData
    SOURCES     Source/ui/index.html
                ${CMAKE_BINARY_DIR}/uigen/index.js)

juce_add_plugin(SishhinHZMachine
    COMPANY_NAME               "SISHHIN"
    PLUGIN_MANUFACTURER_CODE   Sshn
    PLUGIN_CODE                Hzm1
    FORMATS                    VST3
    PRODUCT_NAME               "SISHHIN HZ MACHINE"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    VST3_CATEGORIES            Fx Distortion
    COPY_PLUGIN_AFTER_BUILD    FALSE
    NEEDS_WEB_BROWSER          TRUE
    NEEDS_WEBVIEW2             TRUE)

target_sources(SishhinHZMachine PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SishhinHZMachine PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)

target_include_directories(SishhinHZMachine PRIVATE Source)

target_link_libraries(SishhinHZMachine
    PRIVATE
        UIData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
