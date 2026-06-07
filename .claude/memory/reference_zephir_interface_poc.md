---
name: reference-zephir-interface-poc
description: Public POC repo verifying the poc/interface-flattening branch fix in two Docker runtime modes
metadata:
  type: reference
---

The interface-flattening fix on [[project-interface-flattening-poc]] is
verified by a standalone repo at
`/Users/fenikkusu/Projects/twistersfury.net/zephir-interface-flattening-poc`,
pushed publicly to `git@gitlab.com:twistersfury/proofs/zephir-interface-poc.git`
(branch `master`). It compiles a `Poc\Logger extends \Psr\Log\LoggerInterface`
extension once against the `poc/interface-flattening` branch and runs a
Codeception suite inside two Docker images — `with-psr-ext` (php-psr loaded
at runtime) and `without-psr-ext` (php-psr absent, `psr/log` Composer package
only) — proving the compiled extension loads and behaves identically in both,
with no hard runtime dependency on the optional extension's CE.

**How to apply:** Use this repo to reproduce/extend verification of the
flattening fix, or as a template for similar optional-extension-dependency
POCs (e.g. for Phalcon-based Zephir extensions). GitLab requires SSH or a
PAT for pushes — plain HTTPS Basic auth is rejected.
