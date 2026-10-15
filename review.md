# Code Review — vCard Viewer

First-pass review 2026-09-04. **Remediated 2026-09-17** on branch
`cleanup/2026-09-17`, five commits, not pushed. This file is now the record of
that pass rather than a to-do list.

## What this project is

A Qt6 desktop application that opens `.vcf` files, parses the contacts, and
displays them in a dark-themed list-plus-details layout with search and a
recent-files menu.

## Headline

Every one of the three P1 findings was real, and the first two had been
producing visibly wrong output since the project's only commit. They were
reproduced against the original code before anything was changed.

The parser now lives in `src/vcard.cpp` with no Qt in it and has 122 checks
against it. **The Qt code has not been compiled**, because Qt is not installed
on this machine — see *Not verified* below, which is the one thing a future
session needs to act on.

## What was fixed

### P1 — `NOTE` and `NICKNAME` were parsed as the structured name

Confirmed exactly as the first-pass review described it. Property names were
matched with `startsWith` in an if/else chain, so `key.startsWith("N")` caught
`NOTE` before the `NOTE` branch was ever reached. For any card whose `NOTE:`
line came before its `FN:` line, the note text became the contact's displayed
name, and the note itself vanished.

Fixed by matching the full property name, after splitting off the group prefix
(`item1.EMAIL`) and the parameters (`;TYPE=CELL`). Order no longer decides
correctness.

### P1 — line unfolding was wrong, and quadratic

Also confirmed. `lines.indexOf(trimmedLine)` searched the entire line list by
text to find the line currently being processed, which:

- found the *first* line with that text, so two contacts sharing an `ORG:` line
  took each other's continuations;
- found nothing at all when the line had surrounding whitespace, because the
  list held untrimmed strings and the needle was trimmed, so folding was
  silently skipped;
- cost O(n²) over the file.

Replaced with a single unfolding pass up front, per RFC 6350 §3.2. The same
pass resolves quoted-printable soft line breaks (a trailing `=`), which is the
other reason long values were arriving truncated from 2.1 exports.

### P1 — opening a contact file made outbound network requests

Confirmed, and worth restating plainly: a `.vcf` someone sends you chooses the
URL, so the old code told the sender your IP address and the moment you opened
their file, over plain `http`, with no timeout and no size limit.

The first-pass review's fourth point about this was the important one and was
also correct: the *common* form of `PHOTO` is inline base64, which is not a URL,
so the README's "Name and profile photos" only ever worked for the rare URL
case.

Now: `vcard::parse` classifies the value and reports `PHOTO_KIND` as `embedded`
or `remote`, covering `ENCODING=b`, the bare 2.1 `PHOTO;JPEG;BASE64` form and
4.0 `data:` URIs. Embedded photos are decoded in-process and displayed, with no
network involved. Remote photos are not fetched unless the `photos/allowRemote`
setting is turned on, and then only over `https`, with a ten-second timeout and
a five-megabyte cap.

### P2 — parameters, quoted-printable

Parameters are parsed properly now (name, group, parameter list, quoted
parameter values). Quoted-printable values are decoded, so accented names stop
reading `=C3=A9`.

One deliberate choice: the stored key for a repeated property still carries its
`TYPE` parameters — `PHONE_0_TEL;TYPE=CELL` — because the details panel derives
its labels from that text and rewriting the panel would have been a UI change
that could not be compiled here. The difference is that the key is now *built*
from the parsed parameters rather than being the raw text, so 2.1 bare
parameters (`TEL;HOME;VOICE`) and 4.0 comma lists (`TYPE="cell,voice"`) now
produce the same labels the 3.0 form always did, and `ENCODING`/`CHARSET` no
longer leak into the label.

### P2 — IDE state, P3 — escapes, `LICENSE`, the `ITEM` precedence smell

`.qtcreator/project.json.user`, `.vscode/settings.json` and `.vscode/tasks.json`
are untracked and ignored; all three hard-code paths to a `D:/Qt/6.9.3` that is
not on this machine, and `tasks.json` additionally pointed at
`E:/Backups/code/Qt/vCard`, from before the project moved under `C++/`.
`.qtcreator/project.json` is kept — four lines, no machine paths. The 89MB
`build/` tree is deleted from the working copy.

Backslash escapes are resolved, without disturbing the unescaped `;` and `,`
that give `N`, `ADR` and `CATEGORIES` their structure. `LICENSE` added — the
README had promised an MIT licence and a `LICENSE` file since the first commit.
The `EMAIL || ITEM && contains` precedence smell is gone with the prefix chain
that contained it.

### P3 — the "vCard 3.0 and 4.0" claim

Softened rather than implemented, and the README now says what is and is not
handled. `KIND`, `MEMBER` and `RELATED` are parsed and searchable but have no
place in the details panel, and `TEL;VALUE=uri` shows the URI as written.

## Changes beyond the review's list

Three, all small, all in the commits:

