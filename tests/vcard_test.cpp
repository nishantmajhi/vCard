// Tests for the pure-C++ vCard parser. No framework: a counter, a few check
// helpers, and a main() that reports failures and returns non-zero.
//
//   sh tests/build.sh
//
#include "../src/vcard.h"

#include <iostream>
#include <string>
#include <vector>

static int g_checks = 0;
static int g_failures = 0;
static std::string g_currentTest;

static void failed(const std::string &what, const std::string &expected, const std::string &actual)
{
    ++g_failures;
    std::cout << "  FAIL  " << g_currentTest << ": " << what << "\n"
              << "        expected: [" << expected << "]\n"
              << "        actual:   [" << actual << "]\n";
}

static void checkEqual(const std::string &what, const std::string &expected, const std::string &actual)
{
    ++g_checks;
    if (expected != actual) {
        failed(what, expected, actual);
    }
}

static void checkCount(const std::string &what, std::size_t expected, std::size_t actual)
{
    ++g_checks;
    if (expected != actual) {
        failed(what, std::to_string(expected), std::to_string(actual));
    }
}

static void checkTrue(const std::string &what, bool condition)
{
    ++g_checks;
    if (!condition) {
        failed(what, "true", "false");
    }
}

// Reads a field, reporting a readable miss rather than inserting an empty one.
static std::string field(const vcard::Contact &c, const std::string &key)
{
    auto it = c.find(key);
    return it == c.end() ? std::string("<missing>") : it->second;
}

static bool has(const vcard::Contact &c, const std::string &key)
{
    return c.find(key) != c.end();
}

// Finds the one entry whose key begins with prefix, so tests do not have to
// hard-code the parameter text that ends up inside PHONE_0_<key>.
static std::string fieldByPrefix(const vcard::Contact &c, const std::string &prefix)
{
    for (const auto &kv : c) {
        if (kv.first.compare(0, prefix.size(), prefix) == 0) {
            return kv.second;
        }
    }
    return "<missing>";
}

static std::string keyByPrefix(const vcard::Contact &c, const std::string &prefix)
{
    for (const auto &kv : c) {
        if (kv.first.compare(0, prefix.size(), prefix) == 0) {
            return kv.first;
        }
    }
    return "<missing>";
}

static void test(const std::string &name)
{
    g_currentTest = name;
}

// ---------------------------------------------------------------------------
// review.md P1 #1: NOTE and NICKNAME both start with "N", so the prefix chain
// routed them into the N handler and the note text became the displayed name.

static void testNoteIsNotTheName()
{
    test("NOTE before FN does not become the contact name");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "NOTE:Met at the Berlin conference\r\n"
        "FN:Ada Lovelace\r\n"
        "N:Lovelace;Ada;;;\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("NAME", "Ada Lovelace", field(contacts[0], "NAME"));
    checkEqual("NOTE", "Met at the Berlin conference", field(contacts[0], "NOTE"));
    checkEqual("N", "Lovelace;Ada;;;", field(contacts[0], "N"));
}

static void testNicknameIsNotTheName()
{
    test("NICKNAME is kept separate from N");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "NICKNAME:Sparky\r\n"
        "N:Hopper;Grace;;;\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("NAME", "Grace Hopper", field(contacts[0], "NAME"));
    checkEqual("N", "Hopper;Grace;;;", field(contacts[0], "N"));
    checkEqual("NICKNAME", "Sparky", field(contacts[0], "NICKNAME"));
}

// ---------------------------------------------------------------------------
// review.md P1 #3: the continuation lookahead used indexOf-by-value over the
// untrimmed line list, so it found the first line anywhere in the file with
// matching text, and found nothing at all when the line had stray whitespace.

static void testFoldedLineIsJoined()
{
    test("a folded value is joined to the line it continues");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Katherine Johnson\r\n"
        "NOTE:Computed the trajectory for Friendship 7 and checked the\r\n"
        "  numbers the electronic computer produced.\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("NOTE",
               "Computed the trajectory for Friendship 7 and checked the"
               " numbers the electronic computer produced.",
               field(contacts[0], "NOTE"));
}

