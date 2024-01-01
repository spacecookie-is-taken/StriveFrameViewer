# This cleans up the mod files so I can test the compiled build in isolation

$game_dir = "C:\Program Files (x86)\Steam\steamapps\common\GUILTY GEAR STRIVE\RED\Binaries\Win64\"
# Folders
if(Test-Path -Path "$game_dir\cache"){
  Remove-Item "$game_dir\cache" -Recurse
}
if(Test-Path -Path "$game_dir\Mods"){
  Remove-Item "$game_dir\Mods" -Recurse
}
if(Test-Path -Path "$game_dir\UE4SS_Signatures"){
  Remove-Item "$game_dir\UE4SS_Signatures" -Recurse
}

# Files
if(Test-Path -Path "$game_dir\UE4SS.log"){
  Remove-Item "$game_dir\UE4SS.log"
}
if(Test-Path -Path "$game_dir\dwmapi.dll"){
  Remove-Item "$game_dir\dwmapi.dll"
}
if(Test-Path -Path "$game_dir\imgui.ini"){
  Remove-Item "$game_dir\imgui.ini"
}
if(Test-Path -Path "$game_dir\UE4SS.dll"){
  Remove-Item "$game_dir\UE4SS.dll"
}
if(Test-Path -Path "$game_dir\UE4SS.log"){
  Remove-Item "$game_dir\UE4SS.log"
}
if(Test-Path -Path "$game_dir\UE4SS-settings.ini"){
  Remove-Item "$game_dir\UE4SS-settings.ini"
}

