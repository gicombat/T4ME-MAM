# T4M<sub>enhanced</sub>

T4M loads into World at War through DirectX `d3d9.dll` and performs enhancements to the game. This fork (T4Me) provides several additional enhancements beyond the final release of T4M, r45. 
The mod works for valid versions of World at War on Steam or CD.
Modders can view the full original release of T4M [here](https://www.ugx-mods.com/forum/index.php?topic=8092.0) on UGX-mods.

> CD versions of World at War may require the [LanFixed .exe](http://bit.ly/1nqdKEF)

## New Features/Fixes
- Automatically patches game .exe with 4GB/LAA flag which fixes vertex corruption/texture issues due to not enough memory (r48)
- Added support for ReShade (rename ReShade's .dll to "d3d9r.dll") (r47+)
- Unlocked model LOD dvars: `r_lodBiasSkinned` and `r_lodBiasRigid` (r47+)
- Added a description and default value for custom bools added to my COD5-Remastered mod primarily related to the HUD (Only useful if using this mod) (r47+)
- Tidied up new DVAR descriptions, should not use periods (r47+)
- Fixed issue when trying to play LAN with T4M (r46+)
- Re-enabled intro cinematic (r46+)

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

## Fixes
- Suppressed console spam
- Enabled solo scoreboard (r39)
- Notetacks in CSC work on all notes and not just the notes which include the suffix `tesla_` (r40)
- An extra fastfile named `mod_ex` can load into a mod if the file exists and has priority over all other fastfiles (r42)

## Installation
Installing the T4M mod is extremely simple:
- Drag the DLL into World at War's root directory
- If playing with ReShade, re-name the ReShade "d3d9.dll" to "d3d9r.dll" and also keep it in the same root directory
- For usage with WINE/Proton, simply add `WINEDLLOVERRIDES="d3d9=n,b" %command%` to World At War's launch options in Steam

## Developers
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
- [re4_tweaks](https://github.com/nipkownix/re4_tweaks)

## Notes
- Fullscreen Borderless: Enter this into console -> `r_fullscreen 0;vid_xpos 0;vid_ypos 0;r_noborder 1;vid_restart`
- Revision 48 of the DLL
- Confirmed weapon limit to work along with every other asset limitation
- Mods created that pass the asset limits require users to have T4M
- Steam and "LanFixed" versions of the game will work with the mod, the CD version may not work

## Bugs
- ~~Vertex Corruption~~ Fixed in r48 with automatic game .exe patch, or user can manually patch [using this](https://ntcore.com/?page_id=371)
- Steam vs CD may not be able to connect to the same lobbies
