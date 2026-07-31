# Arch Linux packaging

Three PKGBUILDs are kept here. `PKGBUILD` and `PKGBUILD-git` are validated
in CI on every push/PR (`build-arch` job in
`.github/workflows/cmake-multi-platform.yml`) and, for `PKGBUILD`, built
again as a release artifact:

- **`PKGBUILD`** (`qt-msg-reader`) - a normal versioned package. `source`
  points at a GitHub release tag tarball (`v$pkgver`), so it only works once
  that tag actually exists.
- **`PKGBUILD-git`** (`qt-msg-reader-git`) - a VCS package that always builds
  the latest commit on the repo's default branch, using `pkgver()` +
  `git describe` the way most `-git` AUR packages do.
- **`PKGBUILD-bin`** (`qt-msg-reader-bin`) - repackages the prebuilt
  `.pkg.tar.zst` attached to a GitHub release instead of compiling anything.
  It builds nothing itself, so it has no `makedepends` and its `package()`
  is just an extract of the release asset's `usr/` tree. See "The `-bin`
  package" below for the tag/version caveat.
- **`SRCINFO-git`**, **`SRCINFO-bin`** - generated previews of what
  `PKGBUILD-git` / `PKGBUILD-bin` produce, kept here for reference only.
  They are *generated*, never hand-edited; after changing a PKGBUILD,
  refresh with
  `makepkg --printsrcinfo -p PKGBUILD-git > SRCINFO-git` (likewise for
  `-bin`). Note `SRCINFO-git`'s `pkgver` is the placeholder declared in the
  PKGBUILD (`--printsrcinfo` does not run `pkgver()`); the AUR clone's real
  `.SRCINFO` gets the `git describe` value once the source has been fetched.

## Building locally

```bash
cd packaging/arch
makepkg -si -p PKGBUILD       # versioned release, built from source
makepkg -si -p PKGBUILD-git   # latest git HEAD, built from source
makepkg -si -p PKGBUILD-bin   # prebuilt release asset, no compilation
```

## The `-bin` package

`PKGBUILD-bin` exists because the release workflow already publishes a real
Arch package (`qt-msg-reader-$pkgver-1-x86_64.pkg.tar.zst`), built natively
in the `archlinux:base-devel` container by the same `PKGBUILD` in this
directory. `qt-msg-reader-bin` just downloads that asset and unpacks its
`usr/` tree, so users get the identical binary, `.desktop` file and icons
without a local Qt build.

Two things to keep in mind when bumping it:

- **The release tag and the package version are not the same string.** The
  workflow tags a release from its `version` input (or
  `v<CMake version>-<short sha>`), while the `.pkg.tar.zst` filename comes
  from `CMakeLists.txt`'s `project(... VERSION ...)`. Tag `v1.0.1`
  currently ships an asset named `qt-msg-reader-1.0.0-1-x86_64.pkg.tar.zst`.
  The PKGBUILD therefore keeps `_tag` separate from `pkgver`, and *both*
  have to be updated by hand. Aligning the CMake version with the tag you
  release under would let `_tag` collapse into `"v$pkgver"`.
- **`sha256sums` must be a real checksum, not `SKIP`.** The AUR requires
  checksums for non-VCS sources, and here it is the only thing verifying the
  downloaded binary. Refresh it with `updpkgsums -p PKGBUILD-bin` after
  changing `_tag`/`pkgver`.

The release asset name is identical across releases, so `source` renames the
download locally (`$pkgname-$pkgver-$_tag.pkg.tar.zst::...`) to stop a shared
`SRCDEST` cache from reusing a stale file from a different tag.

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

Repeat with `PKGBUILD-git` under `qt-msg-reader-git`, and `PKGBUILD-bin`
under `qt-msg-reader-bin`, if you also want to publish the rolling and
prebuilt packages. Each is a separate AUR repo with its own remote; all
three may coexist, and the `provides`/`conflicts` entries in the `-git` and
`-bin` recipes are what stop them from being installed on top of each other.

Package names must match their PKGBUILD's `pkgname` exactly, so publish as
`qt-msg-reader-bin` (matching the existing `qt-msg-reader` base name), not
`qtmsgreader-bin`.
