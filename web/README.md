# Zep documentation website

Requires Node.js 22.12+ and uses the pinned VitePress 2 alpha release.

```sh
npm ci
npm run dev
```

Build with `npm run build` and serve the output with `npm run preview`.
Production files are written to `docs/.vitepress/dist/`.

Edit pages in `docs/` and navigation in `docs/.vitepress/config.mts`.
Zep highlighting reuses the editor extension's grammar.
