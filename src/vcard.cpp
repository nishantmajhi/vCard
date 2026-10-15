#include "vcard.h"

#include <cctype>
#include <cstddef>

namespace vcard {
namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

std::string trim(const std::string &s)
{
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && isSpace(s[b])) {
        ++b;
    }
    while (e > b && isSpace(s[e - 1])) {
        --e;
    }
    return s.substr(b, e - b);
}

std::string rtrim(const std::string &s)
{
    std::size_t e = s.size();
    while (e > 0 && isSpace(s[e - 1])) {
        --e;
    }
    return s.substr(0, e);
}

std::string toUpper(std::string s)
{
    for (char &c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string toLower(std::string s)
{
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool startsWith(const std::string &s, const std::string &prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string stripWhitespace(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (!isSpace(c)) {
            out.push_back(c);
        }
    }
    return out;
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

// A property parameter. vCard 2.1 allows bare parameters (TEL;HOME;VOICE:),
// which arrive here with an empty name and the word in value.
struct Param {
    std::string name;  // upper case, empty for a bare 2.1 parameter
    std::string value; // upper case
};

struct Property {
    std::string name;   // upper case, group and parameters removed
    std::vector<Param> params;
    std::string value;  // still encoded and still escaped
};

// Splits on delim, ignoring delimiters inside double quotes. Parameter values
// may be quoted (TYPE="work,home"), and the quotes are not part of the value.
std::vector<std::string> splitRespectingQuotes(const std::string &s, char delim)
{
    std::vector<std::string> out;
    std::string current;
    bool inQuotes = false;
    for (char c : s) {
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (c == delim && !inQuotes) {
            out.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    out.push_back(current);
    return out;
}

// Splits on delim, ignoring delimiters preceded by a backslash. RFC 6350 uses
// ; and , as structure and \; and \, as literal text.
std::vector<std::string> splitUnescaped(const std::string &s, char delim)
{
    std::vector<std::string> out;
    std::string current;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            current.push_back(s[i]);
            current.push_back(s[i + 1]);
            ++i;
            continue;
        }
        if (s[i] == delim) {
            out.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(s[i]);
    }
    out.push_back(current);
    return out;
}

std::string join(const std::vector<std::string> &parts, char delim)
{
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out.push_back(delim);
        }
        out += parts[i];
    }
    return out;
}

// True when the property part of a logical line declares quoted-printable.
// Used during unfolding, before the line has been parsed into a Property.
bool declaresQuotedPrintable(const std::string &line)
{
    std::size_t colon = line.find(':');
    std::string head = toUpper(line.substr(0, colon == std::string::npos ? line.size() : colon));
    return head.find("QUOTED-PRINTABLE") != std::string::npos;
}

// A quoted-printable value continues on the next line when it ends with '='.
bool hasSoftLineBreak(const std::string &line)
{
    if (!declaresQuotedPrintable(line)) {
        return false;
    }
    const std::string t = rtrim(line);
    return !t.empty() && t.back() == '=';
}

// Splits "group.PROP;PARAM=X" into its parts. The group prefix is dropped:
// item1.EMAIL and EMAIL are the same property, and only Apple's exporter cares
// which group a line belongs to.
Property parseProperty(const std::string &logicalLine)
{
    Property prop;

    const std::size_t colon = logicalLine.find(':');
    const std::string head = logicalLine.substr(0, colon);
    prop.value = logicalLine.substr(colon + 1);

    std::vector<std::string> segments = splitRespectingQuotes(head, ';');

    std::string name = trim(segments.empty() ? std::string() : segments[0]);
    const std::size_t dot = name.find('.');
    if (dot != std::string::npos) {
        name = name.substr(dot + 1);
    }
    prop.name = toUpper(name);

    for (std::size_t i = 1; i < segments.size(); ++i) {
        const std::string segment = trim(segments[i]);
        if (segment.empty()) {
            continue;
        }
        const std::size_t eq = segment.find('=');
        Param param;
        if (eq == std::string::npos) {
            param.value = toUpper(segment);
        } else {
            param.name = toUpper(trim(segment.substr(0, eq)));
            param.value = toUpper(trim(segment.substr(eq + 1)));
        }
        prop.params.push_back(param);
    }

    return prop;
}

std::string paramValue(const Property &prop, const std::string &name)
{
    for (const Param &param : prop.params) {
        if (param.name == name) {
            return param.value;
        }
    }
    return std::string();
}

// The stored key for a repeated property carries its TYPE parameters, because
// the details panel derives its labels from them. Building the key from the
// parsed parameters rather than from the raw text means vCard 2.1 bare
// parameters (TEL;HOME) and 4.0 comma lists (TYPE=CELL,VOICE) produce the same
// shape as the 3.0 form the panel was written against.
std::string typeSuffix(const Property &prop)
{
    std::string suffix;
    for (const Param &param : prop.params) {
        if (param.name.empty()) {
            suffix += ";TYPE=" + param.value; // vCard 2.1 bare parameter
            continue;
        }
        if (param.name != "TYPE") {
            continue;
        }
        for (const std::string &one : splitRespectingQuotes(param.value, ',')) {
            const std::string t = trim(one);
            if (!t.empty()) {
                suffix += ";TYPE=" + t;
            }
        }
    }
    return suffix;
}

bool isBase64Encoded(const Property &prop)
{
    const std::string encoding = paramValue(prop, "ENCODING");
    if (encoding == "B" || encoding == "BASE64") {
        return true;
    }
    // vCard 2.1 writes the encoding as a bare parameter.
    for (const Param &param : prop.params) {
        if (param.name.empty() && (param.value == "BASE64" || param.value == "B")) {
            return true;
        }
    }
    return false;
}

bool isQuotedPrintableEncoded(const Property &prop)
{
    if (paramValue(prop, "ENCODING") == "QUOTED-PRINTABLE") {
        return true;
    }
    for (const Param &param : prop.params) {
        if (param.name.empty() && param.value == "QUOTED-PRINTABLE") {
            return true;
        }
    }
    return false;
}

// TYPE=JPEG is what a 3.0 file says; 4.0 says MEDIATYPE=image/jpeg.
std::string photoMediaType(const Property &prop)
{
    std::string type = paramValue(prop, "MEDIATYPE");
    if (type.empty()) {
        type = paramValue(prop, "TYPE");
    }
    if (type.empty()) {
        return std::string();
    }
    if (type.find('/') != std::string::npos) {
        return toLower(type);
    }
    return "image/" + toLower(type);
}

// Decides whether a PHOTO value travels with the file or has to be fetched.
// Anything that is not recognisably embedded data is treated as remote, and
// the caller decides whether to go near it.
void storePhoto(Contact &contact, const Property &prop, const std::string &rawValue)
{
    const std::string value = trim(rawValue);

    if (startsWith(toLower(value), "data:")) {
        // data:[<mediatype>][;base64],<payload>
        const std::size_t comma = value.find(',');
        const std::string meta = value.substr(5, comma == std::string::npos ? std::string::npos : comma - 5);
        const std::string payload = comma == std::string::npos ? std::string() : value.substr(comma + 1);

        std::string mediaType = splitRespectingQuotes(meta, ';')[0];
        if (mediaType.empty()) {
            mediaType = photoMediaType(prop);
        }

        contact["PHOTO_KIND"] = "embedded";
        contact["PHOTO"] = stripWhitespace(payload);
        if (!mediaType.empty()) {
            contact["PHOTO_TYPE"] = toLower(mediaType);
        }
        return;
    }

    if (isBase64Encoded(prop)) {
        contact["PHOTO_KIND"] = "embedded";
        contact["PHOTO"] = stripWhitespace(value);
        const std::string mediaType = photoMediaType(prop);
        if (!mediaType.empty()) {
            contact["PHOTO_TYPE"] = mediaType;
        }
        return;
    }

    contact["PHOTO_KIND"] = "remote";
    contact["PHOTO"] = value;
}

// Builds the name shown in the contact list from the components of N.
std::string displayNameFromN(const std::string &structuredValue)
{
    const std::vector<std::string> parts = splitUnescaped(structuredValue, ';');
    if (parts.size() < 2) {
        return trim(unescapeText(structuredValue));
    }
    const std::string family = trim(unescapeText(parts[0]));
    const std::string given = trim(unescapeText(parts[1]));
    if (given.empty()) {
        return family;
    }
    if (family.empty()) {
        return given;
    }
    return given + " " + family;
}

// Unescapes each component of a structured value without disturbing the
// separators between them.
std::string unescapeStructured(const std::string &value, char delim)
{
    std::vector<std::string> parts = splitUnescaped(value, delim);
    for (std::string &part : parts) {
        part = unescapeText(part);
    }
    return join(parts, delim);
}

} // namespace

std::vector<std::string> unfold(const std::string &content)
{
    std::vector<std::string> logical;
    bool softBreakPending = false;

    std::size_t start = 0;
    while (start <= content.size()) {
        std::size_t end = content.find('\n', start);
        const bool last = (end == std::string::npos);
        if (last) {
            end = content.size();
        }

        std::string line = content.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        start = end + 1;

        if (softBreakPending && !logical.empty()) {
            // The previous line ended mid-value with '='; drop it and continue.
            logical.back() = rtrim(logical.back());
            logical.back().pop_back();
            logical.back() += line;
            softBreakPending = hasSoftLineBreak(logical.back());
        } else if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
            // RFC 6350 folding: exactly one leading whitespace character is the
            // fold marker, and the rest of the line is value. A continuation
            // with nothing before it is malformed, and is dropped rather than
            // becoming a property line of its own.
            if (!logical.empty()) {
                logical.back() += line.substr(1);
                softBreakPending = hasSoftLineBreak(logical.back());
            }
        } else if (!trim(line).empty()) {
            logical.push_back(line);
            softBreakPending = hasSoftLineBreak(line);
        }

        if (last) {
            break;
        }
    }

    return logical;
}

std::string decodeQuotedPrintable(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '=') {
            out.push_back(text[i]);
            continue;
        }
        if (i + 2 < text.size()) {
            const int hi = hexDigit(text[i + 1]);
            const int lo = hexDigit(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        // A '=' that is not a valid escape is left alone rather than eaten, so
        // a malformed value is still readable.
        out.push_back('=');
    }
    return out;
}

std::string unescapeText(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            out.push_back(text[i]);
            continue;
        }
        const char next = text[i + 1];
        if (next == 'n' || next == 'N') {
            out.push_back('\n');
        } else {
            out.push_back(next);
        }
        ++i;
    }
    return out;
}

