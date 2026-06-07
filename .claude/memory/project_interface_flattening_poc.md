---
name: project-interface-flattening-poc
description: poc/interface-flattening branch — auto-flattens parent interface methods when the parent extension isn't in external-dependencies
metadata:
  type: project
---

Branch `poc/interface-flattening` adds a fix so a Zephir-authored interface
that `extends` an interface from an *optional* compiled extension (e.g.
`Psr\Log\LoggerInterface` from `php-psr`) no longer emits a hard
`zend_class_implements()` call against that extension's CE when that CE isn't
guaranteed to exist at runtime. When the parent interface isn't declared in
`external-dependencies` (no `.zep` sources to reference),
`CompilerFile::checkDependencies()` reflects the parent and decides whether to
keep the CE reference or flatten its methods into the child interface's
method table via `shouldFlattenInterface()`.

**The correct rule (confirmed by the user, then empirically verified via a
diagnostic probe — see "Critical correction" below): flatten if and only if
the parent interface is provided purely by Composer/userland PHP and is *not*
backed by a C extension.** The discriminator that actually implements this is
`ReflectionClass::isInternal()`:
- `true` → the CE was registered by compiled C code — either a genuine C
  extension (`php-psr`, `ds`, Phalcon-as-extension, etc.) *or* one of PHP's
  own core/SPL/Reflection/etc. interfaces (themselves part of core
  extensions). Either way the CE is guaranteed present at runtime, so Zephir
  keeps `zend_class_implements()` — this one check elegantly covers BOTH "PHP
  core interface" and "C-extension interface" without any namespace check or
  hardcoded extension allowlist.
- `false` → only userland PHP source declared it (e.g. loaded via Composer's
  autoloader from `psr/log`), no C extension backs it, no guarantee its CE
  exists at runtime → flatten.

```php
private static function shouldFlattenInterface(string $fqn): bool
{
    try {
        $reflection = new ReflectionClass(ltrim($fqn, '\\'));
    } catch (ReflectionException $e) {
        return false;
    }
    return !$reflection->isInternal();
}
```

### Critical correction — the FIRST heuristic was wrong, and so was my analysis

The originally-implemented `shouldFlattenInterface()` (and my own initial
defense of it) checked the FQN's namespace (`strpos($fqn, '\\')`) plus a
hardcoded core-extension allowlist (`['Core','standard','SPL','date','pcre',
'json','Reflection']`) via `getExtensionName()`. **This is wrong** — it
flattens ANY namespaced interface whose extension name isn't on the list,
including ones genuinely backed by a C extension (e.g. `php-psr`'s extension
name is `'psr'`, not on the list). The user caught this by noticing the two
POC images compiled to *byte-identical* C ("They shouldn't be identical,
though. The extension version is supposed to directly extend the extension"),
which is impossible if detection were correct — Mode A (`php-psr` loaded)
should keep `zend_class_implements()`.

I initially also wrongly concluded that `isBundledInterface()`
(`interface_exists($fqn, false)`) — the only "is this CE present" signal the
existing code used — could never distinguish a Composer-only interface from a
C-extension one, because **Zephir itself depends on `monolog/monolog`, which
`implements Psr\Log\LoggerInterface`**, so Composer's autoloader loads that
interface during Zephir's own bootstrap (via `vendor/bin/zephir`'s Composer
bin-proxy, which loads the *consuming project's* `vendor/autoload.php`,
registering `Composer\Autoload\ClassLoader::loadClass`) regardless of whether
`php-psr` is present. A `ZEPHIR_PROBE_AUTOLOAD` diagnostic (temporarily added
to `checkDependencies()`, since reverted) empirically proved
`isBundledInterface()` returns `true` in BOTH conditions — but `isInternal()`
correctly returns `true` only when `php-psr` backs the CE, `false` when only
`psr/log` does. That's the fix.

