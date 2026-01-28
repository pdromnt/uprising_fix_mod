# Uprising Fix Mod

A mod for Uprising Join or Die that uses the dsound.dll to hook into the game's code and inject some fixes:

1- Fix game only using blue skies no matter the level  
2- Fix broken CD Audio (missing music in menus and game)  
3- Attempt to fix jittery mouse (if mouse is still broken after, see fix in tips below)  

## Tips

- Is the game hanging on the loading screen? (Crashing, black screen)
    - **Fix 1**: Try closing down stuff like your RGB control software (SignalRGB, Razer Synapse, GHub, etc)
    - **Fix 2**: Go into Task Manager, find the `uprising.exe` process, right click, go into `Go to details` and once there right click again and select `Analyze wait chain`. That should clue you into what's hanging the game and what you'll need to close in order to start it up.

- Is your mouse funky?
    - When in the Main Menu, go to your settings and controls options, where you'll see the keybinds, on the top right, set **Acceleration to zero/minimum**, and **sensitivity to the max**.

- _dgVoodoo is a better option than nGlide_. You may want to give it a shot.

- Uprising **requires** DirectShow/DirectPlay to be installed, if you didn't get prompted by Windows to install it, go into `Turn Windows features on or off` > `Legacy Components` > Enable `DirectPlay` and reboot your machine when prompted.

## How to Build

- Install G++ (currently i'm using this: https://github.com/jmeubank/tdm-gcc)
- Run `build_hook.bat`
- It'll give you the `dsound.dll` that the game will hook onto.

## Download and Installation

Clone the repo or use the releases page to download the zip file with all the files. Then extract it and place all the files **in the same directory** as the `uprising.exe` executable and that should do it. If it asks to replace any files (probably the settings file and the winmm.ini), say yes.

## License

UNLICENSE

## Contribs

Please do, just open a PR. If you open an issue, please detail your issue as much as possible. Issues with "this is borked" and no explanation/reproduction steps **will be closed**.

## Also...

**Screw Ziggurat** for not letting the community help and sitting on the IP. Gives us the source so we can fix this game!!!

## Credits

[OGG-WINMM](https://github.com/bangstk/ogg-winmm): I'm using the original OGG-WINMM dll that comes with the re-releases, just renamed and forcefully patched in so it loads properly.