static void testDuplicateLinesFoldIndependently()
{
    test("two contacts sharing a line text each get their own continuation");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:First Person\r\n"
        "ORG:Acme\r\n"
        "END:VCARD\r\n"
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Second Person\r\n"
        "ORG:Acme\r\n"
        " Corporation\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 2, contacts.size());
    if (contacts.size() < 2) {
        return;
    }
    checkEqual("first ORG", "Acme", field(contacts[0], "ORG"));
    checkEqual("second ORG", "AcmeCorporation", field(contacts[1], "ORG"));
}

static void testFoldingIsNotQuadratic()
{
    test("a large file parses without an O(n^2) lookahead");
    std::string vcf;
    for (int i = 0; i < 4000; ++i) {
        vcf += "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Contact " + std::to_string(i)
             + "\r\nORG:Shared Organisation Name\r\nEND:VCARD\r\n";
    }
    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 4000, contacts.size());
}

// ---------------------------------------------------------------------------
// review.md, "Other parser gaps": parameters were never split off the property
// name, so an exact match on the property has to happen after they are removed.

static void testParametersDoNotDefeatPropertyMatching()
{
    test("parameters on the property name do not change how it is matched");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Alan Turing\r\n"
        "TEL;TYPE=CELL:+44 20 7946 0958\r\n"
        "EMAIL;TYPE=INTERNET;TYPE=HOME:alan@example.org\r\n"
        "ADR;TYPE=HOME:;;2 Adelaide Road;London;;NW1;UK\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("phone", "+44 20 7946 0958", fieldByPrefix(contacts[0], "PHONE_0_"));
    checkEqual("email", "alan@example.org", fieldByPrefix(contacts[0], "EMAIL_0_"));
    checkTrue("ADR present", has(contacts[0], "ADR"));
    // The details panel recovers type labels from the stored key, so the
    // parameter text has to survive inside it.
    checkTrue("phone key keeps TYPE=CELL",
              keyByPrefix(contacts[0], "PHONE_0_").find("TYPE=CELL") != std::string::npos);
}

static void testGroupedPropertiesAreUngrouped()
{
    test("Apple-style item1.EMAIL grouping is stripped");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Radia Perlman\r\n"
        "item1.EMAIL;type=INTERNET:radia@example.net\r\n"
        "item2.URL:https://example.net\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("email", "radia@example.net", fieldByPrefix(contacts[0], "EMAIL_0_"));
    checkEqual("grouped URL", "https://example.net", field(contacts[0], "URL"));
}

// ---------------------------------------------------------------------------
// review.md, "Other parser gaps": quoted-printable is what puts =C3=A9 on the
// screen in place of an accented character.

static void testQuotedPrintableIsDecoded()
{
    test("ENCODING=QUOTED-PRINTABLE values are decoded");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:2.1\r\n"
        "FN;CHARSET=UTF-8;ENCODING=QUOTED-PRINTABLE:Ren=C3=A9e Fran=C3=A7ois\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("FN", "Ren\xC3\xA9""e Fran\xC3\xA7ois", field(contacts[0], "FN"));
}

static void testQuotedPrintableSoftLineBreak()
{
    test("a quoted-printable soft line break continues the value");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:2.1\r\n"
        "FN:Outlook Export\r\n"
        "NOTE;ENCODING=QUOTED-PRINTABLE:First half of the note =\r\n"
        "and the second half.\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("NOTE", "First half of the note and the second half.",
               field(contacts[0], "NOTE"));
}

// ---------------------------------------------------------------------------
// review.md P3: escaped characters appeared literally.

static void testEscapesAreUnescaped()
{
    test("backslash escapes in text values are resolved");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Escape Test\r\n"
        "NOTE:Line one\\nLine two\\; still two\\, and a backslash \\\\ here\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("NOTE", "Line one\nLine two; still two, and a backslash \\ here",
               field(contacts[0], "NOTE"));
}

static void testStructuredNameKeepsItsSeparators()
{
    test("N keeps its component separators and builds a display name");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "N:van Rossum;Guido;;Mr.;\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("NAME", "Guido van Rossum", field(contacts[0], "NAME"));
    checkEqual("N", "van Rossum;Guido;;Mr.;", field(contacts[0], "N"));
}

