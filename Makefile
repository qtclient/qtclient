# qtclient — CipSoft-compatible Qt/QML Tibia client
# Cargo-like: make [<component>] <lifecycle>
#   components: desktop (default) | launcher | browser
#   lifecycles: build | run | check | test | clean | install | doc | package | publish
#
# Stock (default): CIP host only. `desktop run` is launcher-first (CIP tip).
# Product (PRODUCT=1 + QTCLIENT_PACK): stock install/run, then the configured pack.
# Umbrella: make stock …  |  make qtclient … (sets PRODUCT=1).
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

PRODUCT ?= 0
APP_COMPONENTS := desktop launcher browser
APP_LIFECYCLES := build run check test clean install doc package publish
APP_DEFAULT_COMPONENT := desktop

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
	@echo "  make <component> <lifecycle>   component: desktop|launcher|browser (default desktop)"
	@echo "  make <lifecycle>               same with default component"
	@echo ""
ifeq ($(PRODUCT),1)
	@echo "  PRODUCT=1 mode (umbrella: make qtclient …)"
	@echo "  QTCLIENT_PACK=$(QTCLIENT_PACK)"
	@echo "  make desktop install  SDK → pack MODULE (native/mod.*) → tip + mods/<id>"
	@echo "  make launcher install stock launcher install, then tip brand"
	@echo "  make browser install  wasm SDK → pack static-ABI .a → tibia.wasm + mods/<id>"
	@echo "  make package          SDK → pack → tip build with QTCLIENT_PACKS (no pack install step)"
	@echo "  make desktop run      launcher-first CIP tip (worktrees + pack; shared assets)"
	@echo "  DIRECT=1 make desktop run   debug hatch: tip binary (F5 / disk QML)"
	@echo "  DEV desktop: disk QML from QTCLIENT_PACK (edit qml/overrides; F5 clears cache)"
	@echo "  Stock-only smoke: umbrella make stock desktop run"
else
	@echo "  make desktop build    desktop tibia (DEV=1 → disk QML; STOCK_HOST=1 skips packs)"
	@echo "  make desktop run      launcher-first CIP tip (Update/Play; worktrees + packs)"
	@echo "  DIRECT=1 make desktop run   debug hatch: client binary (--no-mods; ARGS=…)"
	@echo "  make desktop check    --check-qml"
	@echo "  make launcher build   CIP tip launcher (stock brand)"
	@echo "  make launcher install build the stock launcher"
	@echo "  make launcher run     start stock qtclient launcher (--no-mods; ARGS=…)"
	@echo "  make demo-pack        stage demo Release (our package + CipSoft assetsurl + mods/default)"
	@echo "  make demo-run         launcher with demo tip (Play → tip client + mods/default)"
	@echo "  make browser build    qtclient wasm tip (needs make qt WASM=1; optional QTCLIENT_PACKS)"
	@echo "  make browser install  build + stage into dist/cdn for portal /play"
	@echo "  make desktop install  build + point dist/cdn desktop slice at this binary"
	@echo "  make package          PACKAGE_KIND=sdk|desktop|wasm (sdk = QtClientSdk: abi/ + cmake config)"
	@echo "  scripts/lifecycle/build/sdk.sh [--wasm]  pack SDK → build/qtclient-sdk/<desktop|wasm>"
	@echo "  make publish          ORAS push PACKAGE_OUT to ghcr.io/…/client-*"
	@echo "  Product (umbrella): make qtclient … with QTCLIENT_PACK in .env"
	@echo "  make clean            wipe desktop+launcher+browser build trees"
	@echo "  make desktop clean    wipe desktop client trees only"
endif
	@echo ""
	@echo "  make qt               CIP Qt (WASM=1 + emsdk)"
	@echo "  make sync / extract   CIP vendor + assets"
	@echo "  PLAY_PACK_FLAGS=…     flags for desktop run tip emit (--current-only, --pack, …)"

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
ifeq ($(PRODUCT),1)
	@test -n "$(QTCLIENT_PACK)" || { echo "[qtclient] PRODUCT=1 needs QTCLIENT_PACK" >&2; exit 1; }
	@bash "$(QTCLIENT_PACK)/scripts/lifecycle/clean/clean.sh"
else
	@bash "$(LIFEDIR)/clean/all.sh"
endif
else
ifneq ($(_cargo_life),)
.PHONY: $(_cargo_life)
$(_cargo_life):
	@$(MAKE) -f "$(HERE)/Makefile" _cargo COMP=$(_cargo_comp) LIFE=$(_cargo_life) \
		PRODUCT="$(PRODUCT)" QTCLIENT_PACK="$(QTCLIENT_PACK)" \
		$(if $(CLIENT_SDK),CLIENT_SDK="$(CLIENT_SDK)",) \
		$(if $(DEV),DEV=$(DEV),) $(if $(DEBUG),DEBUG=$(DEBUG),) $(if $(ARGS),ARGS="$(ARGS)",) \
		$(if $(DIRECT),DIRECT=$(DIRECT),) \
		$(if $(PLAY_PACK_FLAGS),PLAY_PACK_FLAGS="$(PLAY_PACK_FLAGS)",)
