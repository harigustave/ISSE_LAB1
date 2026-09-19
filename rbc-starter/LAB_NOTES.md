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
$ nm build/normal/obj/parser.o
                 U rbc_ast_create_integer
$ readelf -rW build/normal/obj/parser.o
00000000000000f5  0000003f00000004 R_X86_64_PLT32  0000000000000000 rbc_ast_create_integer - 4
```

**Interpretation:**

parser.o already holds the encoded call instruction, but the symbol table marks `rbc_ast_create_integer` as U (used, not defined here), and its 4-byte call operand at .text offset 0xf5 is only a placeholder. The `R_X86_64_PLT32` relocation records exactly where that operand sits and which symbol it must reach, so what is still unresolved is the final address of the definition (it lives in ast.o). At final link, after the linker lays out all sections and resolves the symbol, it patches the recorded location with the correct PC-relative displacement.

### Q3.2 — Incremental rebuild prediction

**Timing — Prediction first:** Fill in both prediction cells before either `make -n -W` dry run. Complete the observed column afterward.

**Question:**  
Before running the two `make -n -W` experiments, fill in your prediction for a change to `src/lexer.c` and for a change to `include/rbc/lexer.h`: which translation units should be recompiled, and is relinking required? After the dry runs, add the observed selection and explain why the two cases differ, using the generated `.d` files as evidence.

**Prediction / observation table:**

| Changed file | Predicted recompilations / relink | Observed selection |
| --- | --- | --- |
| `src/lexer.c` | Only lexer.o recompiles (the .c is a prerequisite of just its own object), then rbc relinks |Only `-c src/lexer.c`, then relink of build/normal/bin/rbc |
| `include/rbc/lexer.h` | lexer.o and parser.o recompile (both TUs include rbc/lexer.h, per their generated .d files), then rbc relinks | `-c src/lexer.c` and `-c src/parser.c`, then relink of build/normal/bin/rbc |

**Explanation:**

The generated .d files record each object's true prerequisites: `lexer.d` lists `build/normal/obj/lexer.o: src/lexer.c include/rbc/lexer.h`, while `parser.d` lists `parser.o: src/parser.c include/rbc/parser.h include/rbc/ast.h include/rbc/lexer.h`. A newer `src/lexer.c` therefore invalidates only lexer.o, but a newer `include/rbc/lexer.h` invalidates every object whose .d names it (lexer.o and parser.o); main.o does not include lexer.h, so it is untouched. In both cases the executable depends on its objects, so a relink follows the recompilations, and no `make clean` is ever needed for a correct incremental build.

## 4. Testing boundaries and process behavior

### Q4.1 — What each evidence boundary can tell you

**Timing — After observation:** Answer after inspecting the representative tests and process contract.

**Question:**  
Choose one supplied Criterion test and one supplied CLI case. For each, state the boundary it exercises, the contract or returned/process state it checks, and what a failure at that boundary would tell you. Then explain why an `rbc` process status, a failed test, and a diagnostic finding from a tool such as UBSan or Memcheck are not interchangeable evidence.

**Your comparison:**

| Chosen supplied case | Boundary exercised | Contract / returned or process state checked | What a failure at this boundary would tell you |
| --- | --- | --- | --- |
| Criterion test: `Test(input, one_extra_payload_byte_is_too_long)` | Public C module interface: an in-process call to `rbc_input_read_line` | With capacity 4 and stream `"abcd\n"`, the returned struct must be `RBC_INPUT_TOO_LONG` with `length == 0` and an empty NUL-terminated buffer, per input.h | The input module itself violates its stated public contract; the defect is localized to that component, independent of parser/eval/main |
| CLI case: "evaluation recovery" (`8 / 0\n9\n`) | External process boundary: argv + stdin in, stdout + stderr + exit status out | stdout is exactly `9\n`, stderr is exactly `rbc: line 1: division by zero\n`, and the process exits with status 1 (recoverable line error observed before normal EOF) | The integrated whole-program behavior visible to a user is wrong somewhere along input -> parse -> eval -> report, but it does not localize which component is responsible |

**Evidence-channel interpretation:**

A process status is the application's own public verdict, and observing it (I ran the `8 / 0` case: status 1, stdout `9`, the diagnostic on stderr) says nothing about how the result was computed. A failed test says a specific stated contract at a specific boundary was violated, which localizes responsibility to whatever that boundary isolates. A UBSan or Memcheck finding reports an invalid C-language operation or resource state that can occur even while statuses and tests all look right, so none of the three channels can substitute for another; they answer different questions.

## 5. Investigation 1 — bounded input recovery

### Q5.1 — Predict the two-call recovery state

**Timing — Before you run it:** Complete this prediction before adding or running the recovery regression.

**Question:**  
Before adding or running the recovery regression, use buffer capacity `4` and stream contents `"abcd\nxy\n"`. Predict the public result of the first two calls to `rbc_input_read_line`, including status, length, buffer contents, and where the stream should be positioned after each call **if the public contract is satisfied**. Add one sentence explaining the capacity boundary that makes the first logical line overlong.

**Your prediction:**

| Call | Predicted status | Predicted length | Predicted buffer contents | Predicted stream position after the call |
| --- | --- | --- | --- | --- |
| First call | `RBC_INPUT_TOO_LONG` | 0 | `""` (empty, `buffer[0] == '\0'`) | Just past the newline of `abcd\n`, i.e. at the `x` that starts the second logical line |
| Second call | `RBC_INPUT_LINE` | 2 | `"xy"` (newline consumed but excluded) | Just past the newline of `xy\n`, i.e. at EOF |

**Capacity-boundary justification:**

With capacity 4 at most capacity - 1 = 3 payload bytes fit (the buffer also needs the terminating NUL), so the 4-byte payload `abcd` makes the first logical line overlong, and the contract requires the whole rejected line, through its newline, to be discarded before the next call begins.

### Q5.2 — Diagnose the failed recovery postcondition

**Timing — After the observation:** Answer after the intended red regression, before editing production code.

**Question:**  
After the new regression reaches its intended red state, identify the public postcondition that the observation violates. What does the observed second call tell you about the stream state left by the first call, and which component owns that postcondition? Give the evidence that localizes the repair responsibility without describing the patch.

**Selected red-state evidence:**

```text
[FAIL] input::overlong_line_recovery_across_calls
tests/unit/test_input.c:104: Assertion Failed  (second.length == 2)
second call observed: status RBC_INPUT_LINE, length 0, buffer "" (expected "xy")
```

**Diagnosis:**

The violated public postcondition is input.h's requirement that `RBC_INPUT_TOO_LONG` means "the complete overlong logical line has been discarded through newline/EOF". The second call returning an empty `RBC_INPUT_LINE` instead of `"xy"` shows the first call left the stream still inside the rejected line, positioned at that line's own terminating newline, which the second call then consumed as a spurious empty line. The postcondition belongs to `rbc_input_read_line` alone: the failing test reaches the defect through only that one public function, with no parser, evaluator, or main involved, so the repair responsibility is localized to the input component (src/input.c), and main.c must not compensate for it.

### Q5.3 — Why the recovery regression matters

**Timing — After the repair:** Answer after the repaired unit regression and whole-program recovery check.

**Question:**  
Explain why your two-call recovery regression adds discriminatory value beyond the existing input tests. Then state what the 257-byte whole-program recovery check establishes at the executable boundary that the unit regression alone does not.

**Your answer:**

The existing input tests each make a single call, so they prove overlong detection (first-call status, length, empty buffer) but say nothing about the stream state a TOO_LONG return leaves behind; my regression's second call is what turns the "discarded through newline" postcondition into a checked, permanent invariant, and it stayed red on the starter while every single-call test stayed green. The 257-byte whole-program check then establishes the same recovery at the process boundary with the real RBC_LINE_CAPACITY of 256: stdout `5` only, the line-1 too-long diagnostic on stderr, and exit status 1, proving main's loop, line accounting, and status reporting integrate correctly with the repaired module. The unit regression alone could not show that no fragment of the rejected line reaches the parser as a separate expression in the shipped executable; the executable check alone could not localize a failure to the input module. Together they cover both the contract and its integration.

## 6. Investigation 2 — additive associativity with GDB

### Q6.1 — Semantic prediction and competing hypotheses

**Timing — Before you run it:** Complete this before running the selected associativity case or starting the GDB investigation.

**Question:**  
Before running the associativity case, write the required grouping and result for `20 - 5 - 3`. Then give two plausible defect hypotheses from different components that could explain a mismatch, and name one kind of runtime observation at the evaluator boundary that would help distinguish those hypotheses.

**Required grouping / sketch:**

```text
Left-associative additive level: 20 - 5 - 3  ==  (20 - 5) - 3

        (-)
       /   \
     (-)    3
    /   \
  20     5
