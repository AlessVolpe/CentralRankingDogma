// Validate GitHub-native Mermaid blocks without rendering a diagram.
// Mermaid's DOMPurify label sanitizer requires browser DOM APIs at import time.
// Install a small headless DOM BEFORE importing Mermaid (Node 22 has no window).
import fs from 'node:fs';
import path from 'node:path';
import { JSDOM } from 'jsdom';

const dom = new JSDOM('<!doctype html><html><body></body></html>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;

// Static imports execute before the DOM setup above, so this MUST be dynamic.
const { default: mermaid } = await import('mermaid');
mermaid.initialize({ startOnLoad: false, securityLevel: 'strict' });

// Distinguish a broken Node/Mermaid environment from invalid source diagrams.
try {
  await mermaid.parse('flowchart TD\nA["Start"] --> B["End"]');
} catch (error) {
  console.error(`Mermaid parser initialization failed: ${error.message}`);
  process.exit(1);
}

const root = process.cwd();
const files = [];
function scan(dir) {
  for (const ent of fs.readdirSync(dir, { withFileTypes: true })) {
    if (ent.name === 'archive' || ent.name === 'node_modules') continue;
    const file = path.join(dir, ent.name);
    if (ent.isDirectory()) scan(file);
    else if (file.endsWith('.md')) files.push(file);
  }
}
scan(path.join(root, 'docs'));
files.push(path.join(root, 'README.md'));

let found = 0;
let validated = 0;
for (const file of files) {
  const source = fs.readFileSync(file, 'utf8');
  for (const match of source.matchAll(/```mermaid\s*\n([\s\S]*?)\n```/g)) {
    found++;
    try {
      await mermaid.parse(match[1]);
      validated++;
    } catch (error) {
      console.error(`${path.relative(root, file)}: invalid Mermaid: ${error.message}`);
      process.exitCode = 1;
    }
  }
}
if (found === 0) {
  console.error('No Mermaid diagrams were found.');
  process.exitCode = 1;
} else {
  console.log(`Validated ${validated}/${found} Mermaid diagrams.`);
}
dom.window.close();
