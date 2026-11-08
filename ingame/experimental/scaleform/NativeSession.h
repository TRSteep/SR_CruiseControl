#pragma once
#include "NativeUi.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace nativeui {
inline bool visibleOnStage(const Api& api,const Value& panel) {
    Owned stage(api);
    if(!get(api,panel,"stage",stage)||!object(stage.value))return false;
    Owned cursor(api);cursor.assignBorrowed(panel);
    for(unsigned depth=0;depth<32;++depth) {
        Owned flag(api),alpha(api),parent(api);double opacity=0;
        if(!get(api,cursor.value,"visible",flag)||type(flag.value)!=2||!flag.value.object||
           !get(api,cursor.value,"alpha",alpha)||!asNumber(alpha.value,opacity)||!(opacity>.01))return false;
        if(!get(api,cursor.value,"parent",parent))return false;
        if(!object(parent.value))return true;
        cursor.assignBorrowed(parent.value);
    }
    return false;
}
class Session {
    struct Row {
        Owned panel,row;
        int state=-1;
        bool ready=true;
        Row(const Api& api,const Value& parent,const Value& child):panel(api),row(api) {
            panel.assignBorrowed(parent);row.assignBorrowed(child);
        }
        Row(const Row&)=delete;
        Row& operator=(const Row&)=delete;
    };
    using Entry=std::shared_ptr<Row>;
    const Api& api;
    std::mutex mutex;
    std::unordered_map<void*,std::vector<Entry>> movies;
public:
    explicit Session(const Api& value):api(value){}
    bool has(void* movie){std::lock_guard<std::mutex> lock(mutex);return movies.find(movie)!=movies.end();}
    void add(void* movie,const Value& panel,const Value& row) {
        auto added=std::make_shared<Row>(api,panel,row);
        std::vector<Entry> discarded;
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto& rows=movies[movie];
            auto old=std::find_if(rows.begin(),rows.end(),[&](const Entry& r){return r->panel.value.object==panel.object;});
            if(old!=rows.end()){discarded.push_back(*old);rows.erase(old);}
            if(rows.size()>=16){discarded.push_back(rows.front());rows.erase(rows.begin());}
            rows.push_back(std::move(added));
        }
        // Discarded entries release after unlocking, in this live VM callback.
    }
    void drop(void* movie) {
        std::vector<Entry> discarded;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto found=movies.find(movie);if(found==movies.end())return;
            discarded=std::move(found->second);movies.erase(found);
        }
    }
    bool tick(void* movie,int state,const char* label) {
        std::vector<Entry> work;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto found=movies.find(movie);if(found==movies.end())return false;
            work=found->second;
        }
        // Snapshot shared ownership, never the address-registered GFx Values.
        // No registry lock is held while executing AS3 property accessors.
        bool visible=false;Api quiet=api;quiet.trace=nullptr;
        for(const auto& entry:work) {
            auto& row=*entry;const bool changed=row.state!=state;
            if(changed) {
                Owned text(api),tf(api);Value color;color.type=4;
                color.object=state==1?0xd5e07a:state==2?0xef744e:0xd2d5d1;
                row.ready=set(api,row.row.value,"selected",boolean(state==1))&&set(api,row.row.value,"label",string(label))&&
                    get(api,row.row.value,"tfCont",text)&&get(api,text.value,"tf",tf)&&
                    set(api,tf.value,"text",string(label))&&set(api,tf.value,"textColor",color);
                row.state=state;
            }
            visible=(row.ready&&visibleOnStage(changed?api:quiet,row.panel.value))||visible;
        }
        return visible;
    }
};
}
