# Arch Linux packaging

Two PKGBUILDs are kept here, both validated in CI on every push/PR
(`build-arch` job in `.github/workflows/cmake-multi-platform.yml`) and, for
`PKGBUILD`, built again as a release artifact:

- **`PKGBUILD`** (`qt-msg-reader`) - a normal versioned package. `source`
  points at a GitHub release tag tarball (`v$pkgver`), so it only works once
  that tag actually exists.
- **`PKGBUILD-git`** (`qt-msg-reader-git`) - a VCS package that always builds
  the latest commit on the repo's default branch, using `pkgver()` +
  `git describe` the way most `-git` AUR packages do.
- **`SRCINFO-git`** - a generated preview of what `PKGBUILD-git` produces,
  kept here for reference only. It is *generated*, never hand-edited; after
  changing `PKGBUILD-git`, refresh it with
  `makepkg --printsrcinfo -p PKGBUILD-git > SRCINFO-git`. Note its `pkgver`
  is the placeholder declared in the PKGBUILD (`--printsrcinfo` does not run
  `pkgver()`); the AUR clone's real `.SRCINFO` gets the `git describe` value
  once the source has been fetched.

## Building locally

```bash
cd packaging/arch
makepkg -si -p PKGBUILD       # versioned release
makepkg -si -p PKGBUILD-git   # latest git HEAD
```

## How CI validates these

`makepkg` refuses to run as root and won't fetch a tag that doesn't exist
yet, so the `build-arch` CI job doesn't invoke either PKGBUILD completely
unmodified:

- For `PKGBUILD-git`, it points the git source at the commit under test
  (`git+file://$GITHUB_WORKSPACE`) instead of the network, so it always
  builds against the exact code in the PR/push instead of whatever is
  already on GitHub.
- For `PKGBUILD`, it sets `pkgver` to the current `CMakeLists.txt` project
  version and supplies a locally-generated source tarball (via
  `git archive`) instead of downloading the corresponding release tag, since
  that tag may not exist yet (or ever, for non-release pushes).

Both are build-only smoke tests of the `build()`/`package()` recipes; the
`.pkg.tar.zst` produced from the (unmodified) `PKGBUILD` copy is what
actually gets uploaded as a build artifact and attached to GitHub Releases.

## Publishing to the AUR

This repo is not the AUR package repo - the AUR expects its own git remote
(`ssh://aur@aur.archlinux.org/qt-msg-reader.git`) containing just a
`PKGBUILD` and a generated `.SRCINFO` at the repository root. To cut a real
AUR release once there's a tagged version worth publishing:

1. Bump `pkgver` in `PKGBUILD` here to match the tag, and run
   `updpkgsums` (from `pacman-contrib`) to replace the `SKIP` checksum with
   the real one.
2. Clone (or add as a remote) `ssh://aur@aur.archlinux.org/qt-msg-reader.git`
   and copy `PKGBUILD` into it.
3. Run `makepkg --printsrcinfo > .SRCINFO` in that AUR clone.
4. Commit and push both files to the AUR remote.

Repeat with `PKGBUILD-git` under `qt-msg-reader-git` on the AUR if you also
want to publish the rolling package.