// ---------------------------------------------------------------------------
// review.md P1 #2: the common form of PHOTO is inline base64, which the viewer
// treated as a URL and therefore never displayed; a remote URL in a file
// someone sent you is a tracking pixel.

static void testEmbeddedPhotoIsRecognised()
{
    test("PHOTO;ENCODING=b is reported as embedded data, not a URL");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Photo Owner\r\n"
        "PHOTO;ENCODING=b;TYPE=JPEG:/9j/4AAQSkZJRgABAQEAYABgAAD/\r\n"
        " 2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0a\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("PHOTO_KIND", "embedded", field(contacts[0], "PHOTO_KIND"));
    checkEqual("PHOTO",
               "/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0a",
               field(contacts[0], "PHOTO"));
}

static void testDataUriPhotoIsRecognised()
{
    test("a vCard 4.0 data: URI photo is reported as embedded data");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:4.0\r\n"
        "FN:Photo Owner\r\n"
        "PHOTO:data:image/png;base64,iVBORw0KGgo=\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("PHOTO_KIND", "embedded", field(contacts[0], "PHOTO_KIND"));
    checkEqual("PHOTO", "iVBORw0KGgo=", field(contacts[0], "PHOTO"));
    checkEqual("PHOTO_TYPE", "image/png", field(contacts[0], "PHOTO_TYPE"));
}

static void testRemotePhotoIsReportedAsRemote()
{
    test("an http PHOTO value is reported as remote rather than fetched");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Tracking Pixel\r\n"
        "PHOTO;VALUE=URI:http://tracker.example.com/pixel?id=42\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("PHOTO_KIND", "remote", field(contacts[0], "PHOTO_KIND"));
    checkEqual("PHOTO", "http://tracker.example.com/pixel?id=42", field(contacts[0], "PHOTO"));
}

// ---------------------------------------------------------------------------
// Structural cases, to pin down what the parser already got right.

static void testMultipleContacts()
{
    test("several contacts in one file");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:One\r\nEND:VCARD\r\n"
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Two\r\nEND:VCARD\r\n"
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Three\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 3, contacts.size());
    if (contacts.size() < 3) {
        return;
    }
    checkEqual("first", "One", field(contacts[0], "FN"));
    checkEqual("second", "Two", field(contacts[1], "FN"));
    checkEqual("third", "Three", field(contacts[2], "FN"));
}

static void testRepeatedPropertiesAreNumberedInOrder()
{
    test("repeated TEL and EMAIL properties keep their file order");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Many Numbers\r\n"
        "TEL;TYPE=HOME:1111\r\n"
        "TEL;TYPE=WORK:2222\r\n"
        "TEL;TYPE=CELL:3333\r\n"
        "EMAIL;TYPE=HOME:a@example.com\r\n"
        "EMAIL;TYPE=WORK:b@example.com\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("phone 0", "1111", fieldByPrefix(contacts[0], "PHONE_0_"));
    checkEqual("phone 1", "2222", fieldByPrefix(contacts[0], "PHONE_1_"));
    checkEqual("phone 2", "3333", fieldByPrefix(contacts[0], "PHONE_2_"));
    checkEqual("email 0", "a@example.com", fieldByPrefix(contacts[0], "EMAIL_0_"));
    checkEqual("email 1", "b@example.com", fieldByPrefix(contacts[0], "EMAIL_1_"));
}

static void testCounterResetsBetweenContacts()
{
    test("phone numbering restarts for each contact");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:One\r\nTEL:1111\r\nEND:VCARD\r\n"
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Two\r\nTEL:2222\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 2, contacts.size());
    if (contacts.size() < 2) {
        return;
    }
    checkEqual("second contact phone 0", "2222", fieldByPrefix(contacts[1], "PHONE_0_"));
}

static void testUnterminatedCardIsDropped()
{
    test("a card with no END:VCARD is not emitted");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Complete\r\nEND:VCARD\r\n"
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Truncated\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
}

static void testPropertiesOutsideACardAreIgnored()
{
    test("properties before BEGIN or after END do not leak into a contact");
    const std::string vcf =
        "ORG:Stray Organisation\r\n"
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Real Contact\r\nEND:VCARD\r\n"
        "NOTE:Stray note\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkTrue("no stray ORG", !has(contacts[0], "ORG"));
    checkTrue("no stray NOTE", !has(contacts[0], "NOTE"));
}