endif
endif

# Map Make component → lifecycle script stem (browser → wasm scripts on disk).
# desktop run → launcher-first CIP tip unless DIRECT=1 (client binary hatch).
PLAY_PACK_FLAGS ?=
DIRECT ?= 0
_cargo:
	@comp="$(COMP)"; life="$(LIFE)"; \
	case "$$comp" in \
	  desktop) target=desktop ;; \
	  browser) target=wasm ;; \
	  launcher) target=launcher ;; \
	  *) echo "[qtclient] unknown component $$comp (want desktop|launcher|browser)" >&2; exit 1 ;; \
	esac; \
	if [[ "$(PRODUCT)" == "1" ]]; then \
	  pack="$(QTCLIENT_PACK)"; \
	  if [[ -z "$$pack" || ! -f "$$pack/client/mod.json" ]]; then \
	    echo "[qtclient] PRODUCT=1 needs QTCLIENT_PACK with client/mod.json" >&2; \
	    exit 1; \
	  fi; \
	  case "$$life" in \
	    build|install|package) \
	      test -f "$$pack/client/mod.json" || { echo "[qtclient] missing $$pack/client/mod.json" >&2; exit 1; }; \
	      test -f "$$pack/scripts/lifecycle/build/pack.sh" || { echo "[qtclient] pack has no scripts/lifecycle/build/pack.sh" >&2; exit 1; }; \
	      if [[ -n "$(DEV)" ]]; then _dev="$(DEV)"; \
	      elif [[ "$$comp" == desktop ]]; then _dev=1; \
	      else _dev=""; fi; \
	      _plat=desktop; _wasm=""; \
	      if [[ "$$comp" == browser ]]; then _plat=wasm; _wasm=--wasm; fi; \
	      if [[ "$$comp" == launcher ]]; then _prefix=""; else \
	        sdk="$(HERE)/build/qtclient-sdk/$$_plat"; \
	        echo "[qtclient] 1/3 SDK ($$_plat) → $$sdk"; \
	        QTCLIENT_SDK_DIR="$$sdk" bash "$(LIFEDIR)/build/sdk.sh" $$_wasm >/dev/null || exit 1; \
	        echo "[qtclient] 2/3 pack against the SDK only ($$pack)"; \
	        QTCLIENT_SDK_DIR="$$sdk" QTCLIENT_ROOT="$(HERE)" DEBUG="$(DEBUG)" \
	          bash "$$pack/scripts/lifecycle/build/pack.sh" $$_wasm || exit 1; \
	        _prefix="$$pack/build/$$_plat/install"; \
	      fi; \
	      _life="$$life"; [[ "$$life" == package ]] && _life=build; \
	      echo "[qtclient] 3/3 tip $$comp $$_life (QTCLIENT_PACKS=$$_prefix)"; \
	      STOCK_HOST=0 QTCLIENT_PACKS="$$_prefix" QTCLIENT_DEV_PACK_SOURCES="$$pack/client" \
	        COMP="$$comp" DEV="$$_dev" DEBUG="$(DEBUG)" \
	        bash "$(LIFEDIR)/$$_life/$$target.sh" || exit 1; \
	      if [[ "$$life" != package ]]; then \
	        COMP="$$comp" DEV="$$_dev" DEBUG="$(DEBUG)" \
	          STOCK_ALREADY=1 QTCLIENT_ROOT="$(HERE)" \
	          bash "$$pack/scripts/lifecycle/install/$$target.sh"; \
	      fi ;; \
	    run) \
	      if [[ "$$comp" == desktop && "$(DIRECT)" != "1" ]]; then \
	        bash "$(LIFEDIR)/package/play.sh" $(PLAY_PACK_FLAGS); \
	        COMP="$$comp" PRODUCT=1 QTCLIENT_PACK="$$pack" QTCLIENT_ROOT="$(HERE)" \
	          $(if $(ARGS),ARGS="$(ARGS)",) \
	          bash "$(LIFEDIR)/run/play.sh" $(ARGS); \
	      else \
	        COMP="$$comp" QTCLIENT_ROOT="$(HERE)" \
	          $(if $(ARGS),ARGS="$(ARGS)",) \
	          bash "$$pack/scripts/lifecycle/run/$$target.sh"; \
	      fi ;; \
	    check) bash "$$pack/scripts/lifecycle/check/check.sh" ;; \
	    clean) bash "$$pack/scripts/lifecycle/clean/clean.sh" ;; \
	    doc) echo "[qtclient] PRODUCT pack: $$pack (mod.json + qml + overrides)" ;; \
	    test|publish) \
	      echo "[qtclient] PRODUCT=1: $$life is stock-only — use make stock $$comp $$life" >&2; \
	      exit 1 ;; \
	    *) echo "[qtclient] unknown PRODUCT $$comp $$life"; exit 1 ;; \
	  esac; \
	  exit 0; \
	fi; \
	case "$$life" in \
	  package|publish) target="$(if $(PACKAGE_KIND),$(PACKAGE_KIND),sdk)" ;; \
	  doc) target=help ;; \
	  run) \
	    if [[ "$$comp" == desktop && "$(DIRECT)" != "1" ]]; then \
	      bash "$(LIFEDIR)/package/play.sh" $(PLAY_PACK_FLAGS); \
	      COMP="$$comp" PRODUCT="$(PRODUCT)" QTCLIENT_PACK="$(QTCLIENT_PACK)" \
	        DEV="$(DEV)" DEBUG="$(DEBUG)" ARGS="$(ARGS)" \
	        bash "$(LIFEDIR)/run/play.sh" $(ARGS); \
	      exit 0; \
	    fi ;; \
	esac; \
	script="$(LIFEDIR)/$$life/$$target.sh"; \
	test -f "$$script" || { echo "[qtclient] missing $$script (comp=$$comp life=$$life target=$$target)" >&2; exit 1; }; \
	COMP="$$comp" DEV="$(DEV)" DEBUG="$(DEBUG)" ARGS="$(ARGS)" \
	  PACKAGE_KIND="$(if $(PACKAGE_KIND),$(PACKAGE_KIND),sdk)" \
	  PLATFORM="$(PLATFORM)" BIN_DIR="$(BIN_DIR)" PACKAGE_OUT="$(PACKAGE_OUT)" \
	  REGISTRY="$(REGISTRY)" \
	  bash "$$script"

