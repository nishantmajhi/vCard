# vCard Viewer

A desktop application that opens `.vcf` contact files and shows what is in
them. It reads; it does not edit, sync or upload. Everything it displays comes
out of the file you opened.

![vCard Viewer](icons/icon-128.png)

## What it does

Open a `.vcf` file and the window splits in two: a searchable list of the
contacts on the left, the selected contact's details on the right. The details
panel shows the name, photo, phone numbers and email addresses with their type
labels, organisation, job title, address, birthday, website, categories and
notes. The last ten files you opened are on the **Options → Open Recent** menu,
and the most recent one is reopened when you start the application.

The search box filters the list as you type, matching any field of a contact,
not just the name.

## What it reads

`.vcf` files come in three versions and several dialects, and this parser
handles the parts of them that turn up in real exports:

- **vCard 2.1, 3.0 and 4.0.** Version is recorded and shown; nothing else
  depends on it.
- **Folded lines.** A long value split across several lines with a leading
  space or tab, per RFC 6350 §3.2, is rejoined.
- **Quoted-printable values.** `ENCODING=QUOTED-PRINTABLE` is decoded, including
  the soft line breaks (a trailing `=`) that Outlook and older phones emit.
  The bytes are assumed to be UTF-8; a `CHARSET` parameter naming anything else
  is not honoured.
- **Escaped text.** `\n`, `\,`, `\;` and `\\` are resolved, without disturbing
  the unescaped `;` and `,` that give `N`, `ADR` and `CATEGORIES` their
  structure.
- **Property parameters.** `TEL;TYPE=CELL` (3.0), `TEL;HOME;VOICE` (2.1) and
  `TEL;TYPE="cell,voice"` (4.0) all produce the same type labels.
- **Grouped properties.** Apple's `item1.EMAIL` form is treated as `EMAIL`.
- **Embedded photos.** `PHOTO;ENCODING=b`, the bare 2.1 `PHOTO;JPEG;BASE64`
  form, and 4.0 `data:` URIs are decoded and displayed. See *Photos* below for
  the case where the photo is a URL instead.

Properties it has no section for — `NICKNAME`, `X-` extensions and so on — are
still parsed and still searchable; they just are not drawn.

## Photos

A `.vcf` file can carry its photo inline, or it can name a URL and leave the
fetching to whoever opens the file. Phones export the first form; the second is
rare, and it is a problem.

**Inline photos are always shown.** They came with the file, and decoding them
needs no network.

**Remote photos are not fetched by default.** Anyone who sends you a contact
file chooses that URL, so fetching it tells them your IP address and the moment
you opened their file. The placeholder in the photo circle is all you get
unless you opt in:

```ini
; Windows: HKEY_CURRENT_USER\Software\ManishMajhi\vCard Viewer
; macOS:   ~/Library/Preferences/com.ManishMajhi.vCard Viewer.plist
; Linux:   ~/.config/ManishMajhi/vCard Viewer.conf
[photos]
allowRemote=true
```

With that set, a remote photo is fetched only over `https`, with a ten-second
timeout and a five-megabyte limit. Plain `http` URLs are never fetched.

There is no in-application setting for this, deliberately: it is the kind of
switch that should take a moment's thought, not a stray click.

## Building

Needs CMake 3.16+, a C++17 compiler, and Qt 5.15+ or Qt 6.

```sh
cmake -B build
cmake --build build
./build/vCard
```

On Windows with a MinGW Qt, point CMake at it:

```sh
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=/path/to/Qt/6.9.3/mingw_64
```

On Linux, the Qt packages are `qt6-base-dev` (Debian/Ubuntu),
`qt6-qtbase-devel` (Fedora) or `qt6-base` (Arch). On macOS, `brew install qt`.

## Tests

The parser is deliberately free of Qt: `src/vcard.cpp` turns the text of a file
into a list of contacts and knows nothing about widgets, sockets or the
filesystem. That is the part with the decisions in it, so that is the part with
the tests on it.

```sh
sh tests/build.sh
```

The script needs only a C++17 compiler — no Qt, no test framework, no network.
It builds `tests/vcard_test.cpp` against `src/vcard.cpp` and runs it, printing
a count and returning non-zero if anything failed. 122 checks at the time of
writing.

## Layout

| | |
| --- | --- |
| `src/vcard.{h,cpp}` | The parser. Plain C++17, no Qt. |
| `tests/` | Its test suite and the script that builds it. |
| `main.cpp` | Entry point and the dark stylesheet. |
| `mainwindow_setup.cpp` | Builds the widgets and the menus. |
| `mainwindow_file.cpp` | Open, close, and the recent-files list. |
| `mainwindow_parser.cpp` | Adapter between `vcard::parse` and Qt containers. |
| `mainwindow_view.cpp` | Draws the contact list and the details panel. |

## Limitations

- **Read-only.** There is no editing, creating, deleting or exporting.
- **`CHARSET` is ignored.** Quoted-printable values are decoded as UTF-8
  whatever the file claims, so a Latin-1 export with accented characters will
  show mojibake.
- **One value per property, except `TEL` and `EMAIL`.** A card with two `ORG`
  lines keeps the last one.
- **vCard 4.0 is parsed, not interpreted.** `KIND`, `MEMBER`, `RELATED` and
  friends are read and searchable but have no place in the details panel, and
  a `TEL;VALUE=uri` value is shown as the URI it is (`tel:+15550100`).
- **Structured values are shown, not laid out.** `ADR` is joined with commas
  rather than formatted per country.

## License

Free for personal, non-commercial use. You may use, study and modify this
software for yourself, but you may not use it commercially, and you may not
share or publish it, in its original form or modified. See [LICENSE](LICENSE)
for the full terms.

## Author

Manish Majhi — [@nishantmajhi](https://github.com/nishantmajhi)
