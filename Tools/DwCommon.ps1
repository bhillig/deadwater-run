# Shared helpers for Deadwater Run scripts (dot-source this file). Windows PowerShell 5.1 compatible.

$script:DwRepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$script:DwProject  = Join-Path $script:DwRepoRoot 'DeadwaterRun.uproject'

function Get-DwRepoRoot { return $script:DwRepoRoot }
function Get-DwProject  { return $script:DwProject }

# Engine root resolution order: $env:UE_ENGINE_DIR -> Epic launcher manifest (matching EngineAssociation)
# -> registry custom builds -> common install folders. Fails loudly instead of guessing.
function Get-DwEngineDir {
    if ($env:UE_ENGINE_DIR -and (Test-Path (Join-Path $env:UE_ENGINE_DIR 'Engine\Build\BatchFiles\Build.bat'))) {
        return $env:UE_ENGINE_DIR
    }

    $assoc = (Get-Content $script:DwProject -Raw | ConvertFrom-Json).EngineAssociation
    $candidates = @()

    $manifest = 'C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path $manifest) {
        $list = (Get-Content $manifest -Raw | ConvertFrom-Json).InstallationList
        $candidates += $list | Where-Object { $_.AppName -eq "UE_$assoc" } | ForEach-Object { $_.InstallLocation }
    }

    $builds = 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds'
    if (Test-Path $builds) {
        $prop = Get-ItemProperty $builds
        if ($prop.PSObject.Properties[$assoc]) { $candidates += $prop.$assoc }
    }

    $candidates += @(
        (Join-Path $env:USERPROFILE "Epic Games\UE_$assoc"),
        "C:\Program Files\Epic Games\UE_$assoc"
    )

    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c 'Engine\Build\BatchFiles\Build.bat'))) { return $c }
    }
    throw "Unreal Engine $assoc not found. Set `$env:UE_ENGINE_DIR to the engine root (the folder that contains Engine\)."
}

function Get-DwEditorCmd {
    return Join-Path (Get-DwEngineDir) 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
}

# Fails with a clear message if an editor has the project's DLLs loaded (the build would fail or hot-reload).
function Assert-DwEditorClosed {
    # Content and test runs: any editor locks the assets (UnrealEditor, UnrealEditor-Win64-DebugGame, UnrealEditor-Cmd).
    # Builds pass -Name for the one editor binary that loads the modules being built.
    param([string] $Name = 'UnrealEditor*')
    $running = @(Get-Process -Name $Name -ErrorAction SilentlyContinue)
    if ($running) {
        $names = ($running | ForEach-Object { "$($_.ProcessName) (pid $($_.Id))" }) -join ', '
        throw "An Unreal Editor is running: $names. Close it first."
    }
}
