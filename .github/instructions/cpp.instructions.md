---
applyTo: "**/*.{h,hpp,hxx,c,cc,cpp,cxx,ipp}"
---

# C/C++ coding and review instructions (SKALED)

These instructions apply to C/C++ files and complement `.github/copilot-instructions.md`.

## Safety and correctness

- Do not ignore return values or error paths.
- Avoid raw pointers when possible.
- If using raw pointers, check for null immediately before dereference.
- Validate preconditions and postconditions where practical.
- Protect shared mutable state with mutexes/atomics/other safe primitives.

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
