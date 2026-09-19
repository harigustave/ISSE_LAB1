# Assignment 1 — LAB_NOTES

Use this notebook alongside the Assignment 1 handout.

- Only the numbered questions in this file require written answers.
- For a prediction question, write the prediction **before** running the experiment that reveals the result.
- Keep evidence small. A few selected lines are better than a complete log.
- Interpretation matters more than pasted output.
- Do not paste complete terminal histories, GDB transcripts, sanitizer logs, Valgrind/Memcheck logs, or screenshots.
- Production repairs belong in the production files. Regression tests belong in the designated test files.
- This notebook records the reasoning behind that engineering work; it does not replace the code or tests.

## 2. Establish the baseline and system model

### Q2.1 — Ownership and transfer model

**Timing — After observation:** Answer after reading the public contracts and `src/main.c`, before any production-code repair.

**Question:**  
Using the public headers and `src/main.c`, draw a small ownership/data-flow model for one successfully parsed expression and for an unsuccessful parse. Mark which components borrow data, which component owns temporary AST state while parsing, when ownership of a root transfers, and who is responsible for releasing AST state on each path.

**Your ownership/data-flow model:**

```text
SUCCESS PATH (one expression, e.g. "2 + 3 * 4"):

stdin ==borrowed stream==> rbc_input_read_line ==fills==> line[] (main's stack
buffer, borrowed by input; no allocation, no transfer)

line[] ==borrowed bytes==> rbc_parse
    parser owns ALL temporary AST state while building
    (nodes from rbc_ast_create_integer / *_take constructors;
     a successful *_take moves child ownership into the new parent)
    RBC_PARSE_OK: exactly one root transfers to *out_ast ==> main now owns root

root ==borrowed (const, no retention)==> rbc_eval ==> value + status

main: rbc_ast_destroy(root)   <- the single release point on this path
main ==> prints value to stdout

FAILURE PATH (unsuccessful parse, e.g. syntax error):

stdin ==> rbc_input_read_line ==> line[]         (same borrows as above)
line[] ==> rbc_parse
    parser still owns every temporary node it acquired
    status != RBC_PARSE_OK: transfers NOTHING, *out_ast stays NULL,
    parser must release all its temporary AST state before returning
main: reports the error, destroys nothing (root is NULL), continues
with the next line (recoverable) or exits 2 (NOMEM)
```

**Interpretation:**

Input and eval only ever borrow (stream/buffer and a const AST respectively); the single ownership transfer in the whole flow is the one root moved into `*out_ast` on `RBC_PARSE_OK`, after which main is the owner and must call `rbc_ast_destroy` exactly once. On any unsuccessful parse that transfer never happens, so cleanup responsibility for partial construction stays entirely inside the parser, and a failed `*_take` constructor consumes nothing, meaning the parser still owns the children it passed in.

## 3. Build reasoning: objects, relocations, and incremental dependencies

### Q3.1 — Cross-translation-unit reference and relocation

**Timing — Prediction first:** Write the prediction before inspecting `build/normal/obj/parser.o` or its relocations. Complete the rest after the inspection.

**Question:**  
Before inspecting `build/normal/obj/parser.o`, predict whether a valid relocatable object can still contain a reference to a project function defined in another translation unit, and briefly explain why. Then choose one such project function from your inspection and use one symbol entry plus one related relocation entry to explain what information is still unresolved before final linking.

**Your prediction:**

Yes: a relocatable object is allowed to call functions defined in other translation units, because the assembler emits the call with a placeholder and records an undefined symbol plus a relocation entry telling the linker where to patch the real address later.

**Selected evidence:**

```text
<Paste only 1–3 symbol/relocation lines that support your explanation.>
```

**Interpretation:**

<!-- Explain the unresolved information and final-link role in 2–3 sentences. -->

### Q3.2 — Incremental rebuild prediction

**Timing — Prediction first:** Fill in both prediction cells before either `make -n -W` dry run. Complete the observed column afterward.

**Question:**  
Before running the two `make -n -W` experiments, fill in your prediction for a change to `src/lexer.c` and for a change to `include/rbc/lexer.h`: which translation units should be recompiled, and is relinking required? After the dry runs, add the observed selection and explain why the two cases differ, using the generated `.d` files as evidence.

**Prediction / observation table:**

| Changed file | Predicted recompilations / relink | Observed selection |
| --- | --- | --- |
| `src/lexer.c` |  |  |
| `include/rbc/lexer.h` |  |  |

**Explanation:**

<!-- In 2–3 sentences, use the generated .d files to explain why the two cases differ. -->

## 4. Testing boundaries and process behavior

### Q4.1 — What each evidence boundary can tell you

**Timing — After observation:** Answer after inspecting the representative tests and process contract.

**Question:**  
Choose one supplied Criterion test and one supplied CLI case. For each, state the boundary it exercises, the contract or returned/process state it checks, and what a failure at that boundary would tell you. Then explain why an `rbc` process status, a failed test, and a diagnostic finding from a tool such as UBSan or Memcheck are not interchangeable evidence.

