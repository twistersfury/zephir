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
- **Runtime in Mode B: loading the compiled `.so` segfaults.**
  `zephir_get_internal_ce()` returns `NULL` (no CE registered — nothing in
  that runtime loads `Psr\Log\LoggerInterface`), and `zend_class_implements()`
  dereferences it — crashing the whole PHP process at `MINIT`
  (`php -m`, `php -r '...'`, anything that loads the extension). Reproduced
  directly: `Segmentation fault (core dumped)`.

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
user. **Possible future mitigation for both** (not implemented, just floated):
guard `zend_class_implements()`/`zephir_get_internal_ce()` call sites with a
NULL check that raises a catchable PHP `RuntimeException` instead of
segfaulting — this would fix Gap A's crash and turn Gap B's failure mode from
"crash" into "catchable error," in both the new flatten path and the
decades-old `class ... implements`/`extends` CE-reference paths alike.

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
