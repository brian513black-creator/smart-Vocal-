# Blackie Vocal Box

Blackie Vocal Box is a Windows x64 VST3 prototype for making close-miked vocals sound drier and more natural. Its automatic EQ uses the tonal profile measured from the supplied `brian.mp3`. Results depend on the source, particularly when reverb overlaps the sung words.

## Use in a DAW

Import the vocal audio file into your DAW and insert **Blackie Vocal Box** on that track. Use a **48 kHz session** to enable AI Noise. Existing recordings can have a different sample rate if the DAW resamples them for the 48 kHz session.

| Control | Behavior |
| --- | --- |
| AI Noise | Blends in Xiph RNNoise v0.1, a free, BSD-licensed pretrained speech denoiser. Start low on breaths, sustained notes, and quiet harmonies. |
| Room Tail | Follows recent vocal level and gently reduces quieter tails between phrases. Heavy reverb underneath words will remain. |
| Natural EQ | Applies a broad tonal contour to reduce rumble and low-mid buildup and add a slight articulation lift. |
| Auto EQ (Brian) | Starts at 80% on new inserts. Play an isolated vocal and the plugin estimates and applies the band corrections automatically. Set it to zero to hear Natural EQ on its own. Existing sessions retain their saved settings. |

The model runs offline and needs no account, API key, or runtime model download. The VST3 supports mono and stereo. It reports 20 ms of latency at 48 kHz so the DAW can compensate. Auto EQ uses four broad bands and starts adapting after about half a second of active audio. It follows the tonal balance across active phrases and limits boosts and cuts. In stereo it applies the same correction to both channels. The build includes the band measurements only. Auto EQ uses signal analysis and adds no latency. It runs at session rates of 16 kHz and above.

## Build on Windows

Install Visual Studio 2022 with **Desktop development with C++**, CMake 3.22 or newer, and Git. From the checkout in PowerShell:

```powershell
cmake -S . -B build -A x64 -DBVB_BUILD_PLUGIN=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

The same steps run in GitHub Actions. Download the **Blackie-Vocal-Box-Windows-x64** artifact from a successful **Windows VST3** run, or open the local build output under `build/BlackieVocalBox_artefacts/Release/VST3`. Copy the entire **Blackie Vocal Box.vst3** bundle to `C:\Program Files\Common Files\VST3` and rescan plugins in your DAW.

## Check the DSP on Linux

```sh
cmake -S . -B build -DBVB_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The checks cover dry-path latency, denoising on simulated fan noise, room-tail attenuation, Natural EQ, and Auto EQ. They use synthetic signals. Audition the plug-in on real vocals and check the Windows DAW before relying on it in production.

The build fetches pinned JUCE 8.0.8 and RNNoise v0.1 source from GitHub. RNNoise includes its trained model in the pinned source. Its BSD notice is included with the CI artifact. JUCE offers AGPLv3 and commercial licensing options. Review JUCE's terms before distributing a build.
