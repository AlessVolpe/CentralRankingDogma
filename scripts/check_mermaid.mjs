// Validate GitHub-native Mermaid blocks without rendering or a browser.
import fs from 'node:fs';
import path from 'node:path';
import mermaid from 'mermaid';

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
mermaid.initialize({ startOnLoad: false, securityLevel: 'strict' });
let count = 0;
for (const file of files) {
  const source = fs.readFileSync(file, 'utf8');
  for (const match of source.matchAll(/```mermaid\s*\n([\s\S]*?)\n```/g)) {
    try {
      await mermaid.parse(match[1]);
      count++;
    } catch (error) {
      console.error(`${path.relative(root, file)}: invalid Mermaid: ${error.message}`);
      process.exitCode = 1;
    }
  }
}
if (!count) {
  console.error('No Mermaid diagrams were found.');
  process.exitCode = 1;
} else {
  console.log(`Validated ${count} Mermaid diagrams.`);
}
