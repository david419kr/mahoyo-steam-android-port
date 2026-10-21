#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace mahoyo::text {
struct Resolution {
    std::string text;
    int lastId=-1;
};

// Replace every raw $dddddd reference, preserving all surrounding bytes.
// Exactly six ASCII digits are required; a seventh digit leaves the token
// untouched. Inserted CTD text is never rescanned. An out-of-range reference
// throws std::out_of_range, without returning partially resolved text.
Resolution resolveReferences(const std::string& input,const std::vector<std::string>& table);
// Native CTD bank loading replaces every caret before reference expansion.
std::string normalizeCtdLine(std::string input);
// Native @x is a zero-width control, including between appended text runs.
std::string stripDisplayControls(const std::string& input);
// Drop CTD continuation indents after Japanese punctuation, retaining paragraph indents.
std::string compactDialogueSpacing(const std::string& input,const std::string& preceding={});
// Count Unicode scalars in visible base text; ruby readings are annotations.
// Native inline italic/reset controls have no glyph or reveal interval.
size_t visibleLength(const std::string& input);
// Supported inline-font producer for the next native140 job. Ruby raw fields
// are not recursively compiled. Clearing this carry never edits prior glyphs.
bool endingItalic(const std::string& input,bool initialItalic=false);
class Reveal {
    size_t total_=0;
    double shown_=0;
    uint64_t last_=0;
    int speed_=40;
public:
    void reset(const std::string& text,uint64_t now,bool complete=false);
    void append(const std::string& text,uint64_t now);
    void remap(const std::string& text,uint64_t now);
    void tick(uint64_t now);
    void speed(int charactersPerSecond,uint64_t now);
    bool finish();
    bool active() const {return shown_<double(total_);}
    size_t visible() const;
};
} // namespace mahoyo::text
