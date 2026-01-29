---
type: "manual"
---

# Complete Git Commit and Push

Complete the git commit and push to the remote repository, fixing all
pre-commit hook issues that arise during the process.

## Requirements

1. Stage all current changes for commit
2. Attempt to commit the changes
3. If pre-commit hooks fail, analyze and fix ALL issues reported by the hooks
   (including but not limited to: linting errors, formatting issues, test
   failures, type checking errors)
4. Re-run the commit after fixes until pre-commit hooks pass successfully
5. Push the committed changes to the remote repository
6. Ensure that ALL existing functionality remains intact - do not break any
   current features or tests while fixing pre-commit issues

## Constraints

- Use appropriate git commands for staging, committing, and pushing
- Apply fixes that align with the project's coding standards and conventions
- If pre-commit hooks include formatters (like clang-format, black, etc.),
  allow them to auto-fix when possible
- Verify that all tests still pass after applying fixes
- Do not skip or bypass pre-commit hooks - all issues must be properly
  resolved

Note: The instruction is in Chinese. Translation: "Complete this commit to
remote, and fix all pre-commit issues encountered, without affecting existing
functionality"
