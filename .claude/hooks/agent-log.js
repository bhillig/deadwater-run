// Claude Code hook: appends one JSON line per agent change to the main repo's agent log.
// Runs outside the model, so it costs no tokens. Never blocks a tool call: all errors exit 0.
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const MAX_DETAIL = 200;

function readStdin() {
    try {
        return fs.readFileSync(0, 'utf8');
    } catch {
        return '';
    }
}

function git(cwd, args) {
    try {
        return execFileSync('git', ['-C', cwd, ...args], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim();
    } catch {
        return '';
    }
}

// Worktrees share one common .git dir, so every agent logs into the same main-repo file.
function mainRepoRoot(cwd) {
    const commonDir = git(cwd, ['rev-parse', '--path-format=absolute', '--git-common-dir']);
    if (commonDir) {
        return path.dirname(commonDir);
    }
    return process.env.CLAUDE_PROJECT_DIR || cwd;
}

function relativeTo(root, filePath) {
    if (!filePath) {
        return '';
    }
    const rel = path.relative(root, path.resolve(filePath));
    return rel.startsWith('..') ? filePath : rel.split(path.sep).join('/');
}

// Unreal MCP inputs are free-form, so keep a short, single-line summary of them.
function summarizeInput(toolInput) {
    if (!toolInput || typeof toolInput !== 'object') {
        return '';
    }
    const text = JSON.stringify(toolInput);
    return text.length > MAX_DETAIL ? text.slice(0, MAX_DETAIL) + '...' : text;
}

function buildEntry(payload) {
    const cwd = payload.cwd || process.cwd();
    const root = mainRepoRoot(cwd);
    const branch = git(cwd, ['branch', '--show-current']);
    const toolInput = payload.tool_input || {};

    const entry = {
        time: new Date().toISOString(),
        event: payload.hook_event_name || '',
        agent: payload.agent_type || (branch && branch !== 'master' ? `branch:${branch}` : 'main'),
        branch,
        session: (payload.session_id || '').slice(0, 8),
    };

    if (payload.hook_event_name === 'SubagentStop') {
        // A second SubagentStop arrives without agent_type; it carries no information, so skip it.
        if (!payload.agent_type) {
            return { root, entry: null };
        }
        entry.tool = 'SubagentStop';
        return { root, entry };
    }

    entry.tool = payload.tool_name || '';
    const filePath = toolInput.file_path || toolInput.notebook_path;
    if (filePath) {
        entry.file = relativeTo(root, filePath);
    } else {
        entry.detail = summarizeInput(toolInput);
    }
    return { root, entry };
}

function main() {
    const raw = readStdin();
    if (!raw) {
        return;
    }
    const { root, entry } = buildEntry(JSON.parse(raw));
    if (!entry) {
        return;
    }
    const logDir = path.join(root, '.claude', 'logs');
    fs.mkdirSync(logDir, { recursive: true });
    fs.appendFileSync(path.join(logDir, 'agent-log.jsonl'), JSON.stringify(entry) + '\n');
}

try {
    main();
} catch {
    // Logging must never break an agent's work.
}
process.exit(0);
