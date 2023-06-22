# T4M

T4M loads into World at War through DirectX `d3d9.dll` and performs enhancements to the game.
The mod works for valid versions of World at War on Steam or CD versions with a LanFixed .exe.
Modders can view the full release of T4M [here](https://www.ugx-mods.com/forum/index.php?topic=8092.0) on UGX-mods.

> CD versions of World at War require the [LanFixed exe](http://bit.ly/1nqdKEF)

## Features
- Increased asset limits for T5 standard or higher
  - FX: 600
  - Image: 4096
  - Loaded Sound: 2400
  - Material: 4096
  - Stringtable: 80
  - Weapon: 320*
  - Xmodel: 1500 [r41]
- Increased memory limit from 314572800 bytes to 343932928 bytes (425721856 bytes in r42 and afterwards)
- Included windowed, no-border mode
- Unlocked FoV dvars: `cg_fov`, `cg_fovMin`, and `cg_fovScale`
- Display Dvar types in console
- Unlocked external console and prevent in-game console from disabling
- Included bool dvar ` r_externalconsole con_external` (r43+) for displaying the external console
- Added listassetpool
- Added entity count for `cg_drawfps 2`
- Added listassetcounts (r45+)

## Fork Changes (r47+)
- Re-enabled intro cinematic
- Fixed issue when trying to play LAN with T4M
- Added support for ReShade (rename ReShade's .dll to "d3d9r.dll")
- Unlocked model LOD dvars: `r_lodBiasSkinned` and `r_lodBiasRigid`
- Added a description and default value for a few bools added to my COD5-Remastered mod `gpad_flip_triggers`, `cg_drawXboxHUD`, and `cg_drawHealthCount` (Only applies if using this mod)
- Tidied up new DVAR descriptions, should not use periods

## Game Fixes:
- Suppressed console spam
- Enabled solo scoreboard (r39)
- Notetacks in CSC work on all notes and not just the notes which include the suffix `tesla_` (r40)
- An extra fastfile named `mod_ex` can load into a mod if the file exists and has priority over all other fastfiles (r42)

## Installation
Installing the T4M mod is extremely simple:
- Drag the DLL into World at War's root directory
- If playing with ReShade, re-name the ReShade "d3d9.dll" to "d3d9r.dll" and also keep it in the same root directory
- For usage with WINE/Proton simply add `WINEDLLOVERRIDES="d3d9=n,b" %command%` in World At War's launch options in Steam.

## Developers:
- DidUknowiPwn
- momo5502
- SE2Dev
- TheApadayo

## Contributions
- HitmanVere
- Ray1235
- ProGamerzFTW
- JB Shady
- Clippy95

## Notes
- No Border: Enter this into console to enable no border -> r_fullscreen 0;vid_xpos 0;vid_ypos 0;r_noborder 1;vid_restart
- Revision 45 of the DLL
- Confirmed weapon limit to work along with every other asset limitation
- Mods created that pass the asset limits require users to have T4M
- Steam and "LanFixed" versions of the game will work with the mod, the CD version may not work

## Bugs
- Vertex Corruption: Set all Texture/Specular/Normal Map Resolution to anything but your current settings. All THREE options SHOULD NOT BE the same as your current settings. Lower your texture settings one-by-one until it's working fine. The issue is the game is using too much VRAM than the allocated amount (1024MB capped), and the only way to fix it is to patch your game's .exe (x86 executable 4GB VRAM Patch: https://ntcore.com/?page_id=371)
- Steam vs CD may not be able to connect to the same lobbies
