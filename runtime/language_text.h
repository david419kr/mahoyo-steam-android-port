#pragma once
#include "text.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mahoyo::language {
struct TextSource {
    std::string prefix,raw;
};
inline std::string displayText(const std::string& raw,const std::vector<std::string>& table,const std::string& previous={}) {
    auto text=mahoyo::text::compactDialogueSpacing(mahoyo::text::stripDisplayControls(mahoyo::text::resolveReferences(raw,table).text),previous);
    for(const char* suffix:{"@n","@e","@k"})if(text.size()>=2&&text.compare(text.size()-2,2,suffix)==0)text.resize(text.size()-2);
    return text;
}
inline std::string renderSources(const std::vector<TextSource>& sources,const std::vector<std::string>& table) {
    std::string text;for(const auto& source:sources){text+=source.prefix;text+=displayText(source.raw,table,text);}return text;
}
struct TextState {
    std::string edition,language;
    std::vector<TextSource> paragraph,history;
};
inline void writeTextState(std::ostream& out,const TextState& state) {
    out<<"LANGUAGETEXT1 "<<state.edition<<' '<<state.language<<' '<<state.paragraph.size()<<' '<<state.history.size()<<'\n';
    for(const auto* list:{&state.paragraph,&state.history})for(const auto& source:*list)out<<std::quoted(source.prefix)<<' '<<std::quoted(source.raw)<<'\n';
}
inline TextState readTextState(std::istream& in,size_t historyRows) {
    TextState state;std::string tag;size_t paragraphs,rows;
    if(!(in>>tag>>state.edition>>state.language>>paragraphs>>rows)||tag!="LANGUAGETEXT1"||paragraphs>4096||rows!=historyRows||rows>10000)throw std::runtime_error("Invalid saved language references");
    size_t bytes=0;
    for(int index=0;index<2;++index){
        const size_t count=index==0?paragraphs:rows;
        std::vector<TextSource> list;list.reserve(count);
        for(size_t i=0;i<count;++i){TextSource source;if(!(in>>std::quoted(source.prefix)>>std::quoted(source.raw))||source.prefix.size()>1024*1024||source.raw.size()>1024*1024||(bytes+=source.prefix.size()+source.raw.size())>16*1024*1024)throw std::runtime_error("Saved language reference budget");list.push_back(std::move(source));}
        if(index==0)state.paragraph=std::move(list);else state.history=std::move(list);
    }
    return state;
}
}