1. **`FN` now wins over `N` as the display name.** Previously whichever came
   first set it, so the list pane and the details header disagreed on most real
   files (the details header always preferred `FN`). RFC 6350 makes `FN` the
   display name. This changes what the contact list shows for cards where `N`
   precedes `FN`, which is most of them — it now shows the same thing the
   details panel does.
2. **The search box skips the photo fields.** New only because embedded photos
   are now stored whole: scanning tens of kilobytes of base64 on every keystroke
   matches nothing anyone meant.
3. **One `QNetworkAccessManager` for the application** instead of a new one per
   contact selection, parented to the window. The old code leaked one per click.

## Deliberately not done

- **No in-application setting for remote photos.** The first-pass review asked
  for "opt-in with a visible setting". A visible setting means a new menu item,
  and the standing constraint for this archive is business logic only, no UI
  changes. `photos/allowRemote` is documented in the README instead. If a UI
  toggle is wanted, that is a one-line addition to `setupMenuBar()` and a
  decision for the author.
- **No new UI strings.** A remote photo that will not be fetched shows the
  existing `No Image` placeholder rather than a new "not loaded" message. It is
  accurate — no image is available without going to the network — but it is
  quiet about *why*, and a reader of the code should know that was a choice.
- **`CHARSET` is still ignored.** Quoted-printable values are decoded as UTF-8
  whatever the file says. A Latin-1 export will still show mojibake. Fixing it
  properly means a transcoding table or a Qt dependency in the parser, and the
  parser being Qt-free is what makes it testable. Documented as a limitation.
- **The details panel was not rewritten.** It repeats a nine-line
  header-label-styling block seven times and would read better as a helper.
  That is a UI file and could not be compiled here, so it was left alone.

## Not verified — for whoever is next

**The Qt half of this project has never been compiled.** `D:/Qt` does not exist
on this machine and Qt is a large install, so the changes to
`mainwindow_view.cpp`, `mainwindow.cpp`, `mainwindow.h` and
`mainwindow_parser.cpp` have been through a careful read and nothing else. They
are not large — the photo block, the search loop, one member, and a six-line
adapter — but they are unverified, in the same way
`C++/wxWidgets/mini-file-explorer` is.

If a Qt6 install becomes available, this is a twenty-minute job:

```sh
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/mingw_64
cmake --build build
```

Then open a `.vcf` with an inline photo and check that it draws, and one with
an `http://` photo URL and check — with a packet capture or an offline machine
— that nothing leaves the box.

## Verifying the parser

```sh
cd "C++/Qt/vCard" && sh tests/build.sh
```

Expected: `122/122 checks passed`, exit 0. Needs only a C++17 compiler; g++
10.3 at `D:\TDM-GCC-64\bin` was used.

To confirm the regression tests were not written to fit the fix: build the test
file against `git show dea0437:src/vcard.cpp`, the verbatim extraction of the
original parser, with the helper-level tests removed (they call `unfold`,
`decodeQuotedPrintable` and `unescapeText`, which that commit does not have).
The 24 parse-level tests then report **18 failing checks out of 70**, and every
one is a defect rather than a changed expectation.

## Action checklist

- [x] **P1** Match property names exactly so `NOTE` and `NICKNAME` stop being parsed as `N`.
- [x] **P1** Decode base64 photos locally; make any remote fetch opt-in, https-only, with a timeout.
- [x] **P1** Replace the `indexOf` lookahead with a single unfolding pass before parsing.
- [x] **P2** Split parameters off the property name; decode quoted-printable values.
- [x] **P2** Untrack `.qtcreator/*.user` and `.vscode/settings.json`; extend `.gitignore`.
- [x] **P3** Unescape `\n`, `\,`, `\;`; add parentheses to the `ITEM` condition; add a `LICENSE`.
- [x] **P3** Either implement the 4.0-specific forms or soften the README's "vCard 3.0 and 4.0" claim.
- [ ] **Open** Build the Qt application once and confirm the photo path, on a machine that has Qt.

## Licence, 2026-10-02

The author chose one licence for all of their projects: free for personal,
non-commercial use, modifiable for the user's own use, and not to be
distributed, original or modified. No standard licence says that, so `LICENSE`
is a short custom text, the same in every project apart from the year and what
it excludes. The README's licence section says the same. Anything above that
calls the licence an open question, or calls the project MIT, is superseded.

## CI, 2026-10-02

`.github/workflows/ci.yml` runs the parser tests on `ubuntu-latest` and in
MSYS2 on `windows-latest`, and builds the Qt 6 app with CMake on
`ubuntu-latest`. Nothing has been pushed, so it has not run on GitHub yet.
Every job was run before committing, from a fresh clone of this branch, the
way a runner would run it: Linux jobs in a throwaway Ubuntu 24.04 (the image
`ubuntu-latest` uses), Windows jobs here, each with only the tools the job
sets up. 122 on each, and the app built.
