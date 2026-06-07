---
name: project-interface-flattening-poc
description: poc/interface-flattening branch — auto-flattens parent interface methods when the parent extension isn't in external-dependencies
metadata:
  type: project
---

Branch `poc/interface-flattening` adds a fix so a Zephir-authored interface
that `extends` an interface from an *optional* compiled extension (e.g.
`Psr\Log\LoggerInterface` from `php-psr`) no longer emits a hard
`zend_class_implements()` call against that extension's CE. When the parent
interface isn't declared in `external-dependencies` (no `.zep` sources to
reference), `CompilerFile::checkDependencies()` now reflects the parent and
flattens its methods directly into the child interface's method table via
`ZEPHIR_REGISTER_INTERFACE`, with `shouldFlattenInterface()` excluding PHP
core interfaces (`Iterator`, `Countable`, etc., detected by lack of
namespace) which must keep the CE-reference path since they're always present.

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
interface flattening, optional-extension CE references, or the
`Declaration ... must be compatible` notice class of bug in Zephir-generated
arg-info, this is the relevant branch/fix (not yet merged to `master` as of
2026-06-06).
