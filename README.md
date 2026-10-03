# SISHHIN HZ MACHINE (VST3)

Guitar amp / pedal / cab / EQ plugin. UI = `Source/ui/index.html` rendered in a JUCE WebView (WebView2 on Windows).

## Get the .vst3 (no Windows machine needed to build)
1. Create a new GitHub repo and push/upload everything in this folder (keep the `.github` folder).
2. Open the **Actions** tab -> **Build VST3 (Windows)** -> latest run (it starts automatically on push; or press **Run workflow**).
3. When it is green, download the artifact **SISHHIN-HZ-MACHINE-VST3-Windows** (a zip containing `SISHHIN HZ MACHINE.vst3`).

## Install for FL Studio (Windows)
1. Unzip and copy the whole `SISHHIN HZ MACHINE.vst3` folder to `C:\Program Files\Common Files\VST3\`
2. FL Studio -> Options -> Manage plugins -> **Find installed plugins** (verify "Scan" includes the VST3 folder).
3. Load it as an **effect** on a mixer insert (it is an FX plugin, not an instrument).

Requires the Microsoft **WebView2 Runtime** (already on Windows 11 and up-to-date Windows 10; otherwise install the Evergreen runtime from Microsoft).

## Notes
- UI <-> plugin uses plain native functions (`setParam`, `setParams`, `getParams`, `gesture`). If the bridge fails, a red banner appears at the top of the plugin window with the error.
- The UI no longer overwrites your saved settings when the window opens; it reads the real values from the plugin.
- Signal chain: Input -> Gate -> Transpose -> HZ DROP -> HZ SITAR -> HZ COMP -> HZ BOOST -> HZ DRIVE -> Amp (4x oversampled) -> Cab/Mic model or your WAV IR -> EQ -> Room -> Output
- Pitch shifting (Transpose / HZ DROP) adds roughly 20 ms of delay when active; the plugin does not report it to the host.
- Presets and A/B slots are UI-side; the host saves all knob values with your project.
- Releases: push a tag like `v1.0.0` and the workflow also attaches a zip to a GitHub Release.