static void testLfOnlyLineEndings()
{
    test("files with bare LF line endings parse the same");
    const std::string vcf =
        "BEGIN:VCARD\nVERSION:3.0\nFN:Unix Export\nORG:Acme\n Corp\nEND:VCARD\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("FN", "Unix Export", field(contacts[0], "FN"));
    checkEqual("ORG", "AcmeCorp", field(contacts[0], "ORG"));
}

static void testValueContainingColons()
{
    test("only the first colon separates the property from its value");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Colon Test\r\n"
        "URL:https://example.com:8443/path\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("URL", "https://example.com:8443/path", field(contacts[0], "URL"));
}

static void testEmptyInput()
{
    test("empty and junk input produce no contacts");
    checkCount("empty", 0, vcard::parse("").size());
    checkCount("no colons", 0, vcard::parse("not a vcard at all\r\n").size());
    checkCount("begin and end only", 0, vcard::parse("BEGIN:VCARD\r\nEND:VCARD\r\n").size());
}

static void testFnWinsOverNForTheDisplayName()
{
    test("FN is the display name even when N comes first");
    const std::string vcf =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "N:Clarke;Arthur;C.;;\r\n"
        "FN:Sir Arthur C. Clarke\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("NAME", "Sir Arthur C. Clarke", field(contacts[0], "NAME"));
}

static void testCaseInsensitivePropertyNames()
{
    test("lower-case property names are recognised");
    const std::string vcf =
        "begin:vcard\r\nversion:3.0\r\nfn:Lower Case\r\nnote:a note\r\nend:vcard\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("FN", "Lower Case", field(contacts[0], "FN"));
    checkEqual("NOTE", "a note", field(contacts[0], "NOTE"));
}

// ---------------------------------------------------------------------------
// The three helpers parse() is built on, tested directly.

static void testUnfold()
{
    test("unfold joins continuations and drops blank lines");
    std::vector<std::string> lines = vcard::unfold(
        "FIRST:one\r\n"
        " continued with a space\r\n"
        "\r\n"
        "SECOND:two\r\n"
        "\tcontinued with a tab\r\n"
        "THIRD:three");

    checkCount("logical line count", 3, lines.size());
    if (lines.size() < 3) {
        return;
    }
    checkEqual("first", "FIRST:onecontinued with a space", lines[0]);
    checkEqual("second", "SECOND:twocontinued with a tab", lines[1]);
    checkEqual("third", "THIRD:three", lines[2]);
}

static void testUnfoldKeepsOnlyOneFoldCharacter()
{
    test("unfold removes exactly one leading whitespace character");
    std::vector<std::string> lines = vcard::unfold("NOTE:end of line\r\n   three spaces\r\n");
    checkCount("logical line count", 1, lines.size());
    if (lines.empty()) {
        return;
    }
    checkEqual("value", "NOTE:end of line  three spaces", lines[0]);
}

static void testUnfoldIgnoresLeadingContinuation()
{
    test("a continuation with nothing to continue is not a crash");
    std::vector<std::string> lines = vcard::unfold("  orphan continuation\r\nFN:Name\r\n");
    checkCount("logical line count", 1, lines.size());
    if (lines.empty()) {
        return;
    }
    checkEqual("value", "FN:Name", lines[0]);
}

static void testDecodeQuotedPrintable()
{
    test("quoted-printable decoding");
    checkEqual("upper hex", "\xC3\xA9", vcard::decodeQuotedPrintable("=C3=A9"));
    checkEqual("lower hex", "\xC3\xA9", vcard::decodeQuotedPrintable("=c3=a9"));
    checkEqual("plain text", "no escapes", vcard::decodeQuotedPrintable("no escapes"));
    checkEqual("mixed", "a\tb", vcard::decodeQuotedPrintable("a=09b"));
    // Malformed input stays readable rather than losing characters.
    checkEqual("not hex", "=ZZ", vcard::decodeQuotedPrintable("=ZZ"));
    checkEqual("truncated", "a=", vcard::decodeQuotedPrintable("a="));
    checkEqual("truncated pair", "=C", vcard::decodeQuotedPrintable("=C"));
}

