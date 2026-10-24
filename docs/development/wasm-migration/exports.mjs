// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
// Capture the built module and facade without changing the shipped entry points.
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

const dist = resolve(process.argv[2]);
const entry = await import(pathToFileURL(resolve(dist, "opentlv.mjs")));
const { default: createOpenTLV } = await import(pathToFileURL(resolve(dist, "opentlv-core.js")));
const runtime = await createOpenTLV();
const api = await entry.loadOpenTLV();
const module = await WebAssembly.compile(await readFile(resolve(dist, "opentlv-core.wasm")));
const classes = {};
for (const [name, value] of Object.entries(api)) {
  if (typeof value === "function" && value.prototype) {
    classes[name] = {
      prototype: Object.getOwnPropertyNames(value.prototype).sort(),
      static: Object.getOwnPropertyNames(value).filter(key => !["length", "name", "prototype"].includes(key)).sort(),
    };
  }
}
console.log(JSON.stringify({
  entryPoints: Object.keys(entry).sort(),
  api: Object.keys(api).sort(),
  classes,
  version: api.version,
  formats: api.formats,
  modules: api.modules,
  bridgeExports: Object.keys(runtime).filter(name => name.startsWith("_opentlv_wasm_")).sort(),
  wasmExports: WebAssembly.Module.exports(module),
  wasmImports: WebAssembly.Module.imports(module),
}, null, 2));
