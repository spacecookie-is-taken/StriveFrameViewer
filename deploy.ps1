# This just copies the dll to the game files and launches the game to save me a few clicks while testing

Copy-Item ".\Output\StriveFrameData\Game__Debug__Win64\StriveFrameData.dll" "C:\Program Files (x86)\Steam\steamapps\common\GUILTY GEAR STRIVE\RED\Binaries\Win64\Mods\StriveFrameViewer\dlls\main.dll"
Copy-Item ".\Output\Output\Game__Debug__Win64\UE4SS\bin\UE4SS.dll" "C:\Program Files (x86)\Steam\steamapps\common\GUILTY GEAR STRIVE\RED\Binaries\Win64\UE4SS.dll"

& "C:\Program Files (x86)\Steam\steamapps\common\GUILTY GEAR STRIVE\GGST.exe"