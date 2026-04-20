---
applyTo: "**/*.{h,hpp,hxx,c,cc,cpp,cxx,ipp}"
---

# C/C++ coding and review instructions (SKALED)

These instructions apply to C/C++ files and complement `.github/copilot-instructions.md`.

## Core C/C++ rules

- Keep line length at or below 100 characters.
- Keep functions at or below 100 lines; split long functions.
- Functions should do one thing and have clear names.
- Prefer private members/methods; keep public API minimal.
- Keep naming style consistent per file.
- Avoid duplicate copy-paste logic; extract helpers.

## Safety and correctness

- Do not ignore return values or error paths.
- Avoid raw pointers when possible.
- If using raw pointers, check for null immediately before dereference.
- Validate preconditions and postconditions where practical.
- Protect shared mutable state with mutexes/atomics/other safe primitives.

## Logging and comments

- Use project logging libraries; avoid `cout` in normal code paths.
- Keep logs plain text (no color formatting).
- Avoid info-level log spam.
- Remove commented-out dead code.
- Add concise comments for complex logic.
- New function: add a short preceding description comment or use a very descriptive name.
- New global/class field: add a short preceding description comment or use a very
  descriptive name.

## SKALED-specific checks

Treat these areas as high risk and require stronger tests:

- consensus/finalization
- execution/state replay
- historic state and snapshots
- p2p/network message handling
- JSON-RPC behavior
- storage format/schema changes
- crypto/signature logic

For Ethereum API changes, include geth compatibility evidence in PR testing notes.
