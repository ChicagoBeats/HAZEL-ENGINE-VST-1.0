# F1nz Engine (VST3)

Engine-sound instrument based on the f1nz FL Studio engine projects: every cylinder firing triggers a
pitched grain of a kick sample (+ rim/perc layer), shaped by soft-clip, high-pass and a throttle-driven
"muffler" low-pass. Includes a rev limiter, supercharger whine, 8 engine types, a spinning rim, a car view
and a dyno / highway-night / highway-day scene.

**Engines:** Inline-4, Flat-6, V8, V10, V12, W16, Rotary (3-rotor), Diesel V8
**Controls:** Engine, RPM, Throttle, Supercharger, Volume, Ignition, Rev limiter, MIDI drive, View, Scene
**MIDI drive:** turn it on, then held note = RPM (C2 ≈ idle, C6 ≈ redline), velocity or mod wheel (CC1) = throttle.

## Get the plugin from GitHub (no install needed on your PC)

1. Create a new **empty** repository on github.com and push this folder to it:
   ```
   git init && git add . && git commit -m "first build"
   git branch -M main
   git remote add origin https://github.com/YOUR-NAME/f1nz-engine-vst3.git
   git push -u origin main
   ```
   (GitHub Desktop works too. Make sure the hidden `.github` folder is included.)
2. Open the repo's **Actions** tab. The *Build VST3* workflow starts by itself (about 5-10 minutes).
3. When it's green, open the run and download the artifacts at the bottom:
   `F1nzEngine-x64` (64-bit) and `F1nzEngine-Win32` (32-bit).
4. Copy the `F1nz Engine.vst3` item from the `VST3` folder into `C:\Program Files\Common Files\VST3\`
   (32-bit build on a 64-bit Windows: `C:\Program Files (x86)\Common Files\VST3\`) and rescan plugins in your DAW.
5. `Standalone\F1nz Engine.exe` runs without a DAW - handy for a first test.

If the run is red, open it, copy the error text from the failed step and send it to me.

## Windows 7 / 8 - read this

* The build uses JUCE 6.1.6 and a **static runtime**, so no Visual C++ redistributable is needed.
* GitHub's compiler (Visual Studio 2022) can produce binaries that need Windows 8+/10 APIs. The workflow scans
  for this and writes `compat_report.txt` into each artifact. A warning there means it may not load on Windows 7;
  re-run the workflow manually (**Actions > Build VST3 > Run workflow**) and enter toolset `v142`.
* Your DAW must also support VST3 on your Windows version - tell me which DAW/version if the plugin doesn't appear.
* I could not test on Windows 7/8 from here. The engine itself is tested automatically on every push.

## Your own artwork

Replace the files in `Assets/Images/` keeping the same names and push again:
`bg_night.png`, `bg_day.png` (any size, scaled to fill 780x300), `car.png` (transparent PNG), `rim.png` (square, transparent circle).
If your background's road is at a different height, change `kRoadY` at the top of `Source/PluginEditor.cpp`.

## Layout

```
Source/EngineCore.h      engine DSP (no JUCE, unit-tested)
Source/Plugin*.cpp/.h    JUCE plugin, parameters, MIDI, UI
tests/test_core.cpp      checks pitch = firing frequency for all engines + CPU use
Assets/                  samples + images (embedded into the plugin)
```

## Licensing notes

* JUCE 6 is GPLv3 / commercial: fine for personal use; if you distribute binaries you must follow the GPL
  (or buy a JUCE licence).
* The WAV samples come from other creators (Mike Zara's Stash, thecxdy) via the f1nz FLP. Get permission or
  swap in your own samples before sharing the plugin publicly.
