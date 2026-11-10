# Copyright Brandon Hillig 2026
# Run by Tools/CreateCombatContent.ps1 inside UnrealEditor-Cmd. All logic lives in C++ (UDwCombatContentLibrary, then
# UDwAiContentLibrary, which needs the combat content); this only calls them and prints their logs between markers the
# PowerShell wrapper extracts. The last line is PASS only if every step passed.
import unreal

steps = [
    unreal.DwCombatContentLibrary.create_combat_placeholder_content,
    unreal.DwAiContentLibrary.create_ai_content,
]

lines = []
passed = True
for step in steps:
    log = list(step())
    passed = passed and bool(log) and log[-1] == "CONTENT PASS"
    lines.extend(log[:-1])
    if not passed:
        break

print("DWCONTENT-BEGIN")
for line in lines:
    print("DWCONTENT " + line)
print("DWCONTENT " + ("CONTENT PASS" if passed else "CONTENT FAIL"))
print("DWCONTENT-END")
