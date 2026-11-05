# Omasign

Sign a PDF by dragging a rectangle over the page. Omasign puts a real
cryptographic signature in the file and draws it the way Acrobat and Foxit draw
theirs: name set large on the left, details in small type on the right, a faint
seal behind both. No border, no grey box, no name clipped off halfway through a
letter.

It is a small Qt 6 desktop app. It signs with your own key, out of the same NSS
key store Chrome and Okular already use, and it uploads nothing.

## Install

On Fedora 44 (x86_64), install the RPM from the latest release:

```
sudo dnf install https://github.com/navh/omasign/releases/download/v0.1.0/omasign-0.1.0-2.fc44.x86_64.rpm
```

dnf pulls in Qt 6, poppler-qt6 and nss-tools. On another Fedora release,
rebuild the source RPM from the same release:

```
sudo dnf install rpm-build dnf-plugins-core
sudo dnf builddep omasign-0.1.0-2.fc44.src.rpm
rpmbuild --rebuild omasign-0.1.0-2.fc44.src.rpm
sudo dnf install ~/rpmbuild/RPMS/x86_64/omasign-*.rpm
```

On other distributions, `bin/deps` installs the build and runtime dependencies,
then `bin/build` produces `build/omasign`.

## Setting up

The first run asks for the name you sign with and how the signature should look:

- **Typed** — your name set in sans-serif, auto-fitted to whatever
  rectangle you draw. This is what the Acrobat and Foxit signatures on most
  institutional forms look like.
- **Drawn** — draw it with a mouse, trackpad, or stylus. Omasign stores the
  result as scalable stroke data, so it stays crisp at any size on any page.

Omasign then creates a self-signed certificate in your personal NSS key store at
`~/.pki/nssdb`.

## Signing

1. Open a PDF, or drop one on the window.
2. Drag a rectangle where the signature goes. The preview under the cursor is
   the artwork that will be embedded, not an approximation.
3. Let go.

Omasign writes a new file next to the original, named like
`contract-signed-20260827-114233.pdf`, and reopens it, so signing a second place
on the same document builds on the first. Signatures already on the document are
left intact: a form can go round a committee and come back with everyone's
signature still verifying.

Encrypted PDFs with an empty user password (which is what most "protected"
institutional forms are) are handled without asking.

## The mark

The faint mark behind the signature is a Japanese seal reading 御任之印, set the
way a four-character hanko is: down the right column, then down the left. It is
drawn at 25%, which `appearance/watermarkOpacity` can raise or lower, and
`appearance/watermark` can replace it with an SVG of your own.

## About certificate trust

A reader will report an Omasign signature as **valid**, the document has not
been altered since you signed it, and the issuer as **untrusted**, because you
issued the certificate yourself. That is also what most institutional signatures
report. Nothing about a self-signed certificate makes the signature weaker; it
only means no third party vouches for who you are.

If you are issued a real document-signing certificate, import it and pick it in
Settings:

```
pk12util -i yourcert.p12 -d sql:$HOME/.pki/nssdb
```

Anything in that key store with a private key shows up in the certificate list.

## Configuration

`~/.config/omasign/omasign.conf`, alongside `signature.json` for drawn ink.
Everything in it is reachable from Settings except these, which are there for
people who want them:

| Key | Meaning |
| --- | --- |
| `appearance/watermark` | Path to an SVG to use instead of the Omasign seal, `none` disables it. |
| `appearance/watermarkOpacity` | How strongly that mark is drawn, `0`–`1`. Default `0.25`. |
| `appearance/splitRatio` | Share of the width given to the mark, `0.25`–`0.85`. Default `0.52`. |
| `appearance/markColour` | The signature's colour. Default `#131313`. |
| `appearance/metaColour` | The audit block's colour. Default `#3a3a3a`. |
| `appearance/fontFamily` | Default `Inter`, which is bundled. |
| `identity/nssDir` | Key store location. Default `sql:$HOME/.pki/nssdb`. |

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-svg`
- `poppler-qt6`, built with NSS signing support
- `certutil`, from `nss` on Arch and `nss-tools` on Fedora

Omasign follows the desktop's dark and light setting.

## Fonts and attribution

The Inter font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. It is copyright The Inter Project Authors. Bundling it means a
signature looks identical whatever fonts a machine happens to have.

The glyph outlines in the seal and the application icon come from Noto Serif CJK
JP, copyright Adobe, also under the SIL Open Font License 1.1; see
`fonts/OFL-Noto.txt`. The font itself is not bundled, only the four outlines, so
the mark renders on a machine with no CJK font installed.

## Development

```
bin/deps     # install dependencies (needs root)
bin/build    # build to build/omasign
bin/test     # build and run the test suite
bin/install  # build and install the Arch package
```

The tests cover the parts that are easy to get quietly wrong: that a long name
is never clipped, that the artwork is transparent at the edges, that poppler
draws neither border nor background, that a second signature does not invalidate
the first, that drawing a signature does not scroll the view out from under the
pen, and that the interface loads without QML warnings. They build a throwaway
key store, so they never touch yours.

`OMASIGN_SHOT_DIR=/tmp/shots bin/test` also writes a screenshot of the
interface, which is handy when changing the layout.

## Licence

MIT. See `LICENSE`.
