#pragma once
#include "NativeUi.h"
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace nativeui {
inline void release(const Api& api,Value& value) {
    if (value.type & 0x40) api.release(iface(value),&value,data(value));
    value = {};
}
inline Value retain(const Api& api,const Value& source) {
    Value result=source;
    if (result.type & 0x40) api.retain(iface(result),&result,data(result));
    return result;
}
inline bool visibleOnStage(const Api& api,const Value& panel) {
    Owned stage(api);
    if (!get(api,panel,"stage",stage) || !object(stage.value)) return false;
    // Each ancestor is held only during this traversal. Check actual native
    // visibility so a map/menu overlay cannot grant activation by mere existence.
    Value cursor=retain(api,panel);
    bool visible=true, reachedRoot=false;
    for (unsigned depth=0;depth<32;++depth) {
        Owned flag(api),alpha(api),parent(api);
        double opacity=0;
        if (!get(api,cursor,"visible",flag) || type(flag.value)!=2 || !flag.value.object ||
            !get(api,cursor,"alpha",alpha) || !asNumber(alpha.value,opacity) || !(opacity>.01)) { visible=false;break; }
        if (!get(api,cursor,"parent",parent)) { visible=false;break; }
        if (!object(parent.value)) { reachedRoot=true;break; }
        release(api,cursor);cursor=parent.value;parent.value={};
    }
    release(api,cursor);
    return visible && reachedRoot;
}
class Session {
    struct Row { Value panel,row; int state=-1; bool ready=true; };
    const Api& api;
    std::recursive_mutex mutex;
    std::unordered_map<void*,std::vector<Row>> movies;
public:
    explicit Session(const Api& value):api(value){}
    bool has(void* movie) { std::lock_guard<std::recursive_mutex> lock(mutex);return movies.find(movie)!=movies.end(); }
    // No destructor calls into Scaleform: only the Movie destruction hook may
    // release these values, while its VM and ObjectInterface are still alive.
    void add(void* movie,const Value& panel,const Value& row) {
        Row added{retain(api,panel),retain(api,row),-1,true};
        std::vector<Row> discarded;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            auto& rows=movies[movie];
            auto old=std::find_if(rows.begin(),rows.end(),[&](const Row& r){return r.panel.object==panel.object;});
            if (old!=rows.end()) { discarded.push_back(*old);rows.erase(old); }
            if (rows.size()>=16) { discarded.push_back(rows.front());rows.erase(rows.begin()); }
            rows.push_back(added);
        }
        for(auto& entry:discarded){release(api,entry.row);release(api,entry.panel);}
    }
    void drop(void* movie) {
        std::vector<Row> discarded;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            const auto found=movies.find(movie);if(found==movies.end()) return;
            discarded=std::move(found->second);movies.erase(found);
        }
        for(auto& row:discarded){release(api,row.row);release(api,row.panel);}
    }
    bool tick(void* movie,int state,const char* label) {
        std::vector<Row> work;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            const auto found=movies.find(movie);if(found==movies.end()) return false;
            for(const auto& row:found->second)work.push_back({retain(api,row.panel),retain(api,row.row),row.state,row.ready});
        }
        // Never hold the registry lock while calling AS3 properties. Creation
        // callbacks may run on a loading worker that already owns the VM lock.
        bool visible=false;
        Api quiet=api;quiet.trace=nullptr;
        for(auto& row:work) {
            if(row.state!=state) {
                Owned text(api),tf(api);
                Value color;color.type=4;color.object=state==1 ? 0xd5e07a : state==2 ? 0xef744e : 0xd2d5d1;
                row.ready=set(api,row.row,"selected",boolean(state==1)) && set(api,row.row,"label",string(label)) &&
                    get(api,row.row,"tfCont",text) && get(api,text.value,"tf",tf) &&
                    set(api,tf.value,"text",string(label)) && set(api,tf.value,"textColor",color);
                row.state=state;
            }
            visible=(row.ready && visibleOnStage(quiet,row.panel)) || visible;
        }
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            const auto found=movies.find(movie);
            if(found!=movies.end())for(const auto& done:work)for(auto& row:found->second)
                if(row.row.object==done.row.object&&row.panel.object==done.panel.object){row.state=done.state;row.ready=done.ready;}
        }
        for(auto& row:work){release(api,row.row);release(api,row.panel);}
        return visible;
    }
};
}
