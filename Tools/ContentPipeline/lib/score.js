/*
 * The evaluator's score: 0-100, never pass/fail, always with a reason.
 *
 * The content-critic scores five dimensions (0-100 each, with a reason). This module turns them into one number
 * the same way every time, so the loop can't be talked into a pass:
 *   score = weighted mean of the dimensions - code-check penalties, capped at BLOCKER_CAP while any blocker remains.
 * The reason string lists every term, so a score can be audited like the GDD agent's priority_score.
 */
'use strict';

const DIMENSIONS = [
  { key: 'rules', weight: 20, what: 'Game rules: only outcomes, requirements, items, units, captains and numbers in the facts; nothing the game can\'t do' },
  { key: 'text_data', weight: 15, what: 'Text matches data: every body, label and result is true to the outcomes of its choice, in every RandomOf branch (V07)' },
  { key: 'design', weight: 20, what: 'Real decisions with sink-or-plunder tension (S02), ship vs crew costs (S03), Good/Bad fit (S07), fair for a medium ~30 minute run, no dominant or trap choice' },
  { key: 'voice', weight: 20, what: 'Sounds like the voice rules (V01-V07), the reference lines and the shipped pool; Ship\'s Log aesthetic (A01-A03)' },
  { key: 'specificity', weight: 25, what: 'Stranger test: could a stranger tell this is Deadwater Run? Low if the event would work unchanged in any other pirate game (S01-S08, C01-C03)' },
];
const PENALTY = { blocker: 15, major: 6, minor: 1 };
const MAX_PENALTY = 60;
const BLOCKER_CAP = 49;
/** The stranger test is a gate, not just a weight: content a stranger can't place as Deadwater Run is wrong however
 *  polished it is, so specificity under STRANGER_FLOOR caps the score below any sensible threshold. */
const STRANGER_FLOOR = 70;
const STRANGER_CAP = 69;

const clamp = (n) => Math.max(0, Math.min(100, Math.round(n)));

/** Folds the critic's dimensions and the code findings into one score with an itemised reason. */
function scoreEvaluation(evaluation, codeFindings) {
  const dims = DIMENSIONS.map((d) => {
    const got = (evaluation.dimensions || {})[d.key] || {};
    return { ...d, score: clamp(+got.score || 0), reason: got.reason || '(no reason given)' };
  });
  const totalWeight = DIMENSIONS.reduce((s, d) => s + d.weight, 0);
  const base = dims.reduce((s, d) => s + d.score * d.weight, 0) / totalWeight;
  const counts = { blocker: 0, major: 0, minor: 0 };
  for (const f of codeFindings) counts[f.severity] = (counts[f.severity] || 0) + 1;
  const penalty = Math.min(MAX_PENALTY, Object.entries(counts).reduce((s, [k, n]) => s + (PENALTY[k] || 0) * n, 0));
  const blockers = [...codeFindings.filter((f) => f.severity === 'blocker').map((f) => `${f.rule} on ${f.event}`),
    ...(evaluation.findings || []).filter((f) => f.severity === 'blocker').map((f) => `${f.rule} on ${f.event}`)];
  let score = clamp(base - penalty);
  const specificity = dims.find((d) => d.key === 'specificity').score;
  const strangerCapped = specificity < STRANGER_FLOOR && score > STRANGER_CAP;
  if (strangerCapped) score = STRANGER_CAP;
  const capped = blockers.length > 0 && score > BLOCKER_CAP;
  if (capped) score = BLOCKER_CAP;
  const reason = [
    `${score}/100.`,
    `Dimensions (weighted ${dims.map((d) => d.weight).join('/')}): ${dims.map((d) => `${d.key} ${d.score} - ${d.reason}`).join(' | ')}.`,
    `Base ${base.toFixed(1)}.`,
    penalty ? `Code checks -${penalty} (${Object.entries(counts).filter(([, n]) => n).map(([k, n]) => `${n} ${k} x${PENALTY[k]}`).join(', ')}).` : 'Code checks: no deductions.',
    strangerCapped ? `Capped at ${STRANGER_CAP}: specificity ${specificity} is under ${STRANGER_FLOOR} - a stranger couldn't tell this is Deadwater Run.` : '',
    capped ? `Capped at ${BLOCKER_CAP}: blocker(s) remain - ${blockers.join('; ')}.` : '',
    evaluation.summary ? `Evaluator: ${evaluation.summary}` : '',
  ].filter(Boolean).join(' ');
  return { score, base: +base.toFixed(1), penalty, capped, strangerCapped, blockers, dims, counts, reason };
}

/** Per-event scores: the critic's number for the event minus that event's code penalties, capped on a blocker. */
function scoreEvents(evaluation, codeFindings, events) {
  return events.map((e) => {
    const got = (evaluation.events || []).find((x) => x.id === e.id) || {};
    const mine = codeFindings.filter((f) => f.event === e.id);
    const critic = (evaluation.findings || []).filter((f) => f.event === e.id);
    const pen = Math.min(MAX_PENALTY, mine.reduce((s, f) => s + (PENALTY[f.severity] || 0), 0));
    let score = clamp((+got.score || 0) - pen);
    const blocked = [...mine, ...critic].some((f) => f.severity === 'blocker');
    if (blocked) score = Math.min(score, BLOCKER_CAP);
    return { id: e.id, score, reason: got.reason || '(no reason given)', findings: [...mine, ...critic.map((f) => ({ source: 'critic', ...f }))] };
  });
}

/** The loop accepts only a high score with no blocker anywhere and no code major left. */
function accepted(scored, codeFindings, threshold) {
  return scored.score >= threshold && !scored.blockers.length && !codeFindings.some((f) => f.severity === 'major');
}

module.exports = { DIMENSIONS, PENALTY, BLOCKER_CAP, STRANGER_FLOOR, STRANGER_CAP, scoreEvaluation, scoreEvents, accepted };
