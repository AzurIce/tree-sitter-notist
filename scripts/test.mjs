import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
const root = fileURLToPath(new URL('../', import.meta.url));
for (const directory of [root, `${root}/notist-code`, `${root}/notist-markdown`, `${root}/notist-markdown-inline`]) {
  const result = spawnSync('tree-sitter', ['test', '-p', '.'], { cwd: directory, stdio: 'inherit' });
  if (result.error) throw result.error;
  if (result.status !== 0) process.exit(result.status ?? 1);
}
await import('./test-highlight.mjs');
const result = spawnSync('cargo', ['test', '--locked', '-j8', '--', '--test-threads=4'], { cwd: root, stdio: 'inherit' });
if (result.error) throw result.error;
process.exit(result.status ?? 1);
