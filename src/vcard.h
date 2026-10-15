#ifndef VCARD_H
#define VCARD_H

#include <map>
#include <string>
#include <vector>

// Pure C++ vCard parsing. Nothing in here knows about Qt, widgets or the
// filesystem: it turns the text of a .vcf file into a list of contacts, so it
// can be built and tested with a bare compiler.
namespace vcard {

// A contact is a flat map of display keys to values. The keys are the ones the
// details panel looks for: NAME, FN, N, ORG, TITLE, ADR, URL, NOTE, BDAY,
// CATEGORIES, VERSION, PHOTO, plus PHONE_<n>_<key> and EMAIL_<n>_<key> for the
// repeatable properties, where <key> carries the normalised TYPE parameters the
// panel turns into labels.
//
// A contact with a photo also carries:
//   PHOTO_KIND  "embedded" for data that came with the file, "remote" for a URI
//   PHOTO       the base64 payload when embedded, the URI when remote
//   PHOTO_TYPE  the media type ("image/jpeg"), when the file said what it was
using Contact = std::map<std::string, std::string>;

std::vector<Contact> parse(const std::string &content);

// Exposed for testing; parse() uses all three.

// Joins continuation lines onto the line they continue, per RFC 6350 section
// 3.2, and also resolves the quoted-printable soft line breaks that vCard 2.1
// exporters emit. Blank lines are dropped and CR is stripped.
std::vector<std::string> unfold(const std::string &content);

// Decodes =XX escapes. The result is raw bytes; callers assume UTF-8.
std::string decodeQuotedPrintable(const std::string &text);

// Resolves the backslash escapes of RFC 6350 section 3.4: \n and \N become a
// newline, and \\ \, \; become the character that follows the backslash.
std::string unescapeText(const std::string &text);

} // namespace vcard

#endif // VCARD_H
