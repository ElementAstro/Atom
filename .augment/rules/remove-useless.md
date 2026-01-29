---
type: "manual"
---

# Remove Unnecessary Files

Review the Atom project directory structure and identify files that should be
removed. Specifically:

1. **Process/procedural documentation files** - Remove any temporary or
   intermediate documentation files (`*.md`, `*.txt`) that were created during
   development processes but are not part of the official project
   documentation (keep official docs in `docs/` and `doc/` directories)

2. **Unnecessary files** - Identify and remove:
   - Temporary build artifacts not covered by .gitignore
   - Duplicate files
   - Obsolete configuration files
   - Unused scripts or tools
   - Any files that don't serve a current purpose in the project

## Important Constraints

- Do NOT remove any files from `docs/`, `doc/`, `tests/`, `atom/`, `python/`,
  `cmake/`, `scripts/`, or `example/` directories unless they are clearly
  duplicates or obsolete
- Do NOT remove official documentation (README.md, CONTRIBUTING.md, LICENSE)
- Do NOT remove any source code, test files, or build configuration files
- Before deleting any file, explain why it's considered unnecessary and get
  confirmation

First, scan the project directory to identify candidates for removal, then
present a list with justification for each file before proceeding with any
deletions.
