# Changelog

Changes in this copy (gabeparra/ChessCadets) on top of the team repository.

## 2026-10-27
- Brought this copy up to the team's final code (LeineckerGames/ChessCadets main, 2026-07-31). The earlier
  July 8 state is kept as the tag `snapshot-2026-07-08`.
- Removed the third-party marketplace packs (CyberpunkIndustries, Fab, NaturePack) and the two display fonts,
  since their licenses don't allow redistribution.
- New README with a banner, screenshots and a write-up of the AI and my part. The team's README is now
  `Docs/DEVELOPMENT.md`.
- README images are stored in plain git instead of LFS.
- Added Pulse's MIT header to `notation.cpp`, which was missing it.
- Dropped the Android file server token from `Config/DefaultEngine.ini`; Unreal makes a new one when it needs it.
