// Runtime members; independent frontends own no WH gameplay save state.
    void resetExtraFrontends(){galleryGroup=galleryVariant=-1;musicRow=musicPending=-1;musicPhase="idle";musicDue=0;}
    const mahoyo::extra::Gallery& galleryData(){if(!galleryCatalog)galleryCatalog=mahoyo::extra::Gallery::load(root/"meta/gallery-catalog.tsv");return *galleryCatalog;}
    const mahoyo::extra::Music& musicData(){if(!musicCatalog)musicCatalog=mahoyo::extra::Music::load(root/"meta/music-catalog.tsv");return *musicCatalog;}
    void returnFromArchive(){
        if(!archiveCheckpoint){title();return;}if(checkpointTargets.empty())checkpointTargets=mahoyo::extra::checkpoints(root/"meta/checkpoint-map.tsv");
        const int selector=archiveCheckpoint;auto target=checkpointTargets.at(selector);
        // Native12A80 requests the entry checkpoint: mode0/story0, not a picker.
        play(target,false);machine->setVariable(8,0,true);machine->setVariable(9,selector,true);machine->setVariable(459,selector);pump();
    }
    void openGallery(){galleryData();title();layers.clear();clearHistory();automatic=skipping=false;mode="gallery";wait="gallery";++frontendGeneration;}
    std::vector<const mahoyo::extra::Variant*> galleryVariants(int id)const{
        std::vector<const mahoyo::extra::Variant*> out;if(!galleryCatalog)return out;auto group=galleryCatalog->find(id);
        if(group)for(const auto& v:group->variants)if(mahoyo::extra::Gallery::unlocked(v,commonFlags(),seenImages))out.push_back(&v);return out;
    }
    void galleryAction(uint64_t generation,const std::string& action,int group,int variant){
        if(mode!="gallery"||generation!=frontendGeneration)return;
        if(action=="back"){if(galleryGroup>=0)galleryGroup=galleryVariant=-1;else title();return;}
        if(action=="select"){auto variants=galleryVariants(group);if(variants.empty())return;galleryGroup=galleryFocus=group;galleryVariant=variants.front()->index;return;}
        if(group!=galleryGroup||group<0)return;auto variants=galleryVariants(group);
        if(action=="variant"){for(const auto* v:variants)if(v->index==variant){galleryVariant=variant;return;}return;}
        if(action=="next"||action=="previous"){auto found=std::find_if(variants.begin(),variants.end(),[&](const auto* v){return v->index==galleryVariant;});if(found!=variants.end()){auto index=int(found-variants.begin());index=(index+(action=="next"?1:-1)+int(variants.size()))%int(variants.size());galleryVariant=variants[size_t(index)]->index;}return;}
        throw mahoyo::vm::Error("Unsupported gallery action");
    }
    std::string galleryJson()const{
        if(mode!="gallery"||!galleryCatalog)return "null";std::ostringstream out;out<<"{\"id\":"<<frontendGeneration<<",\"group\":"<<galleryGroup<<",\"variant\":"<<galleryVariant<<",\"focus\":"<<galleryFocus<<",\"groups\":[";bool first=true;
        for(const auto& g:galleryCatalog->groups){auto variants=galleryVariants(g.id);if(!first)out<<',';first=false;out<<"{\"id\":"<<g.id<<",\"count\":"<<variants.size();if(!variants.empty())out<<",\"preview\":"<<quote(variants.front()->name);out<<'}';}
        out<<"],\"variants\":[";first=true;for(auto* v:galleryVariants(galleryGroup)){if(!first)out<<',';first=false;out<<"{\"index\":"<<v->index<<",\"name\":"<<quote(v->name)<<'}';}return out.str()+"]}";
    }
    void openMusic(){musicData();title();layers.clear();clearHistory();automatic=skipping=false;mode="music";wait="music";++frontendGeneration;fadeSound("bgm",300,0,true);
        if(musicLast<0||!musicCatalog->find(musicLast)||!mahoyo::extra::Music::unlocked(*musicCatalog->find(musicLast),commonFlags()))musicLast=musicCatalog->adjacent(-1,1,commonFlags());
    }
    bool musicNeedsTick()const{return mode=="music"&&(musicPhase=="switching"||musicPhase=="stopping"||musicPhase=="closing");}
    void startMusicNow(int row){
        auto track=musicData().find(row);if(!track||!mahoyo::extra::Music::unlocked(*track,commonFlags()))return;
        uint64_t oldRemaining=0;for(const auto& fade:audioFades)if(fade.channel=="bgm"&&fade.stop&&fade.end>tick)oldRemaining=fade.end-tick;
        sound(track->name,"bgm",false);audio.back().volume=1;audio.back().pan=64;audio.back().oldFadeOutMs=int(std::min<uint64_t>(oldRemaining,60000));
        musicRow=musicLast=row;musicPending=-1;musicPhase="playing";musicDue=0;
    }
    void requestMusic(int row){
        if(musicPhase=="closing")return;auto track=musicData().find(row);if(!track||!mahoyo::extra::Music::unlocked(*track,commonFlags()))return;
        if(musicRow==row&&(musicPhase=="playing"||musicPhase=="switching"))return;
        if(musicRow>=0&&(musicPhase=="playing"||musicPhase=="switching")){fadeSound("bgm",2000,0,true);musicRow=musicLast=musicPending=row;musicPhase="switching";musicDue=tick+2000;}
        else startMusicNow(row);
    }
    void musicAction(uint64_t generation,const std::string& action,int row){
        if(mode!="music"||generation!=frontendGeneration)return;
        if(!error.empty()){if(action=="back")title();return;}
        if(musicPhase=="closing")return;
        if(action=="play"){requestMusic(row<0?musicLast:row);return;}
        if(action=="next"||action=="previous"){requestMusic(musicData().adjacent(musicRow>=0?musicRow:musicLast,action=="next"?1:-1,commonFlags()));return;}
        if(action=="repeat"){musicRepeat=!musicRepeat;return;}
        if(action=="stop"){if(musicRow>=0){fadeSound("bgm",1500,0,true);musicRow=musicPending=-1;musicPhase="stopping";musicDue=tick+1500;}return;}
        if(action=="back"){if(musicRow>=0||musicPhase=="stopping")fadeSound("bgm",700,0,true);musicRow=musicPending=-1;musicPhase="closing";musicDue=tick+700;return;}
        throw mahoyo::vm::Error("Unsupported music action");
    }
    void advanceMusic(){if(!musicNeedsTick()||tick<musicDue)return;if(musicPhase=="closing"){title();return;}if(musicPhase=="switching"){startMusicNow(musicPending);return;}musicPhase="idle";musicDue=0;}
    void musicEnded(){if(mode!="music"||musicPhase!="playing"||musicRow<0)return;const int next=musicRepeat?musicRow:musicData().adjacent(musicRow,1,commonFlags());if(next>=0)startMusicNow(next);else{musicRow=-1;musicPhase="idle";}}
    std::string musicJson()const{
        if(mode!="music"||!musicCatalog)return "null";std::ostringstream out;out<<"{\"id\":"<<frontendGeneration<<",\"phase\":"<<quote(musicPhase)<<",\"row\":"<<musicRow<<",\"selected\":"<<musicLast<<",\"repeat\":"<<(musicRepeat?"true":"false")<<",\"tracks\":[";bool first=true;
        for(const auto& t:musicCatalog->tracks){bool allowed=mahoyo::extra::Music::unlocked(t,commonFlags());if(!first)out<<',';first=false;out<<"{\"row\":"<<t.row<<",\"unlocked\":"<<(allowed?"true":"false");if(allowed)out<<",\"atlas\":"<<quote(resourceName(t.atlas))<<",\"crop\":["<<t.crop[0]<<','<<t.crop[1]<<','<<t.crop[2]<<','<<t.crop[3]<<']';out<<'}';}return out.str()+"]}";
    }