static void testUnescapeText()
{
    test("backslash escape resolution");
    checkEqual("newline", "a\nb", vcard::unescapeText("a\\nb"));
    checkEqual("capital N newline", "a\nb", vcard::unescapeText("a\\Nb"));
    checkEqual("comma", "a,b", vcard::unescapeText("a\\,b"));
    checkEqual("semicolon", "a;b", vcard::unescapeText("a\\;b"));
    checkEqual("backslash", "a\\b", vcard::unescapeText("a\\\\b"));
    checkEqual("escaped backslash then n", "a\\nb", vcard::unescapeText("a\\\\nb"));
    checkEqual("trailing backslash", "a\\", vcard::unescapeText("a\\"));
    checkEqual("nothing to do", "plain", vcard::unescapeText("plain"));
}

// ---------------------------------------------------------------------------
// Parameter handling, which the details panel turns into type labels.

static void testVCard21BareParametersBecomeTypes()
{
    test("vCard 2.1 bare parameters are normalised to TYPE=");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Old Phone\r\n"
        "TEL;HOME;VOICE:555 0100\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("key", "PHONE_0_TEL;TYPE=HOME;TYPE=VOICE", keyByPrefix(contacts[0], "PHONE_0_"));
    checkEqual("value", "555 0100", fieldByPrefix(contacts[0], "PHONE_0_"));
}

static void testCommaSeparatedTypeList()
{
    test("a vCard 4.0 comma type list becomes separate TYPE= parameters");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:New Phone\r\n"
        "TEL;TYPE=\"cell,voice\";VALUE=uri:tel:+15550100\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("key", "PHONE_0_TEL;TYPE=CELL;TYPE=VOICE", keyByPrefix(contacts[0], "PHONE_0_"));
    checkEqual("value", "tel:+15550100", fieldByPrefix(contacts[0], "PHONE_0_"));
}

static void testEncodingParametersDoNotLeakIntoTheKey()
{
    test("ENCODING and CHARSET do not end up in the label key");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Encoded\r\n"
        "TEL;CHARSET=UTF-8;ENCODING=QUOTED-PRINTABLE;TYPE=CELL:+33=20=31\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("key", "PHONE_0_TEL;TYPE=CELL", keyByPrefix(contacts[0], "PHONE_0_"));
    checkEqual("value", "+33 1", fieldByPrefix(contacts[0], "PHONE_0_"));
}

// ---------------------------------------------------------------------------
// Structured values: the separators are structure, the escaped ones are text.

static void testStructuredSeparatorsSurviveUnescaping()
{
    test("ADR keeps its separators while its components are unescaped");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Address Test\r\n"
        "ADR;TYPE=WORK:;;Flat 2\\, 14 High Street;Bath;;BA1 1AA;UK\r\n"
        "END:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("ADR", ";;Flat 2, 14 High Street;Bath;;BA1 1AA;UK", field(contacts[0], "ADR"));
}

static void testCategoriesKeepTheirCommas()
{
    test("CATEGORIES keeps its comma separators");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Tagged\r\n"
        "CATEGORIES:Family,Friends,Clients\\, prospective\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("CATEGORIES", "Family,Friends,Clients, prospective",
               field(contacts[0], "CATEGORIES"));
}

static void testNameWithOnlyAFamilyName()
{
    test("N with only a family name still yields a display name");
    std::vector<vcard::Contact> contacts =
        vcard::parse("BEGIN:VCARD\r\nVERSION:3.0\r\nN:Cher;;;;\r\nEND:VCARD\r\n");
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("NAME", "Cher", field(contacts[0], "NAME"));
}

static void testEmptyNDoesNotProduceAnEmptyName()
{
    test("an empty N leaves NAME unset rather than blank");
    std::vector<vcard::Contact> contacts =
        vcard::parse("BEGIN:VCARD\r\nVERSION:3.0\r\nN:;;;;\r\nORG:Acme\r\nEND:VCARD\r\n");
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkTrue("NAME absent", !has(contacts[0], "NAME"));
}

// ---------------------------------------------------------------------------

