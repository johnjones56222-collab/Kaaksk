// Bundles src/ into one self-contained public/index.html (no network needed to play).
//   node build.mjs            production build
//   node build.mjs --dev      unminified, with inline source map
//   node build.mjs --artifact <dir>   also write a claude.ai artifact fragment + game.js
import { build } from 'esbuild';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const dev = args.includes('--dev');
const artifactIdx = args.indexOf('--artifact');
const artifactDir = artifactIdx >= 0 ? args[artifactIdx + 1] : null;

const result = await build({
  entryPoints: [path.join(root, 'src/main.js')],
  bundle: true,
  format: 'iife',
  target: ['es2020'],
  minify: !dev,
  sourcemap: dev ? 'inline' : false,
  write: false,
  legalComments: 'none',
  logLevel: 'warning',
});
const js = result.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
const css = await readFile(path.join(root, 'src/styles.css'), 'utf8');
const template = await readFile(path.join(root, 'src/index.template.html'), 'utf8');

const page = template.replace('/*__CSS__*/', () => css).replace('/*__JS__*/', () => js);
await mkdir(path.join(root, 'public'), { recursive: true });
await writeFile(path.join(root, 'public/index.html'), page);
console.log(`public/index.html  ${(page.length / 1024).toFixed(0)} KB${dev ? ' (dev)' : ''}`);

if (artifactDir) {
  // The artifact host supplies <html>/<head>/<body>; publish the body content
  // with the script as a sibling file.
  const head = template.match(/<head>([\s\S]*?)<\/head>/i)[1];
  const body = template.match(/<body[^>]*>([\s\S]*?)<\/body>/i)[1];
  const title = head.match(/<title>[\s\S]*?<\/title>/i)[0];
  const fragment = [
    title,
    '<meta name="claude:viewport-fit-sides" content="cover">',
    `<style>${css}</style>`,
    body.replace(/<script>\s*\/\*__JS__\*\/\s*<\/script>/, '<script src="game.js"></script>'),
  ].join('\n');
  await mkdir(artifactDir, { recursive: true });
  await writeFile(path.join(artifactDir, 'cold-wake.html'), fragment);
  await writeFile(path.join(artifactDir, 'game.js'), result.outputFiles[0].text);
  console.log(`artifact written to ${artifactDir}`);
}
