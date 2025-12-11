import { createWasmTrimAdapter } from './shell-extension/wasm.js';
const adapter = await createWasmTrimAdapter('/home/danielmulec/projekte/trimmeh_b/shell-extension');
const res = adapter.trim('  hello  ', 'normal', {keep_blank_lines:false, strip_box_chars:false, trim_prompts:false, max_lines:50});
print(JSON.stringify(res));