```

**Predicted result:**

12 (whereas a wrongly right-associative grouping 20 - (5 - 3) would print 18).

**Competing hypotheses:**

- Hypothesis 1 (parser/representation): the parser builds a right-associative tree at the additive level, so a structurally wrong AST like 20 - (5 - 3) reaches a correct evaluator.
- Hypothesis 2 (evaluator): the parser builds the correct left-associative AST, but rbc_eval computes the subtraction wrongly (for example swaps operands or mis-handles chained nodes), so a correct tree produces a wrong value.

**Runtime observation that could distinguish them:**

Inspecting the AST root that actually reaches rbc_eval for this execution: a root whose right child is the subtree (5 - 3) confirms hypothesis 1, while a correctly grouped root ((20 - 5) left, 3 right) with a wrong printed value confirms hypothesis 2.

### Q6.2 — Interpret the AST state you observed

**Timing — After the observation:** Answer after inspecting the selected GDB state and before editing the repair.

**Question:**  
From the runtime state you inspected at `rbc_eval`, reconstruct the observed AST grouping with a small sketch. Which of your hypotheses does that state support or rule out, and why? Based on that evidence, which component owns the repair?

**Observed AST/grouping sketch:**

```text
Observed at rbc_eval for "20 - 5 - 3":   20 - (5 - 3)   [right-associative]

        (-)                (printed result: 18)
       /   \
     20     (-)
           /   \
          5     3
