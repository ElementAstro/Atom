---
type: "manual"
---

# FILE AND CODE MANAGEMENT PROTOCOLS

## STRICT RULES FOR FILE OPERATIONS AND CODE CHANGES

### FILE SIZE AND ORGANIZATION MANDATE

#### Rule 1: Reasonable File Size Management

- You MUST keep files at reasonable sizes for good workspace organization
- Large files SHOULD be split into multiple logical files for ease of use
- You MUST verify file sizes using `wc -c filename` when working with large content
- If a file becomes unwieldy, you MUST suggest splitting it into multiple files

#### Rule 2: File Organization Best Practices

**MANDATORY APPROACH for file management:**

1. Calculate planned content size for new files
2. If creating large content: consider logical file splitting
3. For existing files: check current size with `wc -c filename`
4. If file is becoming too large: propose splitting strategy to user
5. Maintain logical organization and clear file purposes

#### Rule 3: Size Monitoring and Reporting

**MANDATORY SEQUENCE for large file operations:**

1. `wc -c filename` to check current file size
2. Report file size when working with substantial content
3. Suggest file splitting when content becomes unwieldy
4. Maintain good workspace organization principles

### FILE CREATION PROTOCOLS

#### New File Creation Requirements

**MANDATORY SEQUENCE - NO DEVIATIONS:**

1. `view` directory to confirm file doesn't exist
2. `codebase-retrieval` to understand project structure and conventions
3. Calculate character count of planned content
4. Verify count under 49,000 characters
5. Present complete file plan to user with character count
6. Wait for explicit user approval
7. Create file using `save-file` ONLY
8. `view` created file to verify contents
9. `wc -c` to verify size compliance
10. Report creation success with verification details

**SKIPPING ANY STEP = IMMEDIATE TASK TERMINATION**

#### File Creation Reporting Format

```
FILE CREATION REPORT:
FILENAME: [exact filename]
PURPOSE: [why file is needed]
PLANNED SIZE: [character count] characters
SIZE VERIFICATION: Under 49,000 limit ✓
USER APPROVAL: [timestamp of approval]
CREATION METHOD: save-file
POST-CREATION SIZE: [actual character count via wc -c]
COMPLIANCE STATUS: [COMPLIANT/VIOLATION]
```

### FILE MODIFICATION PROTOCOLS

#### Existing File Modification Requirements

**MANDATORY SEQUENCE - NO DEVIATIONS:**

1. `view` file to examine current contents and structure
2. `wc -c filename` to get current size
3. `codebase-retrieval` to understand context and dependencies
4. `diagnostics` to check current error state
5. Calculate size impact of planned changes
6. Verify final size will be under 49,000 characters
7. Present modification plan to user with size analysis
8. Wait for explicit user approval
9. Make changes using `str-replace-editor` ONLY
10. `diagnostics` to verify no new errors
11. `wc -c filename` to verify size compliance
12. Report modification success with verification details

**SKIPPING ANY STEP = IMMEDIATE TASK TERMINATION**

#### File Modification Reporting Format

```
FILE MODIFICATION REPORT:
FILENAME: [exact filename]
ORIGINAL SIZE: [character count via wc -c]
PLANNED CHANGES: [description of modifications]
ESTIMATED NEW SIZE: [calculated character count]
SIZE VERIFICATION: Under 49,000 limit ✓
USER APPROVAL: [timestamp of approval]
MODIFICATION METHOD: str-replace-editor
LINES CHANGED: [specific line numbers]
POST-MODIFICATION SIZE: [actual character count via wc -c]
COMPLIANCE STATUS: [COMPLIANT/VIOLATION]
ERROR CHECK: [diagnostics results]
```

### CODE CHANGE MANAGEMENT

#### Pre-Change Requirements

**MANDATORY VERIFICATION CHAIN:**

1. `codebase-retrieval` - understand current implementation thoroughly
2. `view` - examine ALL files that will be modified
3. `diagnostics` - establish baseline error state
4. Cross-validate understanding between tools
5. Create detailed change plan with user approval
6. Verify all dependencies and imports exist
7. Confirm no breaking changes to existing functionality

#### Change Implementation Rules

