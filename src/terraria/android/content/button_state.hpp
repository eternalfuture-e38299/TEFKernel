// Pure state for native touch bindings. No engine pointers or Lua callbacks.
#pragma once
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace tefkernel::buttons {
struct Action {
    std::string key, owner, id, name;
    bool enabled=true, visible=true, held=false;
    unsigned presses=0, releases=0;
    std::string icon_owner, icon;
    bool show_label=true;
};
using Slots=std::map<std::string,std::string>;
using Bindings=std::map<std::string,Slots>;
struct State {
    std::map<std::string,Action> actions;
    Bindings bindings, backup;
    std::vector<Bindings> undos, redos;
    Slots clipboard;
    bool has_backup=false, has_clipboard=false;
    static std::string key(const std::string& owner,const std::string& id){return owner+":"+id;}
    static bool valid_id(const std::string& id){
        if(id.empty()||id.size()>64)return false;
        for(unsigned char c:id)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
        return true;
    }
    Action* find(const std::string& k){auto i=actions.find(k);return i==actions.end()?nullptr:&i->second;}
    void reset_input(){for(auto& [k,a]:actions){a.held=false;a.presses=0;a.releases=0;}}
    void sync(const std::set<std::string>& held){
        for(auto& [k,a]:actions){bool next=a.enabled&&a.visible&&held.count(k);
            if(next&&!a.held)a.presses=std::min(64u,a.presses+1);
            if(!next&&a.held)a.releases=std::min(64u,a.releases+1);
            a.held=next;
        }
    }
    void remove_owner(const std::string& owner){
        for(auto it=actions.begin();it!=actions.end();)if(it->second.owner==owner)it=actions.erase(it);else ++it;
        // Saved bindings intentionally survive unload/reload, but cannot fire.
    }
    std::string binding(const std::string& profile,const std::string& slot)const{
        auto p=bindings.find(profile);if(p==bindings.end())return {};
        auto s=p->second.find(slot);return s==p->second.end()?std::string{}:s->second;
    }
    void erase(const std::string& profile,const std::string& slot){auto p=bindings.find(profile);if(p!=bindings.end()){p->second.erase(slot);if(p->second.empty())bindings.erase(p);}}
    // Native editor invokes snapshot only when it adds a matching undo point.
    void snapshot(){undos.push_back(bindings);redos.clear();}
    void undo(){if(!undos.empty()){redos.push_back(bindings);bindings=undos.back();undos.pop_back();reset_input();}}
    void redo(){if(!redos.empty()){undos.push_back(bindings);bindings=redos.back();redos.pop_back();reset_input();}}
    void begin_edit(){backup=bindings;has_backup=true;undos.clear();redos.clear();reset_input();}
    void cancel(){if(has_backup)bindings=backup;undos.clear();redos.clear();reset_input();}
    void clear_history(){undos.clear();redos.clear();}
};
}
