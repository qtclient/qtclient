# qtclient (otqt) — CipSoft-compatible Qt/QML Tibia client
# Cargo-like: make [<component>] <lifecycle>
#   components: client (default) | launcher | wasm
#   lifecycles: build | run | check | test | clean | install | doc | package | publish
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c

HERE := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
LIFEDIR := $(HERE)/scripts/lifecycle

UMBRELLA_TOOLS := $(abspath $(HERE)/../../tools)
ifeq ($(wildcard $(UMBRELLA_TOOLS)/tibia_paths.py),)
  TOOLS_DIR := $(HERE)/tools
else
  TOOLS_DIR := $(UMBRELLA_TOOLS)
endif
EXTRACT_PY := $(firstword $(wildcard $(TOOLS_DIR)/extract/cli.py $(HERE)/tools/extract/cli.py))
SYNC_PY := $(firstword $(wildcard $(TOOLS_DIR)/tibia-sync/sync.py $(HERE)/tools/tibia-sync/sync.py))

APP_COMPONENTS := client launcher wasm
APP_LIFECYCLES := build run check test clean install doc package publish
APP_DEFAULT_COMPONENT := client

.DEFAULT_GOAL := help

.PHONY: help setup sync tibia-sync extract fetch-assets qt \
	publish-assets cdn local-cdn tibia-dump-smoke tibia-android tibia-ios \
	demo-pack demo-run \
	$(APP_COMPONENTS) $(APP_LIFECYCLES) _cargo

# Host platform for demo pack (override: make demo-pack DEMO_PLATFORM=linux-x64).
ifeq ($(shell uname -s),Darwin)
  DEMO_PLATFORM ?= macos-arm64
else ifeq ($(OS),Windows_NT)
  DEMO_PLATFORM ?= windows-x64
else
  DEMO_PLATFORM ?= linux-x64
endif
DEMO_OUT ?= $(abspath $(HERE)/../../dist/demo)

help:
	@echo "qtclient — cargo-like CLI"
	@echo "  make <component> <lifecycle>   component: client|launcher|wasm (default client)"
	@echo "  make <lifecycle>               same with default component"
	@echo ""
	@echo "  make client build     qtclient desktop tibia (DEV=1 → disk QML; STOCK_HOST=1 skips packs)"
	@echo "  make client run       start stock client (--no-mods; ARGS=…)"
	@echo "  make client check     --check-qml"
	@echo "  make launcher build   CIP tip launcher (stock brand)"
	@echo "  make launcher install build the stock launcher"
	@echo "  make launcher run     start stock qtclient launcher (--no-mods; ARGS=…)"
	@echo "  make demo-pack      stage demo Release (our package + CipSoft assetsurl + mods/default)"
	@echo "  make demo-run       launcher with demo tip (Play → tip client + mods/default)"
	@echo "  make wasm build       qtclient MAIN_MODULE (needs make qt WASM=1)"
	@echo "  make wasm install     build + stage into dist/cdn for portal /play"
	@echo "  make client install   build + point dist/cdn desktop slice at this binary"
	@echo "  make package          PACKAGE_KIND=sdk|desktop|wasm (client-sdk ships cipgen)"
	@echo "  make publish          ORAS push PACKAGE_OUT to ghcr.io/…/client-*"
	@echo "  Product pack: make -C ../idleot-client client|wasm install (client then pack)"
	@echo "  make clean            wipe client+launcher+wasm build trees"
	@echo "  make client clean     wipe desktop client trees only"
	@echo ""
	@echo "  make qt               CIP Qt (WASM=1 + emsdk)"
	@echo "  make sync / extract   CIP vendor + assets"

# --- cargo-like multi-word goals ---
_cargo_words := $(filter $(APP_COMPONENTS) $(APP_LIFECYCLES),$(MAKECMDGOALS))
_cargo_comp := $(firstword $(filter $(APP_COMPONENTS),$(_cargo_words)))
_cargo_life := $(firstword $(filter $(APP_LIFECYCLES),$(_cargo_words)))
_bare_clean := $(and $(filter clean,$(MAKECMDGOALS)),$(if $(filter $(APP_COMPONENTS),$(MAKECMDGOALS)),,1))
ifeq ($(_cargo_comp),)
  _cargo_comp := $(APP_DEFAULT_COMPONENT)
endif

ifneq ($(filter $(APP_COMPONENTS),$(MAKECMDGOALS)),)
$(foreach c,$(filter $(APP_COMPONENTS),$(MAKECMDGOALS)),$(eval .PHONY: $(c))$(eval $(c): ; @:))
endif

ifneq ($(_bare_clean),)
.PHONY: clean
clean:
	@COMP=all bash "$(LIFEDIR)/clean.sh"