A real defect was found and fixed during verification (commit `bb9a4f34b`,
"Preserve parameter types and defaults when flattening parent interface
methods"): the initial flattening reused `Definition::buildFromReflection()`
output, which always sets `data-type => 'variable'` and stores raw PHP
default values — producing untyped arg-info and non-fatal `Declaration ...
must be compatible` notices against the implementation's properly-typed
signatures. The fix re-reflects the parent interface fresh with
`ReflectionMethod`/`ReflectionParameter` and builds Zephir IR-format
parameter arrays directly (mapping builtin types to `Types` constants, class
types to `cast`, PHP defaults to `['type' => ..., 'value' => ...]`
structures) via three new helpers: `buildFlattenedParameters`,
`buildFlattenedParameter`, `buildFlattenedParameterDefault`.

### Gap A and Gap B — documented findings from rubber-duck review + empirical testing

A rubber-duck review of the `isInternal()` fix surfaced two gaps. The user
asked both to be documented and for Gap A to be empirically tested
("What happens now if a class directly implements an interface that is in an
extension vs one that is only included by composer").

**Gap A — `class Foo implements <bundled-but-not-external-dep interface>` is
UNGUARDED, and the failure mode is a SEGFAULT, not a warning.**

The new `isInternal()`-based flatten branch in `checkDependencies()` is gated
on `$classDefinition->isInterface()` — it only ever fires for
`interface X extends Y`. A `class Foo implements Y` takes the exact same
`$interfaceDefinitions[$interface] = $parentDef;` path the code has *always*
taken, unconditionally emitting `zend_class_implements()` against the
reflected CE regardless of whether a C extension backs it.

Empirical test: added `class LoggerDirect implements \Psr\Log\LoggerInterface`
(bypassing `Poc\Logger` entirely) to the POC and compiled it under the fixed
branch in both Mode A (`php-psr` loaded) and Mode B (Composer-only,
`psr/log`). Findings:
- **No "Cannot locate class" warning at compile time in either mode** —
  `isBundledInterface()` returns `true` in both (the same
  `monolog/monolog`-autoloads-`Psr\Log\LoggerInterface` blind spot documented
  above), so the warning branch (`else` arm, ~lines 259–271) is never reached.
- Generated C is structurally identical in both modes — only the referenced CE
  symbol differs:
  - Mode A: `zend_class_implements(poc_loggerdirect_ce, 1,
    zephir_get_internal_ce(SL("psrext\\log\\loggerinterface")))` — safe; `php-psr`
    guarantees the CE exists.
  - Mode B: `zend_class_implements(poc_loggerdirect_ce, 1,
    zephir_get_internal_ce(SL("psr\\log\\loggerinterface")))` — same pattern,
    but the CE has no C-extension backing.
- **Runtime in Mode B: loading the compiled `.so` segfaults.** Reproduced
  directly: `Segmentation fault (core dumped)` on `php -m` / `php -r '...'` /
  anything that loads the extension and triggers `MINIT`.

  **Root cause is subtler than "no NULL guard"** (a follow-up feasibility
  agent dug into this — see below): `zephir_get_internal_ce()`
  (`kernel/main.c:368-378`, byte-identical on `master` and this branch since
  2015's `5e613a8c`) ALREADY guards against a missing CE:
  ```c
  if ((temp_ce = zend_hash_str_find_ptr(CG(class_table), class_name, class_name_len)) == NULL) {
      zend_error(E_ERROR, "Class '%s' not found", class_name);
      return NULL;
  }
  ```
  `zend_error(E_ERROR, ...)` is supposed to `zend_bailout()` (longjmp) into a
  clean fatal-error abort — `zend_class_implements()` should never receive the
  `NULL`. The segfault we observe therefore most likely happens *inside*
  `zend_error(E_ERROR, ...)` itself: at `MINIT` time, the bailout jmpbuf /
  executor context it longjmps to may not yet be established for this SAPI —
  so the "guard" crashes before it can produce its clean fatal error. In other
  words: **the safety net exists, but the net itself isn't safe to use this
  early in the module lifecycle.**

**Confirmed pre-existing, not introduced by this fix**: the
`getImplementedInterfaces()` loop's `class`-handling branch is **byte-identical**
between `master` (`c78e4f35f`) and `poc/interface-flattening` — this exact
unconditional `zend_class_implements()` reference, and this exact segfault
potential, has always existed for any class implementing a bundled interface
that isn't declared in `external-dependencies`. The `isInternal()` fix
deliberately doesn't extend to `class ... implements`, because flattening
fundamentally requires generating method *implementations* (bodies), not
inlining abstract signatures — a structurally different, much larger problem.

So the user's framing — "isn't that already an existing gap... if it doesn't
exist at runtime it'll throw a warning" — is **half right**: it IS confirmed
pre-existing (not a regression of this branch). But the actual failure is
harsher than "a warning": it's an **unconditional, undiagnosed segmentation
fault** — silent at compile time (the `isBundledInterface()` blind spot hides
it from the existing warning path) and a raw NULL-pointer crash at runtime
(no PHP-level exception, no log, no graceful degradation).

**Gap B — `isInternal()` reflects compile-time, not runtime, environment.**

If someone compiles WITH `php-psr` loaded (`isInternal()` ⇒ `true`, keeps
`zend_class_implements()`) but deploys/runs WITHOUT it, the exact same
NULL-CE segfault as Gap A occurs. The inverse (compile without, run with) is
safe but "disconnected" (flattened, no formal `instanceof` relationship).
This is inherent to any AOT-reflection approach — including Zephir's own
`external-dependencies` mechanism, which has always assumed "what's present at
compile time will be present at runtime." The fix doesn't introduce this risk;
if anything it shrinks the blast radius, because the previously-*always*-unsafe
interface-flattening case is now safe whenever compile-time and runtime
environments match (the overwhelmingly common case for a properly built
distributable extension).

**User's verbatim opinion on both:**
> Gap B is 100% to be expected. I don't know if it'd be possible to throw a
> PHP level Runtime Exception automatically in that case, but otherwise leave
> it as is. Regarding Gap A, isn't that already an existing Gap? That is to
> say that if today, someone were to extend an interface that is already
> loaded, the expectation is that it exists and will exist at run time, and if
> it doesn't, it'll throw a warning that it doesn't exist at runtime.

Verdict: Gap A is confirmed pre-existing (not a regression), but manifests as
a segfault rather than the graceful warning the user expected — worth knowing,
not worth blocking this POC on. Gap B is accepted as inherent/expected by the
user.

**Follow-up feasibility check (lightweight, read-only — user explicitly said
"don't spend too much effort, out of scope for this POC"):** dispatched a
scout to assess whether Zephir could raise a catchable PHP `RuntimeException`
instead of segfaulting. Findings:
- A NULL guard already exists in `zephir_get_internal_ce()` (`zend_error
  (E_ERROR, "Class '%s' not found", ...)`) — see the corrected root-cause
  analysis above; the segfault is most likely the guard's own `zend_error`
  call crashing at `MINIT` (no bailout context yet), not a missing check.
- **A true catchable PHP-level exception is NOT feasible at `MINIT`** —
  `zend_throw_exception` needs an active execution context
  (`EG(current_execute_data)`, exception machinery) that doesn't exist during
  module initialization. This is the hard technical ceiling on the user's
  "PHP level Runtime Exception" idea — it can't be done at the point where
  `zend_class_implements()` runs.
- The achievable improvement is narrower: harden the *existing*
  `zend_error(E_ERROR)` guard in `zephir_get_internal_ce` (`kernel/main.c`)
  into a **guaranteed-clean fatal abort** with a clearer message ("Class 'X'
  not found - is the providing extension loaded?"). A `kernel/`-level fix
  would benefit every already-compiled extension linking against the shared
  runtime, with no recompilation — better leverage than a codegen
  (`Definition.php`) change, which only helps newly-compiled code.
- Prior art exists for runtime (not MINIT) exception throwing:
  `kernel/exception.c`'s `zephir_throw_exception_string`/`_format` wrap
  `zend_throw_exception_object` — usable in compiled method bodies, not
  module bootstrap.

**Recommendation** (scout's, concurred): worth a small future patch to make
`zephir_get_internal_ce`'s existing `zend_error(E_ERROR)` path abort cleanly
instead of crashing — but a catchable-`RuntimeException` fix is off the table
at `MINIT` and shouldn't be pursued. This closes the loop on the user's
"could it throw a Runtime Exception?" question for both Gap A and Gap B:
the honest answer is "not at this point in the lifecycle — the best
achievable fix is a clean fatal, not a catchable exception."

**Why:** PSR interfaces are the canonical case — they exist both as the
`php-psr` C extension and the `psr/log` Composer package, and a `.so`
extension can't safely reference an optional extension's CE if it isn't
loaded. This is the same friction Phalcon-based Zephir extensions hit.

**How to apply:** This branch and its fix are verified end-to-end by the
standalone POC repo — see [[reference-zephir-interface-poc]]. If asked about
interface flattening, optional-extension CE references, the
`Declaration ... must be compatible` notice class of bug in Zephir-generated
arg-info, or — especially — about *how to detect whether a reflected parent
interface's CE is safe to reference directly* — the answer is
`ReflectionClass::isInternal()`, not a namespace/extension-name heuristic, and
not `isBundledInterface()`/`interface_exists()` alone (not yet merged to
`master` as of 2026-06-07).

### The Third Gap, the case-sensitivity bug, and a debunked "upgrade landmine" theory

A deeper investigation (triggered by the user noting `phalcon-shared` has the
exact "interface extends external-dependency interface" scenario — e.g.
`TwistersFury\Phalcon\Shared\Di\Interfaces\InitializationAware extends
Phalcon\Di\InitializationAwareInterface` with `"external-dependencies":
{"Phalcon": "/opt/cphalcon"}` — yet has *never* hit the third gap) surfaced a
**separate, pre-existing bug that masks the third gap in all real-world
Phalcon-convention projects**:

`Compiler::loadExternalClass()` (pre-0.23.0) computed the `.zep` path via
`strtolower(str_replace('\\', DIRECTORY_SEPARATOR, $className))` — fully
lowercasing the FQN. Real Phalcon `.zep` trees use a **hybrid casing
convention**: lowercase top-level namespace directory + PascalCase sub-paths
(`/opt/cphalcon/phalcon/Di/InitializationAwareInterface.zep`). The
all-lowercase computed path (`phalcon/di/initializationawareinterface.zep`)
never matches on a case-sensitive filesystem → `file_exists()` → `false` →
`loadExternalClass()` silently returns `false` → `isInterface()` returns
`false` → silent fallthrough to the working reflection-based path
(`zend_class_implements(..., zephir_get_internal_ce(SL("phalcon\\di\\...")))`).
**`external-dependencies` has effectively never engaged for any Phalcon
sub-namespaced class in `phalcon-shared`, on any version** — confirmed via
direct extraction of generated C from the live
`twistersfury/phalcon-shared:8.3-development` image.

This bug WAS independently identified and fixed upstream — commit `37281e08d`
"Add case check for external dependency classes" (PR #2556, merged via
`317ec27e4`, **shipped in Zephir 0.23.0**) replaced the naive lowercasing with
`locateExternalClassFile()`, which tries two candidates: the FQN's exact
casing, or fully lowercased.

**Hypothesis formed (later disproven): an "upgrade landmine".** Reasoning:
if 0.23.0's case-fix made `external-dependencies` finally engage for
`Phalcon\Di\InitializationAwareInterface`, that would route
`InitializationAware.zep` through `generateClassHeadersPost()` — whose
`'class' === $classDefinition->getType()` guard (confirmed still present,
byte-identical, at the `0.23.0` tag) never emits `#include`/`extern` for
`interface` definitions — causing a brand-new "use of undeclared identifier"
**C compile failure** that is impossible today, the moment `phalcon-shared`
upgrades its Zephir toolchain.

**Empirically tested and DISPROVEN** (2026-06-07): upgraded a live clone of
`twistersfury/phalcon-shared:8.3-development` to Zephir 0.23.0
(`composer require phalcon/zephir:0.23.0`), ran `zephir fullclean && zephir
generate --export-classes && make` against the real `/opt/cphalcon` sources.
**Build succeeded (exit 0)**, extension loaded, `instanceof
Phalcon\Di\InitializationAwareInterface` still `true`. Generated C was
*unchanged* — still `zend_class_implements(...,
zephir_get_internal_ce(SL("phalcon\\di\\initializationawareinterface")))`.

**Root cause of why the landmine theory failed**: `locateExternalClassFile()`
in 0.23.0 only tries **two** casings — the FQN exactly as written
(`Phalcon/Di/InitializationAwareInterface`) or fully lowercased
(`phalcon/di/initializationawareinterface`). Neither matches Phalcon's actual
**third, hybrid casing** (`phalcon/Di/InitializationAwareInterface` — lowercase
root + PascalCase sub-path). PR #2556's fix targets a different layout
convention (e.g. PSR-4 projects whose directory casing matches their namespace
1:1, or projects using full-lowercase paths) and does not cover Phalcon's
actual repo layout.

**Conclusion**: there is no upgrade landmine. `external-dependencies` remains
permanently dormant for `Phalcon\*` classes in `phalcon-shared` — at 0.21.0
*and* 0.23.0+ — so the third gap (`generateClassHeadersPost`'s `class`-only
include guard) stays silently masked indefinitely under real-world Phalcon
casing conventions, regardless of the case-sensitivity fix. It remains a real,
fixable upstream defect (worth a small PR extending the guard to cover
`'interface'` too, and arguably `locateExternalClassFile` could try a third
"lowercase-root-only" candidate) — but it is *not* an active risk for anyone
upgrading Zephir today.

### The third gap, fully unmasked: empirical proof via a hybrid-casing patch, plus a SECOND shielded defect

Since the casing bug is the *only* thing keeping the third gap dormant, the
natural follow-up was: patch the casing bug in isolation (a minimal third
candidate in `locateExternalClassFile()` — lowercase only the root namespace
segment, preserve the rest) on a branch (`fix/external-class-hybrid-casing`,
commit `6c71eb8ac`, based on `upstream/development` @ 0.23.0) and re-run
`phalcon-shared`'s build against it. This *should* make `external-dependencies`
finally engage for `Phalcon\*` and surface the third gap directly.

**It does — but a SECOND, more pervasive shielded defect fires first and
must be worked around to even reach the third gap:**

**Shielded defect #2 — the casing bug also suppresses strict static
property-validation (`classDoesNotHaveProperty`).**
`Expression\PropertyAccess::compile()` (`src/Expression/PropertyAccess.php:117`)
gates `checkClassHasProperty()` (`Traits\VariablesTrait.php:51-65`,
`CompilerException::classDoesNotHaveProperty`) behind
`$compiler->isClass($classType)`. Before the casing fix, `isClass()`
(`Compiler.php:1444`) returns `false` for `Phalcon\*` (same casing-mismatch
root cause as the third gap), so the strict check is silently skipped — letting
`phalcon-shared`'s pervasive use of Phalcon's magic/dynamic property access
(`config->listeners`, `config->logger->{adapter}`, etc. — populated via
`__get`/collection magic at runtime, never formally declared on the `.zep`
interfaces) compile unchecked. The instant the casing fix makes `isClass()`
succeed, `getClassDefinition()` returns a real reflected `Definition`, the
strict check fires, and **dozens of `classDoesNotHaveProperty` compile-time
crashes cascade across idiomatic, working, production Phalcon usage** —
`Kernel.zep`, `Logger.zep`, `Url.zep`, etc. **A complete casing fix would
immediately and totally break `phalcon-shared` compilation through this
cascade — not through the third gap — long before the third gap's C-compile
error could ever surface in a real build.**

To get past this cascade and reach the third gap anyway (in a disposable test
container, source patched in-place — not the user's real repo), each crash
site was rewritten to use Zephir's *dynamic* property-access AST node
(`'property-string-access'`/`'property-dynamic-access'` →
`PropertyDynamicAccess`, which never calls `checkClassHasProperty` —
confirmed by reading its full ~159-line source). The reliable bypass is to
assign the property name to a local variable first and access via
`->{variableName}` (unambiguously parsed as `Types::T_VARIABLE`); the more
obvious `->{"literalString"}` form is **inconsistent** — it suppressed the
error for some properties (`listeners`, `event`, `listener`, `session`) but
not others (`system` recurred verbatim), for reasons not fully resolved
(possibly literal-string braces get re-normalized to `property-access` nodes
in some contexts). `let key = "system"; ... ->{key}` worked every time.

**Once patched through (≈14 sites across `Kernel.zep`, `Logger.zep`,
`Url.zep`), `zephir generate --export-classes` succeeded cleanly — and `zephir
compile` then failed with EXACTLY the predicted third-gap error**, confirming
the theoretical chain end-to-end:

```
ext/twistersfury/phalcon/shared/di/interfaces/initializationaware.zep.c:19:93: error:
'phalcon_di_initializationawareinterface_ce' undeclared (first use in this function)
   zend_class_implements(twistersfury_phalcon_shared_di_interfaces_initializationaware_ce,
                         1, phalcon_di_initializationawareinterface_ce);
```

Confirmed via direct inspection of the generated tree:
- `phalcon_di_initializationawareinterface_ce` is referenced as a **direct,
  unresolved C symbol** (no `extern`, no `zephir_get_internal_ce` runtime
  lookup) in **5 separate generated `.c` files**
  (`InitializationAware`, `Authorization` ×2, `Captcha` ×2) — all unresolvable
  at compile time.
- **No header anywhere** declares it `extern` —
  `generateClassHeadersPost()`'s `'class'`-only guard (the actual third-gap
  defect) means `external-dependencies` never emits the needed forward
  declaration for an `interface`'s external parent.
- `ext/phalcon/phalcon/di/` doesn't even exist in the generated `ext/` tree —
  `external-dependencies` expects the symbol to come from the *running*
  Phalcon extension at link/load time, with no compile-time forward
  declaration to satisfy the C compiler.
- A related symptom appeared in `abstractserviceprovider.zep.c`: `fatal error:
  ext/phalcon/phalcon/di/abstractinjectionaware.zep.h: No such file or
  directory` on an `#include` — same missing-header root cause, manifesting as
  a missing file rather than an undeclared symbol.

**This is the closing link in the chain**: the casing bug isn't just masking
one dormant defect — it's a load-bearing shield over (at least) two
independent, serious compile-time defects that would otherwise make
`external-dependencies` unusable for any Phalcon-extension project using
idiomatic Phalcon magic-property patterns. Fixing the casing bug in isolation,
without ALSO (a) relaxing/extending static property validation to tolerate
runtime-magic properties on reflected external classes, AND (b) fixing
`generateClassHeadersPost()`'s `class`-only include/extern guard to also cover
`interface`, would turn a currently-working build into a completely
uncompilable one.
