# This just copies the dll to the game files and launches the game to save me a few clicks while testing

$unv_dir = "C:\Users\Andrew\Documents\projects\ggst_modding\tools\Unverum\Mods\Guilty Gear -Strive-\StriveFrameViewer\ue4ss\StriveFrameViewer\dlls"

if(-Not (Test-Path -Path "$unv_dir")){
  New-Item -ItemType Directory "$unv_dir" | out-null
}

Copy-Item ".\Output\StriveFrameData\Game__Debug__Win64\StriveFrameData.dll" "$unv_dir\main.dll"
