// Offline REA batch. Uses only public MCP tools and the public REA launcher.
import { retainedAnalysisId, recoverAnalysis } from './rea_retained_analysis.mjs';
import { createRequire } from 'node:module';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const dependencies = resolve(root, 'work/rea');
const require = createRequire(resolve(dependencies, 'package.json'));
const { Client } = require('@modelcontextprotocol/client');
const { StdioClientTransport } = require('@modelcontextprotocol/client/stdio');
const target = process.argv[2];
const procedures = (process.env.REA_LMU_RVAS ?? 'ada710,adb300,adc3a0,ecdd90,ecd890,ecd670').split(',');
if (!procedures.length || procedures.length > 12 || procedures.some(rva => !/^[0-9a-f]{1,8}$/i.test(rva))) throw new Error('Expected 1..12 hexadecimal RVAs');
const output = resolve(process.argv[3] ?? resolve(root, 'analysis/rea-25661166/batch'));
if (!target) throw new Error('Usage: node rea_lmu_analysis.mjs <absolute LMU executable> [new output directory]');
const bytes = await readFile(target);
const sha256 = createHash('sha256').update(bytes).digest('hex');
const capturePath = target.replace(/\.[^.\\/]+$/, '.manifest.json');
const capture = JSON.parse(await readFile(capturePath, 'utf8'));
if (capture.schema !== 'lmu-rea-code-v1' || capture.derived_sha256 !== sha256 ||
    capture.original_sha256 !== '66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf' ||
    !capture.code_stable_two_reads || capture.process_writes !== 0) throw new Error('Unsupported or unverified code capture');
const pe = bytes.readUInt32LE(0x3c);
if (bytes.readUInt32LE(pe) !== 0x4550 || bytes.readUInt16LE(pe + 24) !== 0x20b) throw new Error('Expected PE32+');
const base = bytes.readBigUInt64LE(pe + 48);
await mkdir(output, { recursive: true });
const client = new Client({ name: 'lmu-offline-analysis', version: '1.0.0' });
const transport = new StdioClientTransport({
  command: process.execPath,
  args: [resolve(dependencies, 'node_modules/rea-agents/scripts/rea.mjs'), '--mcp'],
  env: { ...process.env, REA_ANALYSIS_PROVIDER: 'ghidra' }, stderr: 'inherit',
  maxBufferSize: 64 * 1024 * 1024,
});
const calls = [];
const retained = [];
let completed = false;
async function call(name, args, label) {
  console.log(`${new Date().toISOString()} ${name} ${label}`);
  const response = await client.callTool({ name, arguments: args }, { timeout: 900000 });
  await writeFile(resolve(output, `${label}.json`), JSON.stringify(response, null, 2), { flag: 'wx' });
  calls.push({ name, arguments: args, file: `${label}.json`, isError: !!response.isError });
  if (response.isError) {
    const evidenceId = name === 'analyze_function' ? retainedAnalysisId(response) : null;
    if (!evidenceId) throw new Error(`REA ${name} failed; see ${label}.json`);
    retained.push({ label, evidenceId });
  }
  return response;
}
let connected = false;
let opened = false;
try {
  await client.connect(transport); connected = true;
  const catalog = await client.listTools();
  await writeFile(resolve(output, 'catalog.json'), JSON.stringify(catalog, null, 2), { flag: 'wx' });
  for (const name of ['open_binary', 'analyze_function', 'close_binary']) {
    if (!catalog.tools.some(t => t.name === name)) throw new Error(`Missing MCP tool ${name}`);
  }
  await call('open_binary', { path: resolve(target), provider_id: 'ghidra' }, 'open'); opened = true;
  for (const rva of procedures) {
    await call('analyze_function', { procedure: `0x${(base + BigInt(`0x${rva}`)).toString(16)}` }, rva);
  }
  completed = retained.length === 0;
} finally {
  try {
    if (opened) {
      const analysesFinished = calls.filter(c => c.name === 'analyze_function').length === procedures.length;
      completed = false;
      await call('close_binary', { snapshot_path: resolve(output, 'snapshot.json') }, 'close');
      if (retained.length) {
        const snapshot = JSON.parse(await readFile(resolve(output, 'snapshot.json'), 'utf8'));
        for (const item of retained) {
          const evidence = recoverAnalysis(snapshot, item.evidenceId);
          await writeFile(resolve(output, `${item.label}.recovered-evidence.json`), JSON.stringify(evidence, null, 2), { flag: 'wx' });
        }
      }
      completed = analysesFinished && calls.filter(c => c.name === 'analyze_function' && c.isError).length === retained.length;
    }
  } finally {
    if (connected) await client.close();
    await writeFile(resolve(output, 'manifest.json'), JSON.stringify({ sha256, capture, image_base: `0x${base.toString(16)}`, calls, retained, completed, finished_at: new Date().toISOString(), game_executed: false }, null, 2));
  }
}
