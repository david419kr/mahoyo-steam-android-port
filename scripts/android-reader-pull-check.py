"""Run real four-direction gesture and rotation policy code without an Android device."""
from pathlib import Path
import os
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'android/app/src/main/java/org/mahoyo/port'
OUT=ROOT/'build/tests/android-reader-pull-check'
TEST='''package org.mahoyo.port;
public final class ReaderPullSelfTest {
static int checks;
static void check(boolean value,String name){checks++;if(!value)throw new AssertionError(name);}
static void near(float a,float b,String name){check(Math.abs(a-b)<.01f,name);}
static int move(ReaderGesture g,int action,float distance,float d){return g.move(1000*d+(action==3?distance:action==4?-distance:0),1000*d+(action==1?distance:action==2?-distance:0));}
public static void main(String[]args){
for(float d:new float[]{1,2,3})for(int action=1;action<=4;action++){
 ReaderGesture g=new ReaderGesture();float top=200*d,bottom=140*d,budget=action==1?top:action==2?bottom:0,peek=40*d,commit=100*d;
 g.start(1000*d,1000*d,d,top,bottom,true);
 check(move(g,action,budget+peek-1,d)==ReaderGesture.NONE,"consume scroll and peek threshold first");
 check(move(g,action,budget+peek,d)==ReaderGesture.PULL&&g.action()==action,"correct axis locks to the matching screen");
 near(g.reveal(),16*d,"initial peek is small");check(g.committed(),"opening intent commits a short deliberate swipe");
 move(g,action,budget+commit-1,d);check(g.committed(),"forward intent does not require a long swipe");move(g,action,budget+commit,d);check(g.committed(),"sufficient continued drag can commit");
 move(g,action,budget+peek,d);check(!g.committed(),"finger reversal withdraws commitment");near(g.reveal(),16*d,"preview follows finger reversal");
 float[] offset=ReaderGesture.offset(action,g.reveal(),1200*d,800*d);
 check(action==1?offset[1]<0:action==2?offset[1]>0:action==3?offset[0]<0:offset[0]>0,"entry edge matches swipe direction");
 float[] full=ReaderGesture.offset(action,Float.MAX_VALUE,1200*d,800*d);near(full[0],0,"fully open x");near(full[1],0,"fully open y");
 g.cancel();check(!g.pulling()&&!g.committed()&&!g.longPress(),"cancellation clears the gesture");
}
ReaderGesture g=new ReaderGesture();g.start(100,500,1,0,0,true);g.move(100,440);check(g.committed(),"short upward gesture opens settings");g.move(100,446);check(!g.committed(),"final reversal cancels even above initial threshold");g.move(100,440);check(g.committed(),"opening again restores intent");g.move(100,442);check(g.committed(),"two pixel jitter does not reverse intent");
for(float d:new float[]{1,2,3})for(int action=1;action<=4;action++){
 MenuCloseGesture c=new MenuCloseGesture();c.start(1000*d,1000*d,d,action,true);
 float sx=action==3?-1:action==4?1:0,sy=action==1?-1:action==2?1:0;
 check(!c.move((1000+sx*100)*d,(1000+sy*100)*d,false),"scrollable child owns movement before edge");
 check(!c.move((1000+sx*139)*d,(1000+sy*139)*d,true),"additional edge distance is required");
 check(c.move((1000+sx*160)*d,(1000+sy*160)*d,true)&&c.committed(),"reverse entry direction closes at edge");
 near(c.distance(),60*d,"only movement beyond scroll edge displaces menu");
 c.move((1000+sx*150)*d,(1000+sy*150)*d,true);check(!c.committed(),"reverse before release keeps menu open");
 c.move((1000+sx*156)*d,(1000+sy*156)*d,true);check(c.committed(),"closing again restores close intent");
 c.cancel();check(!c.claimed()&&!c.committed(),"cancel clears dismissal");
 c.start(0,0,d,action,false);check(!c.move(sx*200*d,sy*200*d,true),"preview or slider cannot dismiss");
}

g.start(0,0,1,0,0,true);check(g.move(300,300)==0,"diagonal is not navigation");g.start(0,0,1,0,0,false);check(g.move(300,0)==0&&!g.longPress(),"restricted scenes cannot navigate or hold skip");
g.start(0,0,1,0,0,true);check(g.longPress()&&!g.longPress()&&g.claimed(),"long press owns exactly one gesture");check(g.move(300,0)==0,"hold cannot also navigate");
g.start(0,0,1,0,0,true);g.move(26,0);check(!g.longPress(),"drag cancels pending hold");g.start(0,0,1,0,0,true);g.move(10,5);check(g.longPress(),"finger jitter permits hold");
ReaderHelpSession help=new ReaderHelpSession(true);check(!help.take(true)&&!help.take(false),"reset opt-out waits until next launch");help=new ReaderHelpSession(false);check(help.take(false)&&!help.take(false),"help is shown once per eligible launch");help=new ReaderHelpSession(false);check(!help.take(true),"current opt-out suppresses help");
SettingsRotation r=new SettingsRotation();check(r.sample(-1,0)==-1,"unknown orientation is ignored");check(r.sample(270,1)==-1,"entry landscape is inherited without an initial request");check(r.sample(270,1000)==-1,"steady entry orientation never causes a portrait flash");check(r.sample(0,1100)==-1&&r.sample(0,1279)==-1,"physical change must settle");check(r.sample(0,1280)==7,"stable portrait turn is allowed with system auto rotation off");check(r.sample(0,2000)==-1,"same portrait does not recreate the Activity again");check(r.sample(270,2100)==-1&&r.sample(270,2280)==6,"stable turn back restores landscape");
r=new SettingsRotation();r.sample(0,0);r.sample(90,10);r.sample(45,500);check(r.sample(90,600)==-1,"diagonal motion resets the stability timer");check(r.sample(90,780)==6,"Android chooses reverse rotation within the requested landscape axis");
r=new SettingsRotation(true);check(r.sample(0,0)==-1,"natural-landscape hardware still inherits entry rotation");check(r.sample(90,10)==-1&&r.sample(90,190)==7,"natural-landscape device can turn to portrait");check(r.sample(180,200)==-1&&r.sample(180,380)==6,"natural-landscape device can turn back to landscape");r=new SettingsRotation();check(r.sample(0,0)==-1,"an initial portrait report cannot rotate the inherited landscape frame");check(r.sample(270,10)==-1&&r.sample(270,190)==6,"fresh landscape reports never request an intermediate portrait axis");System.out.println("reader pulls and entry-safe rotation: "+checks+" checks passed");}}
'''


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    source=OUT/'ReaderPullSelfTest.java';source.write_text(TEST,encoding='utf-8')
    jdk=Path(os.environ.get('JAVA_HOME',''))/'bin'
    subprocess.run([str(jdk/'javac.exe'),'-encoding','UTF-8','-d',str(OUT),str(SOURCE/'ReaderGesture.java'),str(SOURCE/'DragIntent.java'),str(SOURCE/'MenuCloseGesture.java'),str(SOURCE/'SettingsRotation.java'),str(SOURCE/'ReaderHelpSession.java'),str(source)],check=True)
    result=subprocess.run([str(jdk/'java.exe'),'-cp',str(OUT),'org.mahoyo.port.ReaderPullSelfTest'],check=True,capture_output=True,text=True)
    (OUT/'result.txt').write_text(result.stdout,encoding='utf-8');print(result.stdout.strip())
    activity=(SOURCE/'MainActivity.java').read_text(encoding='utf-8')
    method=next(line.strip() for line in activity.splitlines() if line.strip().startswith('private void finishReaderPull('))
    audio_test=OUT/'PullAudioSelfTest.java'
    audio_test.write_text('''package org.mahoyo.port;
final class PullAudioSelfTest {
 static final class Audio {int pauses;void pause(){pauses++;}}
 boolean readerPullOpen=true,readerPullCompleting,readerPullCommit;int readerPullAction,corePauses,animations;Audio audio=new Audio();
 void updateCorePause(){corePauses++;}void animateReaderPull(){animations++;}
 '''+method+'''
 public static void main(String[] args){int checks=0;for(int action=1;action<=4;action++)for(boolean commit:new boolean[]{false,true}){
  PullAudioSelfTest state=new PullAudioSelfTest();state.readerPullAction=action;state.finishReaderPull(commit);
  if(state.audio.pauses!=(commit&&action==ReaderGesture.SETTINGS?1:0))throw new AssertionError("only confirmed settings pauses audio");checks++;
  if(state.corePauses!=(commit?1:0)||state.animations!=1)throw new AssertionError("menu commit still pauses gameplay, cancellation remains live");checks++;
  state.finishReaderPull(commit);if(state.animations!=1)throw new AssertionError("repeat release cannot pause or animate twice");checks++;
 }System.out.println("pull audio continuity: "+checks+" checks passed");}
}''',encoding='utf-8')
    subprocess.run([str(jdk/'javac.exe'),'-encoding','UTF-8','-cp',str(OUT),'-d',str(OUT),str(audio_test)],check=True)
    subprocess.run([str(jdk/'java.exe'),'-cp',str(OUT),'org.mahoyo.port.PullAudioSelfTest'],check=True)
    help_method=next(line.strip() for line in activity.splitlines() if line.strip().startswith('private void maybeShowReaderHelp('))
    help_test=OUT/'HelpFrameSelfTest.java'
    help_test.write_text('''package org.mahoyo.port;
final class HelpFrameSelfTest {
 static final class Json{String waiting="input";String optString(String key){return waiting;}}
 static final class Scene{Json json=new Json();}static final class Renderer{boolean ready;boolean ready(Scene s){return ready;}}
 static final class Content{int centerX(){return 100;}int centerY(){return 100;}}static final class Viewport{Content content=new Content();}
 static final class Values{boolean getBoolean(String key,boolean fallback){return false;}}static final class Prefs{Values values=new Values();}
 static final class ReaderHelpDialog{boolean showing;ReaderHelpDialog(Object owner){}void setOnDismissListener(java.util.function.Consumer<Object> c){}void show(){showing=true;}}
 Scene scene=new Scene(),committedScene;Renderer renderer=new Renderer();Viewport viewport=new Viewport();Prefs prefs=new Prefs();ReaderHelpSession readerHelpSession=new ReaderHelpSession(false);ReaderHelpDialog readerHelpDialog;boolean eligible=true;
 boolean readerGestureEligible(float x,float y){return eligible;}void cancelInputHold(){}
 '''+help_method+'''
 public static void main(String[] args){HelpFrameSelfTest s=new HelpFrameSelfTest();s.maybeShowReaderHelp();if(s.readerHelpDialog!=null)throw new AssertionError("load/continue snapshot cannot show help over stale title");s.committedScene=s.scene;s.maybeShowReaderHelp();if(s.readerHelpDialog!=null)throw new AssertionError("pending assets must finish first");s.renderer.ready=true;s.eligible=false;s.maybeShowReaderHelp();if(s.readerHelpDialog!=null)throw new AssertionError("slot/load overlay must close first");s.eligible=true;s.maybeShowReaderHelp();if(s.readerHelpDialog==null||!s.readerHelpDialog.showing)throw new AssertionError("first ready gameplay frame opens guide dialog");ReaderHelpDialog first=s.readerHelpDialog;s.maybeShowReaderHelp();if(first!=s.readerHelpDialog)throw new AssertionError("render callbacks never open a duplicate guide");System.out.println("help first-frame ordering: 5 checks passed");}
}''',encoding='utf-8')
    subprocess.run([str(jdk/'javac.exe'),'-encoding','UTF-8','-cp',str(OUT),'-d',str(OUT),str(help_test)],check=True)
    subprocess.run([str(jdk/'java.exe'),'-cp',str(OUT),'org.mahoyo.port.HelpFrameSelfTest'],check=True)




if __name__=='__main__':main()
