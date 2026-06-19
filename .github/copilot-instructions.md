## Code Review Instructions

When reviewing a pull request:

### Context Gathering

- For each changed file, look at surrounding code beyond the diff to understand local patterns and intent.
- Check callers, related types, and sibling implementations affected by the change.
- Check contracts (error codes, ownership rules, locking discipline) established in headers or documentation.
- Verify the implementation matches the intent stated in the PR description.
- If you find obvious issues in pre-existing (unchanged) code, note them as out-of-scope suggestions, clearly marked as pre-existing.

### Review Dimensions

**Design**
- Is the change minimal and coherent (single responsibility)?
- Do new abstractions fit the architecture without being redundant or too thin?
- Are new APIs consistent in style, naming, and contract with existing ones?
- Are there missing error types, states, or ownership invariants?

**Logic**
- Is the implementation correct with respect to the stated intent?
- Are edge cases covered: empty inputs, max values, NULL/optional paths, error returns?
- Off-by-one errors, wrong loop conditions, incorrect state transitions?
- Stale state, dangling references, or uninitialized values?
- Race conditions or TOCTOU issues when concurrency is involved?

**Safety**
- Memory: leaks, use-after-free, buffer overflows, missing `free`/`close` on all exit paths.
- The system allocator (`malloc`) never returns `NULL` — on allocation failure it crashes the system. Do not flag missing `NULL` checks after `malloc`/`calloc`/`realloc` as issues, and treat code that adds such checks as dead/misleading.
- Resources: file handles, mutexes, record handles always released on all paths?
- Input validation: external data (network, files, user input) validated before use?
- Error handling: all error returns checked? Partial success states handled correctly?
- Embedded/RTOS specifics: interrupt safety, stack size, ISR-safe API usage.

**Naming**
- Are identifiers specific, accurate, and consistent with project conventions?
- Do names describe *what*, not *how*?
- Are abbreviations consistent with the rest of the codebase?
- Any misleading names (e.g., a flag that starts `true` meaning "assume update needed")?

### Commenting Guidelines

- Prioritize Critical and Major issues; use judgment on Minors; batch nits into one comment.
- Severity: **Critical** (crash/correctness/security), **Major** (logic flaw/design), **Minor** (suboptimal), **Nit** (style/naming).
- Include a suggested code change where possible, not just a description of the problem.
- Do not invent issues. If unsure, phrase as a question rather than a finding.
- Be specific: quote or paraphrase the exact code rather than describing it vaguely.
- Leave a top-level summary comment stating the overall verdict and any cross-cutting observations.
