import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const temporary = mkdtempSync(join(tmpdir(), 'notist-highlight-'));
try {
  const config = join(temporary, 'config.json');
  writeFileSync(config, JSON.stringify({ 'parser-directories': [dirname(root.replace(/\/$/, ''))] }));
  for (const [file, calls] of [['showcase.nmd', 4], ['guide.notmd', 1]]) {
    const result = spawnSync('tree-sitter', ['highlight', '--config-path', config,
      '--html', '--css-classes', `examples/${file}`], { cwd: root, encoding: 'utf8' });
    if (result.error) throw result.error;
    assert.equal(result.status, 0, result.stderr);
    assert.doesNotMatch(result.stderr, /No language found|Failed to/);
    assert.equal([...result.stdout.matchAll(/class='function[^']*'>badge<\/span>/g)].length,
      calls, `${file}: calls must highlight through block, inline and recursive body injections`);
    assert.doesNotMatch(result.stdout, /class='function[^']*'>(?:hidden|bad|not-a-call)<\/span>/);
  }
  console.log('Markdown extension discovery and recursive highlighting passed.');
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
