.DEFAULT_GOAL := build

ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
PRESET ?= debug
BUILD_DIR ?= $(ROOT)zep/cmake-build-$(PRESET)
PREFIX ?= $(HOME)/.local/share/zep
JOBS ?= $(shell nproc)
CMAKE ?= cmake
CTEST ?= ctest
CMAKE_ARGS ?=
TEST_ARGS ?=

.PHONY: configure build release test install install-zep install-std help

configure:
	$(CMAKE) --preset "$(PRESET)" -S "$(ROOT)zep" -B "$(BUILD_DIR)" $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel "$(JOBS)"

release:
	$(MAKE) --file "$(ROOT)Makefile" build PRESET=release

test: build
	$(CTEST) --test-dir "$(BUILD_DIR)" --parallel "$(JOBS)" --output-on-failure --no-tests=error $(TEST_ARGS)

install: build
	$(CMAKE) --install "$(BUILD_DIR)" --prefix "$(PREFIX)"

install-zep: build
	$(CMAKE) --install "$(BUILD_DIR)" --prefix "$(PREFIX)" --component Compiler

install-std: configure
	$(CMAKE) --install "$(BUILD_DIR)" --prefix "$(PREFIX)" --component StandardLibrary

help:
	@printf '%s\n' \
	  'make build          Configure and build the debug compiler' \
	  'make release        Configure and build the release compiler' \
	  'make test           Build and run the deterministic test suite' \
	  'make install        Install the compiler and std' \
	  'make install-zep    Install the compiler executable' \
	  'make install-std    Install std sources and its manifest' \
	  'Options: PRESET BUILD_DIR PREFIX JOBS CMAKE_ARGS TEST_ARGS' \
	  'Examples: make test TEST_ARGS="-R CliHttp"' \
	  '          make install PREFIX=/tmp/zep-install'