**Your comparison:**

| Chosen supplied case | Boundary exercised | Contract / returned or process state checked | What a failure at this boundary would tell you |
| --- | --- | --- | --- |
| Criterion test:  |  |  |  |
| CLI case:  |  |  |  |

**Evidence-channel interpretation:**

<!-- In 2–3 sentences, explain why process status, a failed test, and a UBSan/Memcheck diagnostic are not interchangeable evidence. -->

## 5. Investigation 1 — bounded input recovery

### Q5.1 — Predict the two-call recovery state

**Timing — Before you run it:** Complete this prediction before adding or running the recovery regression.

**Question:**  
Before adding or running the recovery regression, use buffer capacity `4` and stream contents `"abcd\nxy\n"`. Predict the public result of the first two calls to `rbc_input_read_line`, including status, length, buffer contents, and where the stream should be positioned after each call **if the public contract is satisfied**. Add one sentence explaining the capacity boundary that makes the first logical line overlong.

**Your prediction:**

| Call | Predicted status | Predicted length | Predicted buffer contents | Predicted stream position after the call |
| --- | --- | --- | --- | --- |
| First call |  |  |  |  |
| Second call |  |  |  |  |

**Capacity-boundary justification:**

<!-- One sentence. -->

### Q5.2 — Diagnose the failed recovery postcondition

**Timing — After the observation:** Answer after the intended red regression, before editing production code.

**Question:**  
After the new regression reaches its intended red state, identify the public postcondition that the observation violates. What does the observed second call tell you about the stream state left by the first call, and which component owns that postcondition? Give the evidence that localizes the repair responsibility without describing the patch.

**Selected red-state evidence:**

```text
<Include only the short result/assertion summary needed for your diagnosis.>
```

**Diagnosis:**

<!-- Explain the violated postcondition, the stream-state implication, and repair responsibility in 2–4 sentences. Do not describe the patch. -->

### Q5.3 — Why the recovery regression matters

**Timing — After the repair:** Answer after the repaired unit regression and whole-program recovery check.

**Question:**  
Explain why your two-call recovery regression adds discriminatory value beyond the existing input tests. Then state what the 257-byte whole-program recovery check establishes at the executable boundary that the unit regression alone does not.

**Your answer:**

<!-- Answer in 3–4 sentences. Do not paste the test source or full command output. -->

## 6. Investigation 2 — additive associativity with GDB

### Q6.1 — Semantic prediction and competing hypotheses

**Timing — Before you run it:** Complete this before running the selected associativity case or starting the GDB investigation.

**Question:**  
Before running the associativity case, write the required grouping and result for `20 - 5 - 3`. Then give two plausible defect hypotheses from different components that could explain a mismatch, and name one kind of runtime observation at the evaluator boundary that would help distinguish those hypotheses.

**Required grouping / sketch:**

```text
<write or sketch your predicted grouping here>
```

**Predicted result:**

<!-- Write the predicted result here before running the case. -->

**Competing hypotheses:**

- <!-- Hypothesis 1 -->
- <!-- Hypothesis 2 -->

**Runtime observation that could distinguish them:**

<!-- One sentence. -->

### Q6.2 — Interpret the AST state you observed

**Timing — After the observation:** Answer after inspecting the selected GDB state and before editing the repair.

**Question:**  
From the runtime state you inspected at `rbc_eval`, reconstruct the observed AST grouping with a small sketch. Which of your hypotheses does that state support or rule out, and why? Based on that evidence, which component owns the repair?

**Observed AST/grouping sketch:**

```text
<draw the structure you observed here>
```

**Selected field values, if needed:**

```text
<Include only the few values needed to support the sketch. Do not paste the debugger transcript.>
```

**Interpretation:**

<!-- In 2–4 sentences, connect the observed structure to your hypotheses and repair responsibility. -->

### Q6.3 — Executable regression and debugger limits

**Timing — After the repair:** Answer after the CLI regression is green.

**Question:**  
Your permanent CLI regression uses a mixed additive expression. What public language contract does it encode, why does it distinguish the original defective behavior from the repaired behavior, and what does the passing case establish after repair? Also state one limitation of the GDB observation you used during diagnosis.

**Your answer:**

<!-- Answer in 3–5 sentences. Focus on the public contract, discrimination, what the passing case establishes, and one limit of the GDB observation. -->

## 7. Investigation 3 — signed multiplication with sanitizers

### Q7.1 — Separate math, API behavior, C execution, and sanitizer evidence

**Timing — Before you run it:** Complete this before running the sanitizer configuration.

**Question:**  
Before running the sanitizer configuration on `tests/fixtures/sanitize.in`, separate these four questions: (1) is the mathematical product representable in `int64_t`; (2) what `rbc_eval` status does the public contract require if it is not; (3) may a C implementation execute an overflowing signed multiplication first and decide what status to return afterward; and (4) which sanitizer class is relevant to that C-language issue? Briefly justify each answer.

**Your prediction:**

