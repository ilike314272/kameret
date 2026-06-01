param([string]$Preset = "windows-release")
cmake --preset $Preset -S $PSScriptRoot/..
cmake --build --preset $Preset
