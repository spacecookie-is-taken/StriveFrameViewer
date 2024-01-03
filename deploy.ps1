# This just copies the dll to the game files and launches the game to save me a few clicks while testing

$game_dir = "C:\Program Files (x86)\Steam\steamapps\common\GUILTY GEAR STRIVE\RED\Binaries\Win64\"
if(-Not (Test-Path -Path "$game_dir\Mods\StriveFrameViewer\dlls")){
  New-Item -ItemType Directory "$game_dir\Mods\StriveFrameViewer\dlls" | out-null
}
if(-Not (Test-Path -Path "$game_dir\UE4SS_Signatures")){
  New-Item -ItemType Directory "$game_dir\UE4SS_Signatures" | out-null
}

Copy-Item ".\Output\StriveFrameData\Game__Debug__Win64\StriveFrameData.dll" "$game_dir\Mods\StriveFrameViewer\dlls\main.dll" | out-null
Copy-Item ".\Output\Output\Game__Debug__Win64\UE4SS\bin\UE4SS.dll" "$game_dir\UE4SS.dll" | out-null
Copy-Item ".\Output\Output\Game__Debug__Win64\proxy\bin\dwmapi.dll" "$game_dir\dwmapi.dll" | out-null
Copy-Item ".\prereqs\mods.txt" "$game_dir\Mods\mods.txt" | out-null
Copy-Item ".\prereqs\UE4SS-settings.ini" "$game_dir\UE4SS-settings.ini" | out-null
Copy-Item ".\prereqs\FText_Constructor.lua" "$game_dir\UE4SS_Signatures\FText_Constructor.lua" | out-null
if(Test-Path -Path "$game_dir\cache"){
  Remove-Item "$game_dir\cache" -Recurse
}

& "C:\Program Files (x86)\Steam\steamapps\common\GUILTY GEAR STRIVE\GGST.exe"