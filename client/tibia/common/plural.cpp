#include "common/plural.h"

#include "common/pluralexception.h"

#include <string_view>
#include <utility>
#include <vector>

namespace tibia::common {

namespace {

bool contains(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

/// Replaces the first occurrence of `from`.
void replaceFirst(std::string& text, const char* from, const char* to)
{
    const std::string_view fromView(from);
    const size_t position = text.find(fromView);
    if (position != std::string::npos)
        text.replace(position, fromView.size(), to);
}

/// Replaces every occurrence of `from`, searching on after each replacement.
void replaceAll(std::string& text, const char* from, const char* to)
{
    const std::string_view fromView(from);
    const std::string_view toView(to);
    size_t position = text.find(fromView);
    while (position != std::string::npos) {
        text.replace(position, fromView.size(), toView);
        position = text.find(fromView, position + toView.size());
    }
}

/// Splits at spaces and tabs; a run of separators splits once, a separator at
/// either end leaves an empty token there.
std::vector<std::string_view> splitWords(const std::string_view text)
{
    const auto isSeparator = [](const char c) { return c == ' ' || c == '\t'; };
    std::vector<std::string_view> words;
    size_t start = 0;
    size_t index = 0;
    while (true) {
        if (index == text.size()) {
            words.push_back(text.substr(start));
            break;
        }
        if (isSeparator(text[index])) {
            words.push_back(text.substr(start, index - start));
            while (index < text.size() && isSeparator(text[index]))
                ++index;
            start = index;
            continue;
        }
        ++index;
    }
    return words;
}

bool startsWithArticle(const std::string_view text)
{
    return text.starts_with("a ") || (text.size() != 2 && text.starts_with("an "));
}

/// Pluralises the head noun in place.
void pluralizeHead(std::string& head)
{
    char last = ';';
    char secondLast = ';';
    if (head.size() >= 2) {
        secondLast = head[head.size() - 2];
        last = head[head.size() - 1];
        if (head.size() > 4 && contains(head, "thief")) {
            replaceAll(head, "thief", "thieves");
            return;
        }
        if (head.size() > 3 && contains(head, "hero")) {
            replaceFirst(head, "hero", "heroes");
            return;
        }
    }

    static constexpr std::pair<const char*, const char*> compoundIrregulars[] = {
        { "orc spearman", "orc spearmen" },
        { "dwarf henchman", "dwarf henchmen" },
        { "animated snowman", "animated snowmen" },
        { "boar man", "boar men" },
        { "crape man", "crape men" },
        { "raubritter marksman", "raubritter marksmen" },
        { "gingerbread man", "gingerbread men" },
    };
    for (const auto& [singular, pluralForm] : compoundIrregulars) {
        if (contains(head, singular)) {
            replaceFirst(head, singular, pluralForm);
            return;
        }
    }

    static constexpr const char* unchanging[] = {
        "deer", "dead deer", "white deer", "yielothax", "sheep", "black sheep", "fish",
        "efreet", "efreet", "marid", "kollos", "spidris", "jellyfish",
    };
    for (const char* name : unchanging) {
        if (contains(head, name))
            return;
    }

    if (head.size() < 2)
        return;

    if (contains(head, "cyclops")) {
        if (head.substr(head.size() - 3) == "toe")
            head += "s";
        else
            replaceFirst(head, "cyclops", "cyclopes");
        return;
    }

    static constexpr std::pair<const char*, const char*> irregulars[] = {
        { "medusa", "medusae" },
        { "crustacea gigantica", "crustaceae giganticae" },
        { "knife", "knives" },
        { "foot", "feet" },
        { "tooth", "teeth" },
        { "hoof", "hooves" },
        { "fungus", "fungi" },
    };
    for (const auto& [singular, pluralForm] : irregulars) {
        if (contains(head, singular)) {
            replaceFirst(head, singular, pluralForm);
            return;
        }
    }

    const auto isVowel = [](const char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; };
    switch (last) {
    case 'h':
        head += (secondLast == 'c' || secondLast == 's') ? "es" : "s";
        break;
    case 's':
    case 'x':
    case 'z':
        head += "es";
        break;
    case 'y':
        if (isVowel(secondLast))
            head += "s";
        else
            head = head.substr(0, head.size() - 1) + "ies";
        break;
    default:
        if (last == 'f' && secondLast == 'l')
            head = head.substr(0, head.size() - 1) + "ves";
        else if (last == 'o' && secondLast == 't')
            head += "es";
        else
            head += "s";
        break;
    }
}

}

std::string plural(const std::string& name, const int count)
{
    if (name.empty())
        throw TPluralException("Passed string is empty!");
    if (count == 1 || name.size() == 1)
        return name;

    std::string_view text(name);
    if (text.size() >= 3 && text.starts_with("a "))
        text.remove_prefix(2);
    else if (text.size() >= 3 && text.starts_with("an "))
        text.remove_prefix(3);

    if (count < 0)
        return std::string(text);

    if (text.size() > 5) {
        const size_t with = text.find(" with ");
        if (with != std::string_view::npos) {
            const std::string headPlural = plural("a " + std::string(text.substr(0, with)), 0);
            std::string tail(text.substr(with + 6));
            if (tail.size() > 1 && startsWithArticle(tail))
                tail = plural(tail, 0);
            if (count == 0)
                return headPlural + " with " + tail;
            return std::to_string(count) + " " + headPlural + " with " + tail;
        }
    }

    // The words before a lone "of" are the head; "of" and the rest follow it
    // unchanged.
    std::string head;
    std::string rest;
    bool inHead = true;
    bool firstWord = true;
    for (const std::string_view word : splitWords(text)) {
        if (word == "of")
            inHead = false;
        if (inHead) {
            if (!firstWord)
                head += ' ';
            head += word;
            firstWord = false;
        } else {
            rest += ' ';
            rest += word;
        }
    }

    pluralizeHead(head);

    if (count == 0)
        return head + rest;
    return std::to_string(count) + " " + head + rest;
}

}
