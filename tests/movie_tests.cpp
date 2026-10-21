#include "movie.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace mahoyo::movie;
static int checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static void noCommands(const Actions& a,const char* message){check(!a.playSE&&!a.fadeBgm&&!a.needSeek&&!a.completed&&!a.stopVideo,message);}
static Opening ready(){Opening m;m.start(1);m.prepared(1,197300);m.wait(1);return m;}
static void preparationAndNormalPlayback(){
    Opening m;check(!m.active()&&m.state().status==Status::Idle,"initial state is idle");
    auto a=m.start(1);check(a.accepted&&m.active()&&!m.state().prepared,"start requests preparation rather than completing playback");noCommands(a,"start emits no audio or EOF action");
    check(!m.position(1,1000).accepted,"preparation rejects premature positions");check(!m.eof(1).accepted,"preparation rejects premature EOF");
    a=m.prepared(1,197300);check(a.prepared&&!a.completed&&m.state().durationMs==197300,"1c0 releases on preparation and retains media duration");
    check(!m.prepared(1,197300).accepted,"prepared callback is consumed once");
    m.wait(1);a=m.position(1,187499);noCommands(a,"cue is not early");
    a=m.position(1,187500);check(a.playSE&&!a.needSeek&&!a.fadeBgm,"normal cue at187500ms");
    check(std::string(a.cue.path)=="se01004.hw"&&a.cue.slot==2&&a.cue.backend==1&&a.cue.loop,"cue resource and logical loop follow native slot2 descriptor");
    check(a.cue.volumeStart==0&&a.cue.volumeEnd==64&&a.cue.pan==64&&a.cue.fadeMs==15000,"native cue volume envelope is retained");
    noCommands(m.position(1,188000),"cue cannot repeat while awaiting command commit");
    a=m.committed(1);noCommands(a,"cue commit does not begin a normal BGM fade");
    noCommands(m.position(1,192999),"normal playback waits for193s boundary");
    noCommands(m.position(1,193000),"normal193s boundary has neither fade nor seek");
    a=m.eof(1);check(a.completed&&a.stopVideo&&a.stopVideoId==1&&!a.fadeBgm&&!a.playSE,"normal EOF releases1c1 and video exactly once");
    check(!m.active()&&!m.state().waiting&&m.state().status==Status::Finished,"normal EOF is terminal");
    check(!m.eof(1).accepted&&!m.position(1,197300).accepted,"late callbacks cannot replay EOF");
}
static void earlySkipAndSeekAcknowledgement(){
    auto m=ready();m.position(1,12000);auto a=m.skip(1);
    check(a.playSE&&!a.needSeek&&!a.fadeBgm,"early skip queues native cue before requesting seek");
    noCommands(m.skip(1),"repeat skip cannot duplicate pending cue");
    a=m.committed(1);check(a.needSeek&&a.seekToMs==193000&&a.seekSerial==1&&a.fadeBgm&&a.bgmFadeMs==3000,"after cue commit early skip seeks193s and fades BGM3000ms");
    check(m.state().seeking&&m.state().seekSerial==1,"seek is retained in snapshot state until acknowledged");
    noCommands(m.skip(1),"repeat skip cannot duplicate seek/fade");
    noCommands(m.committed(1),"fade commit waits for media EOF rather than a guessed duration");
    check(!m.position(1,197300,0).accepted&&!m.eof(1,0).accepted,"pre-seek callbacks cannot finish newer media position");
    check(!m.seeked(1,0,193000).accepted,"wrong seek acknowledgement is ignored");
    a=m.seeked(1,1,192980);check(a.accepted&&!a.completed&&!m.state().seeking,"seek acknowledges actual backend position, without inventing exact keyframe result");
    check(!m.seeked(1,1,193000).accepted,"duplicate seek completion is idempotent");
    check(!m.position(1,197300,0).accepted&&!m.eof(1,0).accepted,"old serial stays stale after seek acknowledgement");
    a=m.position(1,197000,1);noCommands(a,"post-seek playback does not replay cue or fade");
    a=m.eof(1,1);check(a.completed&&a.stopVideo&&!a.fadeBgm,"only acknowledged generation EOF completes skipped opening");
}
static void boundarySkip(){
    auto before=ready();before.position(1,187500);before.committed(1);before.position(1,192999);
    auto a=before.skip(1);check(a.needSeek&&a.fadeBgm&&!a.playSE,"skip one millisecond before193s takes seek/fade branch");
    auto at=ready();at.position(1,187500);at.committed(1);at.position(1,193000);
    a=at.skip(1);noCommands(a,"skip at193s does not add fade or seek");check(at.eof(1).completed,"late skip still waits for EOF");
}
static void waitAndPausedClock(){
    Opening m;m.start(1);m.skip(1);auto a=m.prepared(1,197300);check(a.prepared&&!a.playSE,"latched skip cannot run1c1 cue during1c0 preparation");
    noCommands(m.position(1,3000),"script's1300+1700ms phase is not replaced by movie EOF waiting");
    a=m.wait(1);check(a.playSE,"latched early skip is consumed only when1c1 starts");
    auto paused=ready();paused.position(1,187499);paused.setPaused(true);
    check(!paused.position(1,193000).accepted&&paused.state().positionMs==187499,"paused wall-time/position callback cannot advance cue state");
    noCommands(paused.skip(1),"skip while paused is latched");a=paused.setPaused(false);check(a.playSE,"resume evaluates latched skip without counting paused time");
    auto ended=ready();ended.position(1,194000);ended.committed(1);ended.setPaused(true);a=ended.eof(1);
    check(a.accepted&&!a.completed&&ended.active(),"EOF during pause is latched until resumed");
    a=ended.setPaused(false);check(a.completed&&a.stopVideo,"resume releases a latched EOF exactly once");
}
static void earlyEofAndQueueBoundaries(){
    Opening m;m.start(1);m.prepared(1,197300);auto a=m.eof(1);check(a.accepted&&!a.completed,"EOF before1c1 waits for the native completion job");
    a=m.wait(1);check(a.playSE&&!a.fadeBgm,"nonplaying native state still queues cue first");
    a=m.committed(1);check(a.fadeBgm&&!a.needSeek&&!a.completed,"native nonplaying branch queues BGM fade, without seeking ended media");
    a=m.committed(1);check(a.completed&&a.stopVideo,"early EOF completes only after queued commands are committed");
    check(!m.committed(1).accepted,"queue acknowledgement cannot replay completion");
    auto pending=ready();pending.position(1,187500);pending.eof(1);a=pending.committed(1);
    check(a.fadeBgm&&!a.completed,"EOF cannot bypass an outstanding cue commit");check(pending.committed(1).completed,"fade commit releases already ended media");
}
static void terminalPositionAcrossPause(){
    auto m=ready();m.position(1,187500);m.committed(1);m.position(1,192999);m.setPaused(true);
    auto a=m.eof(1,0,197300);
    check(a.accepted&&!a.completed&&!a.fadeBgm&&m.state().positionMs==197300,"terminal media position survives pause without scheduling commands");
    a=m.setPaused(false);check(a.completed&&!a.fadeBgm&&!a.playSE,"resume preserves normal EOF when last193s poll was suppressed");
    auto unpaused=ready();unpaused.position(1,187500);unpaused.committed(1);
    a=unpaused.eof(1,0,197300);check(a.completed&&!a.fadeBgm,"terminal position fills a missing final periodic poll");
    auto early=ready();early.position(1,187500);early.committed(1);early.setPaused(true);early.eof(1,0,192999);
    a=early.setPaused(false);check(a.fadeBgm&&!a.completed,"terminal position below193s retains native nonplaying fallback");
    auto stale=ready();stale.skip(1);stale.committed(1);stale.committed(1);stale.seeked(1,1,193000);
    check(!stale.eof(1,0,197300).accepted&&stale.state().positionMs==193000,"stale final position cannot alter a newer seek generation");
    auto canceled=ready();canceled.stop();check(!canceled.eof(1,0,197300).accepted,"terminal position after cancellation remains inert");
}
static void replacementCancellationAndErrors(){
    Opening m;m.start(1);auto a=m.start(2);check(a.canceled&&a.stopVideo&&a.stopVideoId==1&&a.id==2,"replacement explicitly identifies the old video to stop");
    check(!m.prepared(1,197300).accepted&&!m.position(1,197000).accepted&&!m.eof(1).accepted&&!m.skip(1).accepted&&!m.committed(1).accepted,"old generation callbacks and controls are ignored");
    m.prepared(2,197300);m.wait(2);a=m.fail(2);check(a.failed&&a.stopVideo&&!a.completed&&m.state().status==Status::Failed,"decoder error stops media without falsely releasing EOF wait");
    check(!m.eof(2).accepted&&!m.stop().accepted,"failed session cannot finish again");
    m.start(3);a=m.stop();check(a.canceled&&a.stopVideo&&!a.completed&&m.state().status==Status::Stopped,"cancel preparation stops video without consuming script continuation");
    check(!m.prepared(3,197300).accepted&&!m.stop().accepted,"late preparation after cancel is inert");
    bool invalid=false;try{m.start(3);}catch(const std::invalid_argument&){invalid=true;}check(invalid,"movie IDs cannot be reused for stale callbacks");
    invalid=false;try{m.start(0);}catch(const std::invalid_argument&){invalid=true;}check(invalid,"zero video generation is invalid");
    m.setPaused(true);m.start(4);check(m.state().paused,"new session preserves activity pause state");
    auto backward=ready();backward.position(1,2000);check(!backward.position(1,1999).accepted&&backward.state().positionMs==2000,"out-of-order positions cannot rewind media clock");
}
int main(){try{preparationAndNormalPlayback();earlySkipAndSeekAcknowledgement();boundarySkip();waitAndPausedClock();earlyEofAndQueueBoundaries();terminalPositionAcrossPause();replacementCancellationAndErrors();std::cout<<"Opening movie: "<<checks<<" native state/event checks passed\n";return 0;}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