- You MUST use `str-replace-editor` for ALL existing file modifications
- You are FORBIDDEN from using `save-file` to overwrite existing files
- You MUST specify exact line numbers for all replacements
- You MUST ensure `old_str` matches EXACTLY (including whitespace)
- You MUST make changes in logical, atomic units

#### Post-Change Requirements

**MANDATORY VERIFICATION CHAIN:**

1. `diagnostics` - verify no new errors introduced
2. `wc -c` - verify all modified files comply with size limits
3. `view` - spot-check critical changes were applied correctly
4. `launch-process` - run appropriate tests if available
5. Report all changes made with tool verification

### TESTING REQUIREMENTS

#### Mandatory Testing Protocol

**You MUST test changes when:**

- Any code functionality is modified
- New files with executable code are created
- Configuration files are changed
- Dependencies are modified

#### Testing Sequence

1. `diagnostics` - check for syntax/compilation errors
2. `launch-process` - run unit tests if they exist
3. `launch-process` - run integration tests if they exist
4. `launch-process` - run the application/script to verify functionality
5. `read-process` - capture and analyze all test outputs
6. Report test results with exact output details

#### Test Failure Protocol

When tests fail:

1. **IMMEDIATELY** stop further changes
2. **REPORT** exact test failure details
3. **ANALYZE** failure using `diagnostics`
4. **PRESENT** failure analysis to user
5. **AWAIT** user instructions on how to proceed
6. **DO NOT** attempt fixes without user approval

### ROLLBACK PROCEDURES

#### When Changes Fail

**MANDATORY ROLLBACK SEQUENCE:**

1. **IMMEDIATELY** stop making further changes
2. **DOCUMENT** exactly what was changed and what failed
3. **USE** `str-replace-editor` to revert changes in reverse order
4. **VERIFY** rollback using `diagnostics` and `view`
5. **REPORT** rollback completion with verification
6. **PRESENT** failure analysis to user
7. **AWAIT** user instructions for alternative approach

#### Rollback Verification

- You MUST verify each rollback step using appropriate tools
- You MUST confirm system returns to pre-change state
- You MUST run tests to verify rollback success
- You MUST report rollback completion with evidence

### DEPENDENCY MANAGEMENT

#### Package Manager Mandate

- You MUST use appropriate package managers for dependency changes
- You are FORBIDDEN from manually editing package files (package.json, requirements.txt, etc.)
- You MUST use: npm/yarn/pnpm for Node.js, pip/poetry for Python, cargo for Rust, etc.
- **MANUAL PACKAGE FILE EDITING = IMMEDIATE TASK TERMINATION**

#### Dependency Change Protocol

1. `view` current package configuration
2. `codebase-retrieval` to understand project dependencies
3. Present dependency change plan to user
4. Wait for explicit approval
5. Use appropriate package manager command
6. Verify changes using `view` of updated package files
7. Test that project still works after dependency changes

### DOCUMENTATION REQUIREMENTS

#### You MUST Document

- Every file created with purpose and structure
- Every modification made with rationale
- Every test performed with results
- Every failure encountered with analysis
- Every rollback performed with verification

#### Documentation Format

```
CHANGE DOCUMENTATION:
TIMESTAMP: [when change was made]
FILES AFFECTED: [list of all files]
CHANGE TYPE: [creation/modification/deletion]
PURPOSE: [why change was needed]
IMPLEMENTATION: [how change was made]
VERIFICATION: [tools used to verify]
TEST RESULTS: [outcomes of testing]
SIZE COMPLIANCE: [character counts verified]
STATUS: [SUCCESS/FAILURE/ROLLED_BACK]
```

### QUALITY GATES

#### Gate 1: Pre-Change Verification

- [ ] All information gathered and verified
- [ ] User approval obtained
- [ ] Size limits confirmed
- [ ] Dependencies verified
- [ ] Test plan established

#### Gate 2: Implementation Verification

- [ ] Changes made using correct tools
- [ ] Size limits maintained
- [ ] No syntax errors introduced
- [ ] All modifications documented

#### Gate 3: Post-Change Verification

- [ ] Tests pass or failures documented
- [ ] Size compliance verified
- [ ] No new errors introduced
- [ ] Rollback plan available if needed

**FAILING ANY GATE = IMMEDIATE TASK TERMINATION**
