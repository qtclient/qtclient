#!/usr/bin/env bash
# Attach .sig on Emscripten MAIN_MODULE=2 GOT helpers that upstream omits.
# Without this, /play boots crash in generateFuncType (sig.slice) or missing
# sig for jsHaveJspi. Usage: patch-tibia-js-main-module.sh <tibia.js>
set -euo pipefail
JS="${1:?tibia.js}"
[[ -f "$JS" ]] || { echo "missing $JS" >&2; exit 1; }

python3 - "$JS" <<'PY'
import pathlib, sys
path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
orig = text

invoke = (
    'createInvokeFunction=sig=>(ptr,...args)=>{var sp=stackSave();try{return dynCall(sig,ptr,args)}'
    'catch(e){stackRestore(sp);if(e!==e+0)throw e;_setThrew(1,0);if(sig[0]=="j")return 0n}}'
)
invoke_fixed = (
    'createInvokeFunction=sig=>{var f=(ptr,...args)=>{var sp=stackSave();try{return dynCall(sig,ptr,args)}'
    'catch(e){stackRestore(sp);if(e!==e+0)throw e;_setThrew(1,0);if(sig[0]=="j")return 0n}};'
    'f.sig=sig[0]+"i"+sig.slice(1);return f}'
)
if invoke in text:
    text = text.replace(invoke, invoke_fixed, 1)

cxa = (
    'else if(symName.startsWith("__cxa_find_matching_catch_")){sym=wasmImports[symName]='
    '(...args)=>{var rtn=findMatchingCatch(args);return rtn}}'
)
cxa_fixed = (
    'else if(symName.startsWith("__cxa_find_matching_catch_")){sym=wasmImports[symName]='
    '(...args)=>{var rtn=findMatchingCatch(args);return rtn};'
    'sym.sig="p".repeat(Math.max(1,(+symName.split("_").pop()||2)-1))}'
)
if cxa in text:
    text = text.replace(cxa, cxa_fixed, 1)

jshave = (
    'function jsHaveAsyncify(){return typeof Asyncify!=="undefined"}'
    'function jsHaveJspi(){return typeof Asyncify!=="undefined"&&!!Asyncify.makeAsyncFunction'
    '&&(!!WebAssembly.Function||!!WebAssembly.Suspending)}'
)
jshave_fixed = (
    'function jsHaveAsyncify(){return typeof Asyncify!=="undefined"}jsHaveAsyncify.sig="i";'
    'function jsHaveJspi(){return typeof Asyncify!=="undefined"&&!!Asyncify.makeAsyncFunction'
    '&&(!!WebAssembly.Function||!!WebAssembly.Suspending)}jsHaveJspi.sig="i";'
)
if jshave in text and "jsHaveJspi.sig=" not in text:
    text = text.replace(jshave, jshave_fixed, 1)

report = 'if(typeof value=="function"){entry.value=addFunction(value,value.sig)}'
report_prior = (
    'if(typeof value=="function"){if(!value.sig){if(!entry.required)continue;'
    'throw new Error("missing sig for "+symName)}entry.value=addFunction(value,value.sig)}'
)
report_fixed = (
    'if(typeof value=="function"){var sig=value.sig;if(!sig&&/^jsHave/.test(symName))sig="i";'
    'if(!sig){if(!entry.required)continue;throw new Error("missing sig for "+symName)}'
    'entry.value=addFunction(value,sig)}'
)
if report_prior in text:
    text = text.replace(report_prior, report_fixed, 1)
elif report in text:
    text = text.replace(report, report_fixed, 1)

if text == orig:
    print(f"[patch-tibia-js] no MAIN_MODULE sig patterns in {path} (ok if already patched)", file=sys.stderr)
else:
    path.write_text(text, encoding="utf-8")
    print(f"[patch-tibia-js] patched {path}")
PY
