# Copyright Brandon Hillig 2026
# Run by Tools/CreateUIContent.ps1 inside UnrealEditor-Cmd. All logic lives in C++ (UDwUIContentLibrary);
# this only calls it and prints its log between markers the PowerShell wrapper extracts.
import unreal

log = unreal.DwUIContentLibrary.create_ui_content()
print("DWCONTENT-BEGIN")
for line in log:
    print("DWCONTENT " + line)
print("DWCONTENT-END")
