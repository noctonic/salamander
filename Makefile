CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Icpp
EMXX ?= em++
EMXXFLAGS ?= -O3 -std=c++17 -Icpp \
  -sENVIRONMENT=web \
  -sMODULARIZE=1 -sEXPORT_ES6=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sASSERTIONS=1 \
  -sINITIAL_MEMORY=134217728 \
  -sEXPORTED_RUNTIME_METHODS='["cwrap","ccall","getValue","setValue","FS","UTF8ToString"]' \
  -sEXPORTED_FUNCTIONS='["_wasm_encode","_wasm_decode","_malloc","_free"]'

all: rgcli

rgcli: cpp/rg_cli.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

wasm: web/rgcli.mjs

web/rgcli.mjs: cpp/rg_cli.cpp
	mkdir -p web
	$(EMXX) $(EMXXFLAGS) $< -o $@

clean:
	rm -f rgcli
	rm -f web/rgcli.mjs web/rgcli.wasm

.PHONY: all clean wasm