static void testLargeEmbeddedPhotoIsReassembled()
{
    test("a photo folded across many lines is reassembled whole");
    std::string payload;
    std::string vcf = "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Big Photo\r\nPHOTO;ENCODING=b;TYPE=JPEG:";
    for (int i = 0; i < 200; ++i) {
        const std::string chunk = "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVph";
        payload += chunk;
        vcf += (i == 0 ? chunk : "\r\n " + chunk);
    }
    vcf += "\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("PHOTO_KIND", "embedded", field(contacts[0], "PHOTO_KIND"));
    checkEqual("PHOTO_TYPE", "image/jpeg", field(contacts[0], "PHOTO_TYPE"));
    checkCount("payload length", payload.size(), field(contacts[0], "PHOTO").size());
    checkTrue("payload matches", payload == field(contacts[0], "PHOTO"));
}

static void testBase64PhotoWithoutEncodingParameter()
{
    test("a bare BASE64 parameter is recognised as an encoding");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Old Export\r\n"
        "PHOTO;JPEG;BASE64:aVZCT1J3MEtHZ28=\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("PHOTO_KIND", "embedded", field(contacts[0], "PHOTO_KIND"));
    checkEqual("PHOTO", "aVZCT1J3MEtHZ28=", field(contacts[0], "PHOTO"));
}

static void testHttpsPhotoIsStillRemote()
{
    test("an https photo is remote too, and the scheme is preserved");
    const std::string vcf =
        "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Remote\r\n"
        "PHOTO;VALUE=URI:https://example.com/me.jpg\r\nEND:VCARD\r\n";

    std::vector<vcard::Contact> contacts = vcard::parse(vcf);
    checkCount("contact count", 1, contacts.size());
    if (contacts.empty()) {
        return;
    }
    checkEqual("PHOTO_KIND", "remote", field(contacts[0], "PHOTO_KIND"));
    checkEqual("PHOTO", "https://example.com/me.jpg", field(contacts[0], "PHOTO"));
}

static void testRepeatedEndDoesNotDuplicate()
{
    test("a stray second END:VCARD does not emit the contact twice");
    std::vector<vcard::Contact> contacts =
        vcard::parse("BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Once\r\nEND:VCARD\r\nEND:VCARD\r\n");
    checkCount("contact count", 1, contacts.size());
}

int main()
{
    testNoteIsNotTheName();
    testNicknameIsNotTheName();
    testFoldedLineIsJoined();
    testDuplicateLinesFoldIndependently();
    testFoldingIsNotQuadratic();
    testParametersDoNotDefeatPropertyMatching();
    testGroupedPropertiesAreUngrouped();
    testQuotedPrintableIsDecoded();
    testQuotedPrintableSoftLineBreak();
    testEscapesAreUnescaped();
    testStructuredNameKeepsItsSeparators();
    testEmbeddedPhotoIsRecognised();
    testDataUriPhotoIsRecognised();
    testRemotePhotoIsReportedAsRemote();
    testMultipleContacts();
    testRepeatedPropertiesAreNumberedInOrder();
    testCounterResetsBetweenContacts();
    testUnterminatedCardIsDropped();
    testPropertiesOutsideACardAreIgnored();
    testLfOnlyLineEndings();
    testValueContainingColons();
    testEmptyInput();
    testFnWinsOverNForTheDisplayName();
    testCaseInsensitivePropertyNames();

    testUnfold();
    testUnfoldKeepsOnlyOneFoldCharacter();
    testUnfoldIgnoresLeadingContinuation();
    testDecodeQuotedPrintable();
    testUnescapeText();
    testVCard21BareParametersBecomeTypes();
    testCommaSeparatedTypeList();
    testEncodingParametersDoNotLeakIntoTheKey();
    testStructuredSeparatorsSurviveUnescaping();
    testCategoriesKeepTheirCommas();
    testNameWithOnlyAFamilyName();
    testEmptyNDoesNotProduceAnEmptyName();
    testLargeEmbeddedPhotoIsReassembled();
    testBase64PhotoWithoutEncodingParameter();
    testHttpsPhotoIsStillRemote();
    testRepeatedEndDoesNotDuplicate();

    std::cout << "\n"
              << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
