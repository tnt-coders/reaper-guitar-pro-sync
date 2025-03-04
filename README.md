
# REAPER Guitar Pro Sync
Ever want to have a Guitar Pro tab play along with your REAPER project perfectly in sync? Now you can!

**Demo Video:**
[![Demo Video](https://i.imgur.com/OgXCUdy.jpeg)](https://youtu.be/CZxBhGkcF-s)

**REAPER Guitar Pro Sync** is a plugin for REAPER that gives Guitar Pro 8 control over REAPER playback. This allows you to practice along to your REAPER projects with a Guitar Pro tab, using REAPER to control all audio. I created this plugin so that I could use VST guitar tones when I play along with Guitar Pro and use REAPER automation to control tone switches and parameter changes in sync with the Guitar Pro tab.
## Supported Features
This plugin allows Guitar Pro to control the following:
* Play/pause state in REAPER
* Cursor location (Play cursor in REAPER will follow actions taken in Guitar Pro including looping and jumping from one location to another)
* Playback speed in REAPER (While Guitar Pro is playing the playback speed in REAPER will be set to match the current speed in Guitar Pro)
# Installation/Usage
* Grab the latest DLL file from the [releases](https://github.com/tnt-coders/reaper-guitar-pro-sync/releases) page and place it in your REAPER UserPlugins folder. (for example `C:\Users\username\AppData\Roaming\REAPER\UserPlugins`)
* Restart REAPER
* Look for `TNT: Toggle Guitar Pro sync` in REAPER's "Actions" list.
# Guitar Pro/REAPER Project Setup
In order for this PLUGIN to function correctly it expects that the tempo map for your REAPER project matches the tempo map in Guitar Pro *EXACTLY*. If it is off even slightly things will not play back in sync.
## Importing Guitar Pro Tempo Map Into REAPER
* Sync the audio with the tab in Guitar Pro first, then add a "drum" track to the project and export the project as MIDI.
* In REAPER you can drag and drop the MIDI file into your project on the first beat of the first measure. REAPER should prompt you asking if you want to take the tempo map from the MIDI file.
* Then add the audio to the REAPER project and make sure you are using the same audio file you used for Guitar Pro and ensure it begins playback at the same location on the tempo map as it does in Guitar Pro.
* Once this is done, you can delete the MIDI track from the project and remove the original MIDI file. It is no longer needed.
## Supported Guitar Pro Versions
This plugin is only guaranteed to work with Guitar Pro 8 (Version 8.1.3 - Build 121)
* This may be expanded to other versions if Guitar Pro starts getting more updates, but this is currently the latest version.
* The reason it is tied to this specific version is that Guitar Pro does not have any public API so this program functions by directly reading memory from the Guitar Pro application. If Guitar Pro is updated, memory addresses of information this plugin uses may change causing the plugin to break.
# Compiling From Source
It's recommended to read all steps in advance before beginning installation. Currently only Windows builds are supported.
## Minimal Visual Studio Installation
Install [Visual Studio Community with Develop C and C++ applications component](https://visualstudio.microsoft.com/vs/features/cplusplus/). Default installation includes MSVC compiler and CMake. Visual Studio installation can be trimmed down before installation or afterwards by cherry-picking only the necessary components from Visual Studio Installer > Individual Components section.
* C++ CMake tools for Windows
* Windows SDK
## Project Setup
* Install [Visual Studio Code](https://code.visualstudio.com/) (VSCode).
* Install [Git](https://git-scm.com/downloads), if not already installed. 
* On Windows, open **Developer PowerShell (or Command Prompt) for VS**. Change directory to user preferred location for source repositories, or make one.
* Get [reaper-guitar-pro-sync](https://github.com/tnt-coders/reaper-guitar-pro-sync) files with `git clone --recursive https://github.com/tnt-coders/reaper-guitar-pro-sync.git`.
* Change directory to root of this repository.
* Install [VSCode C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) with `code --verbose --install-extension ms-vscode.cpptools-extension-pack`. This might take a while.
* Open new VSCode workspace by issuing command `code .` (in root directory of this repository).
* VSCode finishes installing the C/C++ Extensions pack. This is indicated in Status Bar.
* Wait, until all dependencies have been downloaded and installed. This might take a while. 
* Then quit VSCode and restart.
* If VSCode notifies about Reload, then Reload.
* If VSCode notifies to configure project 'plugin' with CMake Tools, choose Yes to configure. CMake Tools can also be set to allow always configure new C/C++ projects.
* Select appropriate build kit for platform and architecture. Visual Studio for Windows (e.g. Visual Studio Community Release 2022 - amd64 for modern Windows PC).
* VSCode should also notify about setting up IntelliSense for current workspace. Allow this.
* If this did not happen, these can be set up by issuing VSCode Command Palette Commands (Ctrl/Cmd + Shift + P) `CMake: Configure` and `CMake: Select a Kit`, or from VSCode Status Bar. 
## Build/Install
* On Windows, VSCode needs to be started from Developer PowerShell (or Command Prompt) for VS.
* By default, VSCode builds a debug version of the plugin it by running `CMake: Build` or keyboard shortcut `F7`.
* Install plugin with VSCode command `CMake: Install`.
* Start REAPER, and new plugin and it's Action `TNT: Toggle Guitar Pro sync` should show up in the Actions List.
* Running the Action should give Guitar Pro the ability to control reaper when playing a song.\
## Debugging
* Choosing between debug and release builds can be done with `CMake: Select Variant`.
* Debugging is launched with `F5`. First time, VSCode opens up default Launch Task configuration for debugging. Choose correct Environment and select Default Configuration. In `launch.json` file, edit the `"program":` value to match REAPER executable/binary installation path, e.g. `"program": "C:/Program Files/REAPER (x64)/reaper.exe"`.
* Example `launch.json`:<br>
![image](https://i.imgur.com/ufG4jMf.png)

* [VSCode debugger](https://code.visualstudio.com/docs/cpp/cpp-debug) allows step-by-step code execution, watching variables, etc.<br>
![image](https://i.imgur.com/N4LuyFV.gif)

