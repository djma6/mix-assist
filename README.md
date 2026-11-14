# Mix Assistant (VST3, JUCE 8)

Build:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
VST3: build/MixAssistant_artefacts/Release/VST3/

Track mode on every track, Master mode on the master bus.
Each built-in tool (EQ, Compressor, Stereo, Limiter) has an on/off switch:
OFF = use your own plugin, Mix Assistant still shows the recommended values.
Language: English (default) / Persian, top bar.