std::vector<Contact> parse(const std::string &content)
{
    std::vector<Contact> contacts;

    Contact current;
    bool inCard = false;
    bool haveFormattedName = false;
    int phoneCounter = 0;
    int emailCounter = 0;

    for (const std::string &line : unfold(content)) {
        const std::string upperLine = toUpper(line);

        if (startsWith(upperLine, "BEGIN:VCARD")) {
            current.clear();
            inCard = true;
            haveFormattedName = false;
            phoneCounter = 0;
            emailCounter = 0;
            continue;
        }

        if (startsWith(upperLine, "END:VCARD")) {
            if (inCard && !current.empty()) {
                contacts.push_back(current);
            }
            current.clear();
            inCard = false;
            continue;
        }

        if (!inCard || line.find(':') == std::string::npos) {
            continue;
        }

        const Property prop = parseProperty(line);
        if (prop.name.empty()) {
            continue;
        }

        // PHOTO is the one property whose value is not text: escapes and
        // whitespace inside base64 are meaningful, so it is handled before any
        // decoding runs.
        if (prop.name == "PHOTO" || prop.name == "LOGO") {
            std::string raw = prop.value;
            if (isQuotedPrintableEncoded(prop)) {
                raw = decodeQuotedPrintable(raw);
            }
            if (prop.name == "PHOTO") {
                storePhoto(current, prop, raw);
            }
            continue;
        }

        std::string value = prop.value;
        if (isQuotedPrintableEncoded(prop)) {
            value = decodeQuotedPrintable(value);
        }
        value = trim(value);

        if (prop.name == "FN") {
            const std::string text = unescapeText(value);
            current["FN"] = text;
            haveFormattedName = true;
            // FN is the display name per RFC 6350; if N was seen first, its
            // guess is replaced here so the list and the details panel agree.
            if (!text.empty()) {
                current["NAME"] = text;
            }
        } else if (prop.name == "N") {
            current["N"] = unescapeStructured(value, ';');
            if (!haveFormattedName) {
                const std::string display = displayNameFromN(value);
                if (!display.empty()) {
                    current["NAME"] = display;
                }
            }
        } else if (prop.name == "TEL") {
            current["PHONE_" + std::to_string(phoneCounter) + "_TEL" + typeSuffix(prop)] =
                unescapeText(value);
            ++phoneCounter;
        } else if (prop.name == "EMAIL") {
            current["EMAIL_" + std::to_string(emailCounter) + "_EMAIL" + typeSuffix(prop)] =
                unescapeText(value);
            ++emailCounter;
        } else if (prop.name == "ADR") {
            current["ADR"] = unescapeStructured(value, ';');
        } else if (prop.name == "ORG") {
            current["ORG"] = unescapeStructured(value, ';');
        } else if (prop.name == "CATEGORIES") {
            current["CATEGORIES"] = unescapeStructured(value, ',');
        } else if (prop.name == "VERSION") {
            current["VERSION"] = value;
        } else {
            // TITLE, URL, NOTE, BDAY and everything else the panel does not
            // have a section for but the search box can still match on.
            current[prop.name] = unescapeText(value);
        }
    }

    return contacts;
}

} // namespace vcard
