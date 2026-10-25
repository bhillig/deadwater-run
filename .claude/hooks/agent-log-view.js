// Prints the agent change log grouped by agent.
// Usage: node .claude/hooks/agent-log-view.js [--since YYYY-MM-DD | --all] [--agent <name>]
// Defaults to today's changes.
const fs = require('fs');
const path = require('path');

const logPath = path.join(__dirname, '..', 'logs', 'agent-log.jsonl');

// Log times are UTC; "today" means local midnight, expressed in UTC so string comparison works.
function localMidnightUtc() {
    const d = new Date();
    d.setHours(0, 0, 0, 0);
    return d.toISOString();
}

function parseArgs(argv) {
    const args = { since: localMidnightUtc(), agent: null };
    for (let i = 0; i < argv.length; i++) {
        if (argv[i] === '--all') {
            args.since = '';
        } else if (argv[i] === '--since') {
            args.since = argv[++i];
        } else if (argv[i] === '--agent') {
            args.agent = argv[++i];
        }
    }
    return args;
}

function loadEntries() {
    if (!fs.existsSync(logPath)) {
        return [];
    }
    return fs.readFileSync(logPath, 'utf8')
        .split('\n')
        .filter(Boolean)
        .map((line) => {
            try {
                return JSON.parse(line);
            } catch {
                return null;
            }
        })
        .filter(Boolean);
}

function main() {
    const args = parseArgs(process.argv.slice(2));
    const entries = loadEntries()
        .filter((e) => !args.since || e.time >= args.since)
        .filter((e) => !args.agent || e.agent === args.agent);

    if (entries.length === 0) {
        console.log(`No agent changes${args.since ? ` since ${args.since}` : ''}.`);
        return;
    }

    const byAgent = new Map();
    for (const e of entries) {
        if (!byAgent.has(e.agent)) {
            byAgent.set(e.agent, []);
        }
        byAgent.get(e.agent).push(e);
    }

    for (const [agent, list] of byAgent) {
        const files = new Set(list.filter((e) => e.file).map((e) => e.file));
        console.log(`\n== ${agent} (${list.length} changes, ${files.size} files) ==`);
        for (const e of list) {
            const time = new Date(e.time).toLocaleString('sv-SE').slice(0, 16);
            const target = e.file || e.detail || '';
            console.log(`  ${time}  ${e.branch || '-'}  ${e.tool}  ${target}`);
        }
    }
}

main();
