#include "../runtime/text.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
using mahoyo::text::resolveReferences;
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static void expect(const std::string& input,const std::vector<std::string>& table,const std::string& text,int lastId){const auto result=resolveReferences(input,table);check(result.text==text,"resolved text mismatch");check(result.lastId==lastId,"last reference ID mismatch");}
int main(int argc,char** argv){
    try{
        const std::vector<std::string> table={"日本語の本文","<ruby>漢字<rt>かんじ</rt></ruby>","second\nline","", "$999999 is literal CTD text", "@c1保持@x"};
        expect("",table,"",-1);
        expect("plain 日本語 @c1ルビ@x\n",table,"plain 日本語 @c1ルビ@x\n",-1);
        expect("$000000",table,table[0],0);
        expect("before $000001 after",table,"before "+table[1]+" after",1);
        expect("\n$000002",table,"\n"+table[2],2);
        expect("＆ダミー＆@i$000000@x\n@c1$000001@c0",table,"＆ダミー＆@i"+table[0]+"@x\n@c1"+table[1]+"@c0",1);
        expect("$000002$000000:$000002",table,table[2]+table[0]+":"+table[2],2);
        expect("x$000003y",table,"xy",3);
        expect("$000004",table,table[4],4); // Do not recursively resolve$999999.
        expect("$000005",table,table[5],5); // Native control tags are untouched.
        expect("$00000 $00000a $１２３４５６ $-00000 $0000000 $0000009",table,"$00000 $00000a $１２３４５６ $-00000 $0000000 $0000009",-1);
        expect("$000000a/$000001９",table,table[0]+"a/"+table[1]+"９",1); // ASCII-only seventh-digit rule.
        expect("$$000000",table,"$"+table[0],0);
        expect("$0000000$000001",table,"$0000000"+table[1],1);
        expect("$9999999",{},"$9999999",-1); // Non-matching tokens do not look up CTD.
        expect(std::string("a\0$000000\0b",11),table,std::string("a\0",2)+table[0]+std::string("\0b",2),0);
        for(const auto& input:{"$000006","prefix\n$999999@x"}){
            bool rejected=false;
            try{resolveReferences(input,table);}catch(const std::out_of_range& error){const auto message=std::string(error.what());rejected=message.find("CTD reference $")!=std::string::npos&&message.find("table size 6")!=std::string::npos;}
            check(rejected,"out-of-range CTD reference must report token and table size");
        }
        bool emptyRejected=false;try{resolveReferences("$000000",{});}catch(const std::out_of_range&){emptyRejected=true;}check(emptyRejected,"empty table rejects valid token");
        using mahoyo::text::visibleLength;using mahoyo::text::Reveal;
        using mahoyo::text::stripDisplayControls;
        check(stripDisplayControls("@x本文")=="本文","native @x prefix is not a glyph");
        check(stripDisplayControls("前文。　@x次の<文字|ルビ>😀")=="前文。　次の<文字|ルビ>😀","appended native controls preserve text/ruby/Unicode");
        check(stripDisplayControls("＆ダミー＆@x本文")=="本文","dummy and @x are both controls");
        check(stripDisplayControls("@@x @X @c1 unknown")=="@@x @X @c1 unknown","escaped and unimplemented syntax is preserved");
        check(visibleLength(stripDisplayControls("@x日本語"))==3,"control tokens do not consume reveal time");
        using mahoyo::text::compactDialogueSpacing;
        check(compactDialogueSpacing("　前文。　次、　　その次。\n　新段落")=="　前文。次、その次。\n　新段落","remove only punctuation continuation indents; preserve paragraph starts");
        check(compactDialogueSpacing("　続き。", "前文、")=="続き。","continuation whitespace across separate140 text producers");
        check(compactDialogueSpacing("　段落", "前文。\n")=="　段落","explicit newline retains fullwidth paragraph indent");
        check(compactDialogueSpacing("。@i　続き@r、　後")=="。@i続き@r、後","zero-width font controls do not protect stray continuation spaces");
        check(compactDialogueSpacing("<。　|。、　ルビ>、　次")=="<。　|。、　ルビ>、次","raw ruby fields remain byte exact");
        check(compactDialogueSpacing("　名前　本文。 半角、 維持")=="　名前　本文。 半角、 維持","intentional body and ASCII spaces remain");
        check(visibleLength(compactDialogueSpacing("前、　後。　次"))==5,"compacted spacing consumes no phantom reveal interval");
        check(visibleLength("a\n日本語😀<漢字|かんじ>")==7,"newlines consume no glyph delay; ruby/base and supplementary Unicode count");
        check(mahoyo::text::normalizeCtdLine("A^^B\\^")=="A\n\nB\\\n","CTD caret normalization has no escape");
        const std::vector<std::string> normalized={mahoyo::text::normalizeCtdLine("A^B")};
        expect("^$000000",normalized,"^A\nB",0); // Literal CHS caret is not CTD normalization.
        check(visibleLength("@oA@@oB\\@oC\nD")==6,"nowrap controls and LF have no glyph timing; repeated at/backslash remain literal");
        check(visibleLength("<@o漢字|@p-6/かんじ>\n@oX")==3,"ruby offsets never change base glyph timing");
        check(visibleLength("A@iB@i0\nC</i>D@rE")==6,"italic/reset controls and repeated on consume no glyph delay; digit is literal");
        check(visibleLength("@@iX\\@iY@IZ")==7,"raw italic has no double-at/backslash escape; uppercase stays literal");
        check(visibleLength("<I>A</I><i0>B</i0><fr>C")==3,"native internal italic names are case insensitive prefix matches");
        check(visibleLength("@i<AB|@p2/R><A@iB|@iR>")==6,"outer italic controls do not recursively consume raw controls inside ruby base");
        check(visibleLength("@i@a1AB@r@z@c1C@c5D@a2E")==5,"presentation controls have no glyph or reveal delay");
        check(visibleLength("<A@a1B|@c1R>")==5,"raw ruby fields are not recursively compiled");
        check(visibleLength("@a0@a3@cX@A1")==12,"unknown syntax remains literal");
        check(mahoyo::text::endingItalic("@i@a1@c5@z")&&!mahoyo::text::endingItalic("@a2@c1@z"),"presentation commands do not change italic carry");
        check(compactDialogueSpacing("。@a1@c5@z　X")=="。@a1@c5@zX","presentation controls preserve continuation spacing state");
        Reveal controls;controls.speed(10,0);controls.reset("@i@a1AB@r",0);controls.tick(100);check(controls.visible()==1&&controls.active(),"presentation commands add no hidden typewriter ticks");controls.tick(200);check(!controls.active(),"controlled body completes after visible glyphs only");
        Reveal italicDelay;italicDelay.speed(10,0);italicDelay.reset("@iA@iB</i>C",0);italicDelay.tick(200);check(italicDelay.visible()==2&&italicDelay.active(),"italic markup adds no typewriter delay");italicDelay.tick(300);check(!italicDelay.active(),"italic page finishes after its visible glyphs");
        using mahoyo::text::endingItalic;
        check(!endingItalic("")&&endingItalic("",true),"empty body preserves explicit input font carry");
        check(endingItalic("@iA\nB")&&endingItalic("@iA@iB"),"italic carry survives LF and repeated enable");
        check(!endingItalic("@iA@rB")&&endingItalic("@iA@rB@iC"),"full native font-record reset clears italic and a new @i enables again");
        check(!endingItalic("@iA</i>B")&&endingItalic("@iA</i>B@iC"),"explicit italic end resets and may be reenabled");
        check(endingItalic("@@iA\\@iB")&&!endingItalic("@I")&&endingItalic("@i0"),"native case, escape and numeric rules apply to carry");
        check(!endingItalic("<A@iB|@iR>")&&endingItalic("@i<A@rB|@rR>"),"raw ruby fields do not mutate font carry");
        check(!endingItalic("</I><fr>",true)&&endingItalic("<I0>")&&!endingItalic("<i|R>"),"internal tag prefix parsing and ruby compiler precedence");
        const auto first="@i<AB|reading>";const auto historical=std::string(first);bool carry=endingItalic(first);check(carry,"nonwait producer yields italic carry");carry=false;check(historical==first,"clearing persistent carry does not mutate prior body or ruby");
        check(!endingItalic("X",carry)&&endingItalic("X",true),"next140 begins at the explicit cleared or inherited font state");
        const std::string boundary="</i>@r";check(visibleLength(first+boundary+"X")==3&&!endingItalic(first+boundary+"X"),"next140 default boundary preserves old glyphs and does not add timing");
        check(endingItalic(first+boundary+"@iX"),"next140 inherited boundary restores italic after independent paragraph reset");
        Reveal zeroDelay;zeroDelay.speed(10,0);zeroDelay.reset("A\n@oB",0);zeroDelay.tick(100);check(zeroDelay.visible()==1&&zeroDelay.active(),"first glyph has one interval");zeroDelay.tick(200);check(zeroDelay.visible()==2&&!zeroDelay.active(),"newline and nowrap add no interval");
        check(visibleLength("<broken>text")==12,"non-ruby literal count");
        Reveal reveal;reveal.reset("日本語<漢字|かんじ>😀",100);
        check(reveal.active()&&reveal.visible()==0,"reveal starts empty");
        reveal.tick(125);check(reveal.visible()==1,"40 Unicode characters per second");
        reveal.tick(150);check(reveal.visible()==2,"reveals Unicode scalars");
        reveal.speed(10,150);reveal.tick(250);check(reveal.visible()==3,"speed change retains progress");
        reveal.append("日本語<漢字|かんじ>😀追加",250);check(reveal.visible()==3,"append does not restart shown prefix");
        check(reveal.finish()&&!reveal.active()&&reveal.visible()==8,"first advance completes text");
        check(!reveal.finish(),"second advance can proceed");
        reveal.reset("abc",300);reveal.speed(0,300);check(!reveal.active()&&reveal.visible()==3,"instant display");
        reveal.reset("次",400,true);check(!reveal.active(),"restored page is complete");
        reveal.speed(100,400);reveal.reset("abcdef",500);reveal.tick(510);reveal.tick(505);check(reveal.visible()==1,"backward clock never reveals negative characters");
        if(argc==3){
            std::ifstream ctd(argv[2],std::ios::binary);check(bool(ctd),"private CTD input opens");
            std::vector<std::string> rows;std::string row;while(std::getline(ctd,row)){if(!row.empty()&&row.back()=='\r')row.pop_back();rows.push_back(row);}
            const std::regex reference(R"(\$[0-9]{6}(?![0-9]))");size_t files=0,references=0;
            for(const auto& file:std::filesystem::directory_iterator(argv[1])){
                if(file.path().extension()!=".chs")continue;
                std::ifstream source(file.path(),std::ios::binary);std::string raw{std::istreambuf_iterator<char>(source),{}};
                references+=size_t(std::distance(std::sregex_iterator(raw.begin(),raw.end(),reference),std::sregex_iterator()));
                auto resolved=resolveReferences(raw,rows);check(!std::regex_search(resolved.text,reference),"private CHS leaves no raw CTD markers");++files;
            }
            check(files>0&&references>0,"private reference scan is nonempty");std::cout<<files<<" private CHS, "<<references<<" references resolved without residual IDs\n";
        }
        std::cout<<"Text reference UTF8, placement, boundaries, bounds and one-pass checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;}
}
