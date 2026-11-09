# Copyright Brandon Hillig 2026
# Run by Tools/CreateCombatContent.ps1 inside UnrealEditor-Cmd. All logic lives in C++ (UDwCombatContentLibrary);
# this only calls it and prints its log between markers the PowerShell wrapper extracts.
import unreal

log = unreal.DwCombatContentLibrary.create_combat_placeholder_content()
print("DWCONTENT-BEGIN")
for line in log:
    print("DWCONTENT " + line)
print("DWCONTENT-END")