demo-pack:
	@comp_bin=$$(bash -c 'source "$(HERE)/scripts/lifecycle/lib/desktop-paths.sh"; qtclient_desktop_paths "$(HERE)"; dirname "$$QTCLIENT_CLIENT_BIN"'); \
	  if [[ ! -d "$$comp_bin" ]]; then \
	    echo "[demo-pack] missing client build — make desktop build && make launcher build" >&2; \
	    exit 1; \
	  fi; \
	bash "$(HERE)/scripts/lifecycle/package/demo.sh" \
	    --platform "$(DEMO_PLATFORM)" \
	    --bin "$$comp_bin" \
	    --out "$(DEMO_OUT)" \
	    $(if $(RELEASE_BASE),--release-base-url "$(RELEASE_BASE)",)

demo-run:
	@bash "$(HERE)/scripts/lifecycle/run/demo.sh" $(ARGS)

setup:
	@git submodule update --init third_party/Qt5LegacyControls 2>/dev/null || true
	@$(MAKE) qt
	@$(MAKE) sync
	@$(MAKE) extract
	@echo "[setup] done — make desktop build && make launcher build && make desktop run"

sync tibia-sync:
	@test -f "$(SYNC_PY)" || { echo "[sync] missing $(SYNC_PY)"; exit 1; }
	@python3 "$(SYNC_PY)" \
		$(if $(TIBIA),--tibia="$(TIBIA)",) \
		$(if $(MSGMAP),--msgmap="$(MSGMAP)",)

extract:
	@test -f "$(EXTRACT_PY)" || { echo "[extract] missing $(EXTRACT_PY)"; exit 1; }
	@python3 "$(EXTRACT_PY)" \
		$(if $(TIBIA),--tibia="$(TIBIA)",) \
		--assets-dir="$(HERE)/data/assets" \
		$(EXTRACT_FLAGS)
	@python3 "$(HERE)/tools/publish/cli.py" assets things 2>/dev/null || true

fetch-assets:
	@python3 "$(HERE)/tools/fetch-assets/fetch_assets.py" \
		$(if $(TIBIA),--tibia="$(TIBIA)",) \
		--out="$(HERE)" \
		$(ARGS)

qt:
	@bash "$(HERE)/tools/qt/install-qt.sh" \
		$(if $(filter 1 true yes on,$(WASM)),--wasm,)

tibia-dump-smoke:
	@bash "$(HERE)/scripts/ci/check-session-dump-smoke.sh"

tibia-android:
	@bash "$(HERE)/scripts/lifecycle/build/android.sh" $(ARGS)

tibia-ios:
	@bash "$(HERE)/scripts/lifecycle/build/ios.sh" $(ARGS)

cdn:
	@python3 "$(HERE)/tools/publish/local_cdn.py" --desktop --wasm

publish-assets:
	@python3 "$(HERE)/tools/publish/cli.py" assets $(ARGS)
	@python3 "$(HERE)/tools/publish/local_cdn.py"

local-cdn:
	@python3 "$(HERE)/tools/publish/local_cdn.py"