```

**Selected field values, if needed:**

```text
*root                    = {kind = RBC_AST_BINARY, binary = {op = RBC_BINARY_SUBTRACT, ...}}
*root->data.binary.left  = {kind = RBC_AST_INTEGER, integer = 20}
*root->data.binary.right = {kind = RBC_AST_BINARY, binary = {op = RBC_BINARY_SUBTRACT, ...}}
```

**Interpretation:**

The root's right child is itself a subtraction while the left child is the bare integer 20, so the structure reaching the evaluator is already the wrongly grouped 20 - (5 - 3); this supports the parser-representation hypothesis and rules out the evaluator hypothesis, since rbc_eval merely borrows whatever tree it is given and 18 is the faithful value of this tree. The defect therefore lives where the additive-level tree shape is constructed, and the repair belongs to src/parser.c; the evaluator must not be changed to compensate for a representation defect established elsewhere.

### Q6.3 — Executable regression and debugger limits

**Timing — After the repair:** Answer after the CLI regression is green.

**Question:**  
Your permanent CLI regression uses a mixed additive expression. What public language contract does it encode, why does it distinguish the original defective behavior from the repaired behavior, and what does the passing case establish after repair? Also state one limitation of the GDB observation you used during diagnosis.

**Your answer:**

The CLI case encodes the public language contract that `+` and `-` share one precedence level and associate left, so `10 - 2 + 3` must print `11` with empty stderr and status 0. A mixed chain is what makes it discriminating: the defective right-associative grouping yields `10 - (2 + 3) = 5` while the repaired grouping yields `(10 - 2) + 3 = 11`, whereas a chain of only `+` produces the same value under both groupings and both runs exit 0, so only the printed value separates them. Passing now establishes that the shipped executable, end to end through stdin, parser, evaluator, and stdout, honors that associativity contract permanently, and the case will go red if the tree shape ever regresses. One limit of the GDB observation: it examined the AST for one expression on one execution, so by itself it proves nothing about other operator mixes, other inputs, or that the repair preserved everything else; the full green suite carries that weight.

## 7. Investigation 3 — signed multiplication with sanitizers

### Q7.1 — Separate math, API behavior, C execution, and sanitizer evidence

**Timing — Before you run it:** Complete this before running the sanitizer configuration.

**Question:**  
Before running the sanitizer configuration on `tests/fixtures/sanitize.in`, separate these four questions: (1) is the mathematical product representable in `int64_t`; (2) what `rbc_eval` status does the public contract require if it is not; (3) may a C implementation execute an overflowing signed multiplication first and decide what status to return afterward; and (4) which sanitizer class is relevant to that C-language issue? Briefly justify each answer.

**Your prediction:**

| Question | Answer | Brief justification |
| --- | --- | --- |
| Mathematical product representable in `int64_t`? | No | 3037000500^2 = 9223372037000250000, which exceeds INT64_MAX = 9223372036854775807 (3037000500 is just above the integer square root of INT64_MAX) |
| Required `rbc_eval` status if it is not representable? | `RBC_EVAL_OVERFLOW`, with `*out_value` unchanged | eval.h: on every evaluation failure the output variable is untouched; main then reports the line-level integer overflow diagnostic and the process uses recoverable status 1 |
| May the implementation execute an overflowing signed multiplication first and decide the status afterward? | No | Signed integer overflow is undefined behavior in C, not a wrapping operation that can be inspected afterward; the implementation must establish representability before executing the multiply, so compute-then-check is invalid even when the outward status looks right |
| Sanitizer class relevant to that C-language issue? | UBSan (UndefinedBehaviorSanitizer), signed-integer-overflow check | UBSan diagnoses invalid C operations on the exercised path; ASan targets memory-safety classes and is not the tool for arithmetic UB |

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

