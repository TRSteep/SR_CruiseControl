#pragma once
#include "NativeUi.h"
#include "CruiseIcon.h"
#include "HudLocalization.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace nativeui {
inline void syncNativeVisibility(const Api& api,const Value& panel,const Value& row) {
    Api quiet=api;quiet.trace=nullptr;
    Owned brake(quiet),flag(quiet),alpha(quiet);
    bool wantedVisible=false;double wantedAlpha=0;
    // The game can hide its named buttons without hiding their container.
    // Mirror the handbrake row, including fades, instead of tracking a key.
    if(get(quiet,panel,"brakeBtn",brake)&&get(quiet,brake.value,"visible",flag)&&type(flag.value)==2&&
       get(quiet,brake.value,"alpha",alpha)&&asNumber(alpha.value,wantedAlpha)&&
       std::isfinite(wantedAlpha)&&wantedAlpha>=0&&wantedAlpha<=1) {
        wantedVisible=flag.value.object!=0;
    }else wantedAlpha=0; // Hide until the native row is readable again.
    Owned actualFlag(quiet),actualAlpha(quiet);double opacity=0;
    bool changed=false,okay=true;
    if(!get(quiet,row,"visible",actualFlag)||type(actualFlag.value)!=2||
       (actualFlag.value.object!=0)!=wantedVisible) {
        changed=true;okay=set(quiet,row,"visible",boolean(wantedVisible))&&okay;
    }
    if(!get(quiet,row,"alpha",actualAlpha)||!asNumber(actualAlpha.value,opacity)||
       !std::isfinite(opacity)||std::abs(opacity-wantedAlpha)>0.00001) {
        changed=true;okay=set(quiet,row,"alpha",number(wantedAlpha))&&okay;
    }
    if(changed)trace(api,"hud.visibility.end",wantedVisible&&wantedAlpha>.01?"shown":"hidden",okay);
}

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
        hudlocale::Language language=hudlocale::Language::English;
        float languageElapsed=1;
        std::string caption;
        Row(const Api& api,const Value& parent,const Value& child):panel(api),row(api) {
            panel.assignBorrowed(parent);row.assignBorrowed(child);
        }
        Row(const Row&)=delete;
        Row& operator=(const Row&)=delete;
    };
    using Entry=std::shared_ptr<Row>;
    const Api& api;
    const hudlocale::Read read;
    std::mutex mutex;
    std::unordered_map<void*,std::vector<Entry>> movies;
public:
    explicit Session(const Api& value,hudlocale::Read reader=nullptr):api(value),read(reader){}
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
    bool tick(void* movie,int state,const char* label,float delta=0) {
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
            auto& row=*entry;
            if(delta>0&&delta<10)row.languageElapsed+=delta;
            if(read&&row.languageElapsed>=1) {
                row.languageElapsed=0;
                hudlocale::fromPanel(quiet,row.panel.value,read,row.language);
            }
            const char* caption=read?hudlocale::label(row.language):label;
            const bool stateChanged=row.state!=state;
            const bool changed=stateChanged||row.caption!=caption||!row.ready;
            if(changed) {
                Owned text(api),tf(api);Value color;color.type=4;
                color.object=0xd2d5d1;
                row.ready=set(api,row.row.value,"selected",boolean(state==1))&&set(api,row.row.value,"label",string(caption))&&
                    get(api,row.row.value,"tfCont",text)&&get(api,text.value,"tf",tf)&&
                    set(api,tf.value,"text",string(caption))&&set(api,tf.value,"textColor",color);
                if(row.ready) {
                    fitCaption(api,tf.value);
                    if(read&&row.caption!=caption)trace(api,"locale.changed",row.language==hudlocale::Language::Russian?"ru":row.language==hudlocale::Language::French?"fr":"en",true);
                    row.caption=caption;
                }
                row.state=state;
                if(stateChanged) {
                    const bool icon=drawCruiseIcon(api,row.row.value,state);
                    trace(api,"icon.draw.end",state==1?"on":state==2?"error":"off",icon);
                }
            }
            // The entire custom row owns the icon and F-key badge. Hiding
            // this parent hides all three without altering cruise state.
            syncNativeVisibility(api,row.panel.value,row.row.value);
            visible=(row.ready&&visibleOnStage(changed?api:quiet,row.panel.value))||visible;
        }
        return visible;
    }
};
}