else
ifneq ($(_cargo_life),)
.PHONY: $(_cargo_life)
$(_cargo_life):
	@$(MAKE) -f "$(HERE)/Makefile" _cargo COMP=$(_cargo_comp) LIFE=$(_cargo_life) \
		$(if $(DEV),DEV=$(DEV),) $(if $(DEBUG),DEBUG=$(DEBUG),) $(if $(ARGS),ARGS="$(ARGS)",)
endif
endif

_cargo:
	@comp="$(COMP)"; life="$(LIFE)"; \
	case "$$life" in \
	  build|run|check|test|clean|install|doc) \
	    COMP="$$comp" DEV="$(DEV)" DEBUG="$(DEBUG)" ARGS="$(ARGS)" \
	      bash "$(LIFEDIR)/$$life.sh" ;; \
	  package) \
	    COMP="$$comp" PACKAGE_KIND="$(if $(PACKAGE_KIND),$(PACKAGE_KIND),sdk)" \
	      PLATFORM="$(PLATFORM)" BIN_DIR="$(BIN_DIR)" PACKAGE_OUT="$(PACKAGE_OUT)" \
	      bash "$(LIFEDIR)/package.sh" ;; \
	  publish) \
	    COMP="$$comp" PACKAGE_KIND="$(if $(PACKAGE_KIND),$(PACKAGE_KIND),sdk)" \
	      PLATFORM="$(PLATFORM)" PACKAGE_OUT="$(PACKAGE_OUT)" \
	      REGISTRY="$(REGISTRY)" \
	      bash "$(LIFEDIR)/publish.sh" ;; \
	  *) echo "[qtclient] unknown $$comp $$life — try: make help"; exit 1 ;; \
	esac

demo-pack:
	@comp_bin=$$(bash -c 'source "$(HERE)/scripts/deploy/desktop-paths.sh"; qtclient_desktop_paths "$(HERE)"; dirname "$$QTCLIENT_CLIENT_BIN"'); \
	  if [[ ! -d "$$comp_bin" ]]; then \
	    echo "[demo-pack] missing client build — make client build && make launcher build" >&2; \
	    exit 1; \
	  fi; \
	bash "$(HERE)/scripts/deploy/pack-demo-release.sh" \
	    --platform "$(DEMO_PLATFORM)" \
	    --bin "$$comp_bin" \
	    --out "$(DEMO_OUT)" \
	    $(if $(RELEASE_BASE),--release-base-url "$(RELEASE_BASE)",)

demo-run:
	@bash "$(HERE)/scripts/deploy/start-demo-launcher.sh" $(ARGS)

setup:
	@git submodule update --init third_party/Qt5LegacyControls 2>/dev/null || true
	@$(MAKE) qt
	@$(MAKE) sync
	@$(MAKE) extract
	@echo "[setup] done — make client build && make client run"

sync tibia-sync:
	@test -f "$(SYNC_PY)" || { echo "[sync] missing $(SYNC_PY)"; exit 1; }
	@python3 "$(SYNC_PY)" \
		$(if $(TIBIA),--tibia="$(TIBIA)",) \
		$(if $(MSGMAP),--msgmap="$(MSGMAP)",) \
		$(if $(CLIENT_VERSION),--things-dir="$(HERE)/data/assets",)

extract:
	@test -f "$(EXTRACT_PY)" || { echo "[extract] missing $(EXTRACT_PY)"; exit 1; }
	@python3 "$(EXTRACT_PY)" \
		$(if $(TIBIA),--tibia="$(TIBIA)",) \
		--assets-dir="$(HERE)/data/assets" \
		$(if $(CLIENT_VERSION),--client-version="$(CLIENT_VERSION)",) \
		$(EXTRACT_FLAGS)
	@python3 "$(HERE)/tools/publish/cli.py" assets things 2>/dev/null || true

fetch-assets:
	@python3 "$(HERE)/tools/fetch-assets/fetch_assets.py" \
		$(if $(TIBIA),--tibia="$(TIBIA)",) \
		--out="$(HERE)" \
		$(ARGS)

qt:
	@bash "$(HERE)/tools/qt/install-qt.sh" $(if $(filter 1 true yes on,$(WASM)),--wasm,)

tibia-dump-smoke:
	@bash "$(HERE)/scripts/ci/check-session-dump-smoke.sh"

tibia-android:
	@bash "$(HERE)/scripts/deploy/build-tibia-android.sh" $(ARGS)

tibia-ios:
	@bash "$(HERE)/scripts/deploy/build-tibia-ios.sh" $(ARGS)

cdn:
	@python3 "$(HERE)/tools/publish/local_cdn.py" --desktop --wasm

publish-assets:
	@python3 "$(HERE)/tools/publish/cli.py" assets $(ARGS)
	@python3 "$(HERE)/tools/publish/local_cdn.py"

local-cdn:
	@python3 "$(HERE)/tools/publish/local_cdn.py"
