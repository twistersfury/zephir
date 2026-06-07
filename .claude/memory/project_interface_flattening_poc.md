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