| Question | Answer | Brief justification |
| --- | --- | --- |
| Mathematical product representable in `int64_t`? |  |  |
| Required `rbc_eval` status if it is not representable? |  |  |
| May the implementation execute an overflowing signed multiplication first and decide the status afterward? |  |  |
| Sanitizer class relevant to that C-language issue? |  |  |

### Q7.2 — Interpret the UBSan finding

**Timing — After the observation:** Answer after the intended diagnostic-red sanitizer run and before the production repair.

**Question:**  
Include only the smallest UBSan excerpt needed to identify the exercised operation and source context. Explain what C-language failure it reports, how that differs from the calculator’s application-level overflow condition, and what this evidence tells you that an ordinary unsanitized `RBC_EVAL_OVERFLOW` result would not.

**Selected UBSan evidence:**

```text
<Paste only the 1–3 diagnostic lines needed to identify the operation and source context.>
```

**Interpretation:**

<!-- Explain the C-language failure, how it differs from the application-level overflow condition, and what UBSan adds in 3–4 sentences. -->

### Q7.3 — API contract coverage versus sanitizer discrimination

**Timing — After the repair:** Answer after the public-API multiplication cases and direct sanitized post-repair checks.

**Question:**  
Compare the two new public-API multiplication contract-coverage tests with your post-repair sanitized run. What contract does each type of evidence cover, and what changed between the initial diagnostic-red path and the repaired path? State one claim the repaired sanitizer run does **not** justify.

**Your answer:**

<!-- Answer in 3–5 sentences. Distinguish public-contract evidence from sanitizer evidence and include one limit of the repaired sanitizer run. -->

## 8. Investigation 4 — deterministic allocation failure and Memcheck

### Q8.1 — Predict the selected partial ownership state

**Timing — Before you run it:** Complete this ownership prediction before running the resource probe or Memcheck.

**Question:**  
Before running Memcheck, use the resource probe’s configured failure point to make a compact partial-state table. Identify the selected allocation attempt; for each relevant AST resource acquired before that failure, state who owns it at the failure point; state whether the failed constructor call transfers ownership; predict the parser status and returned-root state; and state the cleanup condition that must hold before an unsuccessful parse returns.

**Selected allocation attempt:**

<!-- Identify the selected attempt. -->

**Partial ownership state:**

| Resource / object | Acquired before failure? | Current owner at the failure point | Ownership transferred? | Cleanup obligation |
| --- | --- | --- | --- | --- |
|  |  |  |  |  |

<!-- Add or remove blank rows as needed for the partial state you predict. -->

**Predicted parser status and returned-root state:**

<!-- State the predicted public result. -->

**Cleanup condition before unsuccessful return:**

<!-- State the required cleanup condition, then add 1–2 sentences if needed to explain the table. -->

### Q8.2 — Use Memcheck provenance to diagnose ownership

**Timing — After the observation:** Answer after the intended diagnostic-red Memcheck run and before editing the cleanup repair.

**Question:**  
Include only the Memcheck lines needed to identify the still-live allocation and its acquisition provenance. Using those lines plus the `*_take` and parser contracts, explain the acquire → attempted transfer → failure → retained ownership chain. Which component therefore retains the cleanup obligation, and what evidence rules out changing constructor ownership semantics as the repair?

**Selected Memcheck evidence:**

```text
<Paste only the 1–4 lines needed to identify the still-live allocation and its acquisition provenance.>
```

**Ownership/cleanup interpretation:**

<!-- In 3–5 sentences, trace acquire → attempted transfer → failure → retained ownership and justify the responsible component without naming the patch. -->

### Q8.3 — Why the deterministic cleanup regression discriminates

**Timing — After the repair:** Answer after the parser regression and repaired Valgrind check are green.

**Question:**  
Why do the allocation-attempt count and live-allocation count make your parser regression discriminate the selected failure path instead of merely checking the outward error status? What additional evidence does the repaired `make valgrind` result provide, and what does a clean result on this selected path **not** prove?

**Your answer:**

<!-- Answer in 3–4 sentences. Explain the two counters, what Valgrind adds, and the limit of one clean deterministic path. -->

## 9. Final verification and authored-change review

### Q9.1 — Final checks, claims, and limits

**Timing — During final verification:** Complete this only after all required final checks have been run.

**Question:**  
Complete a compact matrix for the final checks you actually ran: warning-clean normal build, full normal tests, bounded-input whole-program recovery, normal whole-program overflow behavior, sanitized tests/exercised runs, deterministic Valgrind run, and focused authored-change review. For each check, state the engineering claim it supports and one important limit. End with 1–2 sentences explaining why these checks are complementary rather than interchangeable.

**Verification matrix:**

| Check | Claim supported | Important limit |
| --- | --- | --- |
| Warning-clean normal build |  |  |
| Full normal tests |  |  |
| Bounded-input whole-program recovery |  |  |
| Normal whole-program overflow behavior |  |  |
| Sanitized tests / exercised runs |  |  |
| Deterministic Valgrind run |  |  |
| Focused authored-change review |  |  |

**Why these checks are complementary:**

<!-- End with 1–2 sentences. Do not paste the final logs. -->

