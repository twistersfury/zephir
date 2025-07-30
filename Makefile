###############################################################
# This file is part of the Zephir.
#
# (c) Phalcon Team <team@zephir-lang.com>
#
# For the full copyright and license information, please view
# the LICENSE file that was distributed with this source code.
#
###############################################################
# SYSTEM MAKE FILE
###############################################################
# Use this to allow quick/easy modifications and/or updates to
#   the software. Requires make to be installed.
#
# Note: This assumes you already have Docker setup and properly
#   configured in your path environment
###############################################################
# Windows
#
# http://gnuwin32.sourceforge.net/downlinks/make.php
###############################################################
# OS X / Linux
#
# Likely Already Installed. Otherwise use the os's package
#   manager (IE: apt-get, brew, macports, etc)
###############################################################

######################################################
# Initial Values (Overrides Included Defaults)
######################################################

DOCKER_SERVICE ?=
DOCKER_CONFIG ?= docker-compose.yml

XDEBUG_OPTIONS=-d zend_extension=xdebug.so -d xdebug.mode=debug,develop -d xdebug.start_with_request=yes -d xdebug.client_host=host.docker.internal

######################################################
#                 Default Run Command                #
######################################################
.PHONY: default
default: test

######################################################
# Docker
######################################################

.PHONY: docker-build
docker-build:
	docker compose -f $(DOCKER_CONFIG) build $(DOCKER_SERVICE)

.PHONY: docker-build-80
docker-build-83: DOCKER_SERVICE=zephir-8.0
docker-build: docker-build

.PHONY: docker-build-81
docker-build-83: DOCKER_SERVICE=zephir-8.1
docker-build: docker-build

.PHONY: docker-build-82
docker-build-83: DOCKER_SERVICE=zephir-8.2
docker-build: docker-build

.PHONY: docker-build-83
docker-build-83: DOCKER_SERVICE=zephir-8.3
docker-build: docker-build

.PHONY: docker-build-84
docker-build-83: DOCKER_SERVICE=zephir-8.4
docker-build: docker-build

.PHONY: docker-start
docker-start:
	docker compose -f $(DOCKER_CONFIG) up $(DOCKER_SERVICE) -d

.PHONY: docker-stop
docker-stop:
	docker compose -f $(DOCKER_CONFIG) down

######################################################
# Composer
######################################################

.PHONY: composer-install
composer-install: docker-start
	docker compose -f $(DOCKER_CONFIG) exec $(DOCKER_SERVICE) php -d memory_limit=-1 composer install

.PHONY: composer-install-80
composer-install-80: DOCKER_SERVICE=zephir-8.0
composer-install-80: composer-install

.PHONY: composer-install-81
composer-install-81: DOCKER_SERVICE=zephir-8.1
composer-install-81: composer-install

.PHONY: composer-install-82
composer-install-82: DOCKER_SERVICE=zephir-8.2
composer-install-82: composer-install

.PHONY: composer-install-83
composer-install-83: DOCKER_SERVICE=zephir-8.3
composer-install-83: composer-install

.PHONY: composer-install-84
composer-install-84: DOCKER_SERVICE=zephir-8.4
composer-install-84: composer-install

######################################################
# Zephir
######################################################

.PHONY: zephir-command
zephir-command: docker-start
	docker compose -f $(DOCKER_CONFIG) exec $(DOCKER_SERVICE) php $(XDEBUG_OPTIONS) -d memory_limit=-1 ./zephir $(ZEPHIR_COMMAND)

.PHONY: zephir-clean
zephir-clean: ZEPHIR_COMMAND=fullclean
zephir-clean: zephir-command

.PHONY: zephir-generate
zephir-generate: ZEPHIR_COMMAND=generate
zephir-generate: zephir-command

.PHONY: zephir-compile
zephir-compile: ZEPHIR_COMMAND=compile
zephir-compile: zephir-command

.PHONY: zephir-build
zephir-build:
	$(MAKE) zephir-clean DOCKER_SERVICE=$(DOCKER_SERVICE)
	$(MAKE) zephir-generate DOCKER_SERVICE=$(DOCKER_SERVICE)
	$(MAKE) zephir-compile DOCKER_SERVICE=$(DOCKER_SERVICE)

.PHONY: zephir-build-80
zephir-build-80: DOCKER_SERVICE=zephir-8.0
zephir-build-80: zephir-build

.PHONY: zephir-build-81
zephir-build-81: DOCKER_SERVICE=zephir-8.1
zephir-build-81: zephir-build

.PHONY: zephir-build-80
zephir-build-82: DOCKER_SERVICE=zephir-8.2
zephir-build-82: zephir-build

.PHONY: zephir-build-83
zephir-build-83: DOCKER_SERVICE=zephir-8.3
zephir-build-83: zephir-build

.PHONY: zephir-build-84
zephir-build-84: DOCKER_SERVICE=zephir-8.4
zephir-build-84: zephir-build

######################################################
# Test
######################################################

.PHONY: test
test: docker-start
	docker compose -f $(DOCKER_CONFIG) exec $(DOCKER_SERVICE) php $(XDEBUG_OPTIONS) -d memory_limit=-1 -d extension=/srv/ext/modules/stub.so vendor/bin/phpunit --bootstrap tests/ext-bootstrap.php --testsuite Extension
	docker compose -f $(DOCKER_CONFIG) exec $(DOCKER_SERVICE) php $(XDEBUG_OPTIONS) -d memory_limit=-1 vendor/bin/phpunit --colors=always --testsuite Zephir

.PHONY: test-80
test-80: DOCKER_SERVICE=zephir-8.0
test-80: test

.PHONY: test-81
test-81: DOCKER_SERVICE=zephir-8.1
test-81: test

.PHONY: test-82
test-82: DOCKER_SERVICE=zephir-8.2
test-82: test

.PHONY: test-83
test-83: DOCKER_SERVICE=zephir-8.3
test-83: test

.PHONY: test-84
test-84: DOCKER_SERVICE=zephir-8.4
test-84: test
