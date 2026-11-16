// Stand-in for `claude -p --agent <owner>` used by loop.test.js. Reads the ticket prompt on stdin, writes the files
// that ticket needs into the fixture repo (cwd), and commits - like a real agent delivery. DW-901's first attempt
// is deliberately incomplete so the loop's re-scan + retry path runs.
'use strict';
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const prompt = fs.readFileSync(0, 'utf8');
const id = (/Goal:\s+(DW-\d+)/.exec(prompt) || [])[1];
const retry = prompt.includes('Previous attempt is incomplete');
const put = (rel, text) => { fs.mkdirSync(path.dirname(rel), { recursive: true }); fs.writeFileSync(rel, text); };

if (id === 'DW-901') {
  put('Source/DeadwaterRun/Run/DwKrakenBoss.h', 'UCLASS()\nclass DEADWATERRUN_API UDwKrakenBoss : public UObject\n{\n};\n');
  if (retry) put('Content/DeadwaterRun/Data/Kraken/DA_Kraken_Test.uasset', 'stub');
} else if (id === 'DW-902') {
  put('Source/DeadwaterRun/UI/DwBossHud.h', 'UCLASS()\nclass DEADWATERRUN_API UDwBossHud : public UObject\n{\n};\n');
}
execSync('git add -A && git commit -q -m "' + id + ' stub delivery"', { stdio: 'ignore' });
process.stdout.write(JSON.stringify({ result: `${id} delivered (retry=${retry})`, total_cost_usd: 0, num_turns: 1 }));
