# DPWIM Documentation Router

Status: Active

## First Read

1. Read `../sdad-state.yaml`.
2. Read this router.
3. Inspect current source, tests, and runtime evidence.
4. Read only the intent-selected route or heading.

## Working Route

| Intent or trigger | Read now | Load on demand |
| --- | --- | --- |
| Any active packet | state, source/tests | one intent-selected route |
| Implement or fix | active SPEC, TODO, findings | implementation notes; ADR |
| Windows capture or sync | platform/audio source and tests | SPEC risk and acceptance sections |
| VST2/VST3 build | CMake, plugin source, build docs | evidence and risk playbook |
| Review or docs | source/tests, SPEC, findings | affected docs; operating-rule heading |
| Release, signing, public repository, licensing | state gates, SPEC, findings | evidence and risk playbook |
| Pause/resume/handoff | state and declared handoff | documentation playbook |
| History/reference | current authorities first | external reference and archive |

## On-Demand Policy And Playbooks

- Policy: `Repository-Operating-Rules.md` by heading.
- Large/private input: `sdad/playbooks/context-and-data.md`.
- Packet/scope: `sdad/playbooks/work-packets.md`.
- Claims/gates/release: `sdad/playbooks/evidence-and-risk-gates.md`.
- Docs/state/handoff: `sdad/playbooks/documentation-and-handoff.md`.
- Advanced eval loops: `sdad/playbooks/advanced-extensions.md`.

## Write Route

| New information | Record in |
| --- | --- |
| Scope, behavior, acceptance | active SPEC |
| Current/deferred work | `TODO-Open-Items.md` |
| Defect, blocked check, risk | `../review-findings.md` |
| Spec-unstated choice | `implementation-notes.md` |
| Durable tradeoff | numbered ADR under `../SPEC/adr/` |
| Current execution | `../sdad-state.yaml` |
| Cross-session continuity | state-declared handoff |

## Source Of Truth

Current owner direction controls requested change intent. Source, tests, and
runtime establish observed behavior. The state-declared active SPEC is the
single normative entrypoint. State owns execution; handoff owns continuity.

## Active Catalog

- Core: state, adapter, active SPEC, TODO, findings, implementation notes.
- Policy: `Repository-Operating-Rules.md`; procedures: `sdad/playbooks/`.
- Current handoff: use `../sdad-state.yaml#current_handoff` when declared.
- Decisions: `../SPEC/adr/`.
- Releases: `releases/`; current preview notes: `releases/v0.1.0.md`.

## Maintenance

Keep this routing-only and bounded. Report documents actually read and checks
actually run.
