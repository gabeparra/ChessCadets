# Contributing to ChessCadets

Short version: **fresh clone of this repo, own branch, PR on GitHub, one map per person.**

## One-time setup

1. Clone `github.com/gabeparra/chesscadetsremake` fresh. **Do not reuse a clone of the old
   LeineckerGames repo** — its history is incompatible (rewritten 2026-07-09) and pulling
   from it breaks the repo for everyone.
2. Install Git LFS (`git lfs install`), then `git lfs pull` inside the clone.
3. Old clones and forks of the old repo: archive or delete them.

## Day-to-day workflow

1. Branch off `main` (`git checkout -b yourname-feature`).
2. Commit, push your branch, open a **Pull Request**, merge it **on GitHub** — never merge
   locally into `main`.
3. After pulling, run `git lfs pull` before opening the editor.

## Binary assets (.uasset / .umap)

- Git cannot merge them — conflicts throw away one side entirely.
- So: **one person per map/asset at a time.** Say which map you're working on in chat
  before you start.
- World Partition maps (L_Field): commit `Content/__ExternalActors__/...` together with
  the `.umap`.
- If a map or mesh looks broken after a pull, check whether the file on disk is LFS
  pointer text (open it in a text editor — real assets are binary). If it's text:
  `git lfs pull` fixes it. **Never re-save/commit over it.**

## If something looks wrong

- `git pull` shows hundreds of unexpected commits → STOP, don't push, ask Gabriel.
- Asset conflicts you didn't expect → STOP, ask whose version wins.
- When in doubt, a fresh clone is cheap; a broken `main` is not.

