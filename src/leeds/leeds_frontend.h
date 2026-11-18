#pragma once
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Leeds FrontEnd/*.psp is XML with single-byte tag/attribute tokens, not
// executable PSP code. Keep the authored coordinates, navigation and actions.
struct StorylandFrontendAction { std::string name, parameter; };
struct StorylandFrontendElement {
    std::string type,name,hook,label,textureDictionary,texture,custom;
    float x=0,y=0,width=0,height=0,scaleX=.5f,scaleY=.7f,labelX=0,labelY=0;
    int font=1,horizontal=0,vertical=1,labelAlignment=0,minimum=0,maximum=0,step=1;
    uint32_t color=0xffffffff,highlightColor=0xffffffff;
    std::vector<std::string> states;
    std::map<std::string,std::vector<StorylandFrontendAction>> triggers;
};
struct StorylandFrontendPage {
    std::string name,initial;
    float width=480,height=272;
    std::vector<StorylandFrontendElement> elements;
};
class StorylandFrontend {
    struct Node {
        std::string name,text;
        std::map<std::string,std::string> attributes;
        std::vector<Node> children;
    };
    static std::string token(unsigned byte) { return std::string(1,char(byte)); }
    static void whitespace(const std::string& s,size_t& i) {
        while(i<s.size()&&(s[i]==' '||s[i]=='\t'||s[i]=='\r'||s[i]=='\n'))++i;
    }
    static std::string name(const std::string& s,size_t& i) {
        const auto start=i;
        while(i<s.size()&&s[i]!=' '&&s[i]!='\t'&&s[i]!='\r'&&s[i]!='\n'&&s[i]!='='&&s[i]!='/'&&s[i]!='>')++i;
        if(i==start)throw std::runtime_error("Missing frontend token");
        return s.substr(start,i-start);
    }
    static Node parse(const std::string& s,size_t& i,unsigned depth=0) {
        if(depth>16||i>=s.size()||s[i++]!='<')throw std::runtime_error("Invalid frontend structure");
        Node n;n.name=name(s,i);
        for(;;) {
            whitespace(s,i);
            if(i>=s.size())throw std::runtime_error("Truncated frontend tag");
            if(s[i]=='>'){++i;break;}
            if(s[i]=='/'&&i+1<s.size()&&s[i+1]=='>'){i+=2;return n;}
            const auto key=name(s,i);whitespace(s,i);
            if(i>=s.size()||s[i++]!='=')throw std::runtime_error("Missing frontend attribute assignment");
            whitespace(s,i);
            if(i>=s.size()||s[i++]!='\"')throw std::runtime_error("Missing frontend attribute quote");
            const auto end=s.find('"',i);
            if(end==std::string::npos)throw std::runtime_error("Truncated frontend attribute");
            n.attributes[key]=s.substr(i,end-i);i=end+1;
        }
        for(;;) {
            if(i>=s.size())throw std::runtime_error("Unclosed frontend tag");
            if(s[i]!='<'){n.text+=s[i++];continue;}
            if(i+1<s.size()&&s[i+1]=='/') {
                i+=2;const auto close=name(s,i);whitespace(s,i);
                if(close!=n.name||i>=s.size()||s[i++]!='>')throw std::runtime_error("Mismatched frontend tag");
                return n;
            }
            n.children.push_back(parse(s,i,depth+1));
        }
    }
    static const Node& child(const Node& n,unsigned tag) {
        for(const auto& c:n.children)if(c.name==token(tag))return c;
        static const Node empty;return empty;
    }
    static std::string attr(const Node& n,const std::string& key) {
        const auto p=n.attributes.find(key);return p==n.attributes.end()?std::string{}:p->second;
    }
    static std::string value(const Node& n,unsigned tag) {return attr(child(n,tag),token(0xad));}
    static float number(const Node& n,const std::string& key,float fallback=0) {
        const auto s=attr(n,key);if(s.empty())return fallback;
        size_t consumed=0;const auto result=std::stof(s,&consumed);
        if(consumed!=s.size()||!std::isfinite(result))throw std::runtime_error("Invalid frontend number");
        return result;
    }
    static uint32_t color(const Node& n) {
        uint32_t result=0;unsigned shift=0;
        for(const char* key:{"R","G","B","A"}) {
            const float v=number(n,key,255);
            if(v<0||v>255)throw std::runtime_error("Invalid frontend color");
            result|=uint32_t(v)<<shift;shift+=8;
        }
        return result;
    }
public:
    std::map<std::string,StorylandFrontendPage> pages;
    std::string activePage,highlighted,masterHighlight,error;
    bool masterFocus=true;
    std::map<std::string,int> values;
    static StorylandFrontendPage decode(const std::string& bytes) {
        if(bytes.size()>1024*1024)throw std::runtime_error("Frontend page exceeds size limit");
        size_t offset=0;whitespace(bytes,offset);const auto root=parse(bytes,offset);whitespace(bytes,offset);
        if(offset!=bytes.size()||root.name!=token(0x80))throw std::runtime_error("Invalid frontend page root");
        StorylandFrontendPage page;page.name=value(root,0x83);page.initial=value(root,0x84);
        page.width=number(child(root,0x82),token(0xb1),480);page.height=number(child(root,0x82),token(0xb2),272);
        if(page.name.empty()||page.width<=0||page.height<=0)throw std::runtime_error("Invalid frontend page dimensions/name");
        for(const auto& node:root.children)if(node.name==token(0x85)) {
            StorylandFrontendElement e;e.type=child(node,0x86).text;e.name=child(node,0x87).text;
            if(e.name.empty())throw std::runtime_error("Unnamed frontend element");
            e.x=number(child(node,0x89),"X");e.y=number(child(node,0x89),"Y");
            e.width=number(child(node,0x8b),token(0xb1));e.height=number(child(node,0x8b),token(0xb2));
            e.scaleX=number(child(node,0x96),"x",.5f);e.scaleY=number(child(node,0x96),"y",.7f);
            e.labelAlignment=int(number(child(node,0x91),token(0xad)));
            e.labelX=number(child(node,0x92),"X");e.labelY=number(child(node,0x92),"Y");
            e.font=int(number(child(node,0x97),token(0xad),1));
            e.horizontal=int(number(child(node,0x98),"Horizontal"));e.vertical=int(number(child(node,0x98),"Vertical",1));
            e.minimum=int(number(child(node,0x9a),token(0xad)));e.maximum=int(number(child(node,0x9b),token(0xad)));
            e.step=std::max(1,int(number(child(node,0x99),token(0xad),1)));
            e.color=color(child(node,0x94));e.highlightColor=color(child(node,0x95));
            e.hook=value(node,0x90);e.label=value(node,0x93);e.custom=value(node,0xaa);
            e.textureDictionary=child(node,0x9e).text;e.texture=child(node,0x9f).text;
            const int count=int(number(child(node,0x9c),token(0xad)));
            if(count<0||count>1024)throw std::runtime_error("Invalid frontend state count");
            for(int s=0;s<count;++s)e.states.push_back(attr(child(node,0x9d),"S"+std::to_string(s)));
            for(const auto& event:node.children)if(event.name==token(0x8e)) {
                auto& actions=e.triggers[attr(event,token(0xb4))];
                for(const auto& action:event.children)if(action.name==token(0x8f))
                    actions.push_back({attr(action,token(0xad)),attr(action,"P1")});
            }
            page.elements.push_back(std::move(e));
        }
        return page;
    }
    bool load(const std::filesystem::path& directory,bool widescreen=false) {
        try {
            std::map<std::string,StorylandFrontendPage> loaded;
            for(const auto& entry:std::filesystem::recursive_directory_iterator(directory)) {
                if(!entry.is_regular_file())continue;
                auto extension=entry.path().extension().string();
                std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
                if(extension!=".psp")continue;
                const auto stem=entry.path().stem().string();
                if(stem=="DspPg_US")continue; // Explicit regional alternative to DispPg.
                const bool wide=!stem.empty()&&stem.back()=='W';
                if(wide&&!widescreen)continue;
                if(!wide&&widescreen&&std::filesystem::exists(entry.path().parent_path()/(stem+"W.psp")))continue;
                std::ifstream input(entry.path(),std::ios::binary);if(!input)throw std::runtime_error("Cannot read frontend page");
                std::string bytes((std::istreambuf_iterator<char>(input)),{});
                auto page=decode(bytes);
                if(stem=="BrfBtns"||stem=="BrfBtnsW")page.name="BRIEF_BUTTONS";
                if(!loaded.emplace(page.name,std::move(page)).second)throw std::runtime_error("Duplicate frontend page name");
            }
            if(!loaded.count("MASTER")||!loaded.count("LOAD_PAGE"))throw std::runtime_error("Missing MASTER or LOAD_PAGE frontend definitions");
            pages=std::move(loaded);error.clear();reset();return true;
        }catch(const std::exception& e){error=e.what();return false;}
    }
    static std::filesystem::path childDirectory(const std::filesystem::path& parent,const std::string& name) {
        std::error_code error;
        const auto exact=parent/name;
        if(std::filesystem::is_directory(exact,error))return exact;
        error.clear();
        for(std::filesystem::directory_iterator entry(parent,error),end;!error&&entry!=end;entry.increment(error)) {
            const auto candidate=entry->path();
            auto filename=candidate.filename().string();
            std::transform(filename.begin(),filename.end(),filename.begin(),[](unsigned char character) {
                return char(std::tolower(character));
            });
            auto expected=name;
            std::transform(expected.begin(),expected.end(),expected.begin(),[](unsigned char character) {
                return char(std::tolower(character));
            });
            std::error_code typeError;
            if(filename==expected&&entry->is_directory(typeError))return candidate;
        }
        return {};
    }
    bool loadFromGamePaths(const std::vector<std::filesystem::path>& sources,bool widescreen=false) {
        std::vector<std::filesystem::path> roots;
        for(const auto& source:sources) {
            if(source.empty())continue;
            std::error_code typeError;
            auto resolved=std::filesystem::absolute(source,typeError);
            if(typeError){typeError.clear();resolved=source;}
            roots.push_back(std::filesystem::is_directory(resolved,typeError)?resolved:resolved.parent_path());
        }
        std::set<std::filesystem::path> visited;
        std::string lastFailure;
        for(unsigned level=0;level<6;++level) {
            for(auto& root:roots) {
                if(root.empty())continue;
                if(visited.insert(root.lexically_normal()).second) {
                    const auto frontend=childDirectory(root,"FRONTEND");
                    const auto rundata=childDirectory(root,"RUNDATA");
                    const auto runtimeFrontend=rundata.empty()?std::filesystem::path{}:childDirectory(rundata,"FRONTEND");
                    for(const auto& parent:{frontend,runtimeFrontend,root}) {
                        if(parent.empty())continue;
                        const auto defaults=childDirectory(parent,"DEFAULT");
                        if(defaults.empty())continue;
                        if(load(defaults,widescreen))return true;
                        lastFailure=defaults.string()+": "+error;
                    }
                }
                const auto parent=root.parent_path();
                root=parent==root?std::filesystem::path{}:parent;
            }
        }
        error=lastFailure.empty()?"FRONTEND/DEFAULT was not found beside the game's DTZ, world or MAIN.SCM files.":lastFailure;
        return false;
    }
    void reset() {masterFocus=true;activePage="LOAD_PAGE";highlighted=pages.count(activePage)?pages.at(activePage).initial:"";masterHighlight=pages.count("MASTER")?pages.at("MASTER").initial:"";}
    const StorylandFrontendPage* focusedPage()const {
        auto p=pages.find(masterFocus?"MASTER":activePage);return p==pages.end()?nullptr:&p->second;
    }
    using Handler=std::function<void(const StorylandFrontendAction&,const StorylandFrontendElement&)>;
    bool isNewGameConfirmation(const StorylandFrontendPage& page, std::string& affirmative) const {
        if (page.name == "MASTER" || page.name == "LOAD_PAGE") return false;
        bool negative = false;
        for (const auto& element : page.elements) {
            const auto actions = element.triggers.find("TRIGGER_OK_PRESSED");
            if (actions == element.triggers.end()) continue;
            const bool startsGame = std::any_of(actions->second.begin(), actions->second.end(), [](const auto& action) {
                return action.name == "ACTION_NEW_GAME";
            });
            if (startsGame) {
                if (!affirmative.empty()) return false;
                affirmative = element.name;
                continue;
            }
            bool returns = false;
            for (const auto& action : actions->second) {
                if (action.name == "ACTION_FOCUS_TO_MASTER_PAGE" || action.name == "ACTION_RETURN_TO_GAME") returns = true;
                if (action.name == "ACTION_SET_ACTIVE_PAGE") {
                    if (action.parameter == activePage || action.parameter == "MASTER" || action.parameter == "LOAD_PAGE") returns = true;
                    else return false;
                }
                if (action.name == "ACTION_LOAD_GAME" || action.name == "ACTION_SAVE_GAME") return false;
            }
            negative = negative || returns;
        }
        return !affirmative.empty() && negative;
    }
    bool trigger(const std::string& event,const Handler& host,unsigned depth=0,bool confirmNewGame=true) {
        if(depth>16){error="Frontend action cycle";return false;}
        const auto* page=focusedPage();if(!page)return false;
        const auto target=masterFocus?masterHighlight:highlighted;
        for(const auto& element:page->elements)if(element.name==target) {
            const auto found=element.triggers.find(event);if(found==element.triggers.end())return false;
            // Actions can change focus; their owner remains the original element.
            for(const auto& a:found->second) {
                if(a.name=="ACTION_HIGHLIGHT_ELEMENT_BY_NAME") {
                    if(std::none_of(page->elements.begin(),page->elements.end(),[&](const auto& e){return e.name==a.parameter;})){error="Unknown frontend element: "+a.parameter;continue;}
                    (masterFocus?masterHighlight:highlighted)=a.parameter;
                    trigger("TRIGGER_ON_HIGHLIGHT",host,depth+1,confirmNewGame);
                }else if(a.name=="ACTION_SET_ACTIVE_PAGE") {
                    auto p=pages.find(a.parameter);if(p==pages.end()){error="Missing frontend page: "+a.parameter;continue;}
                    if (!confirmNewGame) {
                        std::string affirmative;
                        if (isNewGameConfirmation(p->second, affirmative)) {
                            activePage = a.parameter;
                            highlighted = affirmative;
                            masterFocus = false;
                            return trigger("TRIGGER_OK_PRESSED", host, depth + 1, confirmNewGame);
                        }
                    }
                    activePage=a.parameter;highlighted=p->second.initial;
                }else if(a.name=="ACTION_FOCUS_TO_ACTIVE_PAGE") {masterFocus=false;trigger("TRIGGER_ON_HIGHLIGHT",host,depth+1,confirmNewGame);}
                else if(a.name=="ACTION_FOCUS_TO_MASTER_PAGE")masterFocus=true;
                else host(a,element);
            }
            return true;
        }
        return false;
    }
};
