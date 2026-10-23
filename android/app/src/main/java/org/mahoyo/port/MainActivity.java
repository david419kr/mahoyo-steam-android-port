package org.mahoyo.port;

import android.app.*;
import android.content.*;
import android.content.res.Configuration;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.RippleDrawable;
import android.graphics.drawable.StateListDrawable;
import android.opengl.GLSurfaceView;
import android.os.*;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

public final class MainActivity extends Activity implements SceneRenderer.Assets,MoviePlayer.Assets {
    private FrameLayout root,controls;
    private ProgressBar inputHoldProgress;
    private final InputRegionSession inputSession=new InputRegionSession();
    private TeatimeControlsView teatime;
    private ArchiveView archive;
    private GalleryView gallery;private MusicView musicRoom;
    private ChoiceView choice;private volatile boolean choiceBlocked=true;
    private final ReaderGesture readerGesture=new ReaderGesture();
    private float holdX,holdY;private boolean heldSkip;
    private final Runnable startHeldSkip=()->{if(readerGestureEligible(holdX,holdY)&&readerGesture.longPress()){heldSkip=true;long now=SystemClock.uptimeMillis();MotionEvent cancel=MotionEvent.obtain(now,now,MotionEvent.ACTION_CANCEL,holdX,holdY,0);super.dispatchTouchEvent(cancel);cancel.recycle();command("skip","1");}};
    private Dialog slotDialog;private AlertDialog slotConfirmation;
    private SlotThumbnailStore slotThumbnails;
    private boolean slotUiOpen;private long slotSessionSerial;private Runnable slotCancel;
    private android.graphics.Bitmap pendingSlotThumbnail;
    private String savingModalId="";
    private GLSurfaceView surface;
    private int surfaceBufferWidth,surfaceBufferHeight;
    private SceneRenderer renderer;
    private TextOverlay text;
    private MoviePlayer moviePlayer;
    private Scene scene;
    private Scene committedScene;
    private String particleReadyAck="";
    private long staleRenderCommits;
    private Viewport viewport;
    private DisplayPrefs prefs;
    private AudioBus audio;
    private File gameRoot;
    private volatile GameResources gameData;
    private ProgressBar dataProgress;
    private final ExecutorService engine=Executors.newSingleThreadExecutor();
    private final Handler ui=new Handler(Looper.getMainLooper());
    private long handle;
    private boolean menu,menuInitialized,hidden,paused=true,destroyed;
    private int menuDefaultWidth,menuDefaultHeight;
    private float touchX,touchY;
    private final MenuGeometry menuGeometry=new MenuGeometry();
    private boolean languageSwitchPending;
    private String activityUiLanguage="";
    private String languageSnapshot;
    private long languageTickStarted;
    private final Map<String,MenuButton> menuButtons=new LinkedHashMap<>();
    private final TwoFingerTap twoFingerTap=new TwoFingerTap();
    private final TransitionHandshake transitionHandshake=new TransitionHandshake();
    private final MainHaltReturn haltReturn=new MainHaltReturn();
    private boolean settingsOpen;
    private volatile boolean readerPullOpen;
    private boolean readerPullCompleting;
    private FrameLayout readerPullLayer;
    private SettingsPanel readerPullPanel;
    private SwipeDialog readerPullDialog;private int readerPullAction;private float readerPullReveal;private long readerPullSerial;private volatile boolean readerPullCommit;private boolean readerPullOpeningSession;
    private ReaderHelpSession readerHelpSession;private ReaderHelpDialog readerHelpDialog;
    private String lastError="";
    private String lastStateKey="";
    private AlertDialog errorDialog;
    private volatile boolean errorModalOpen;
    private boolean historyLoading,historyOpen,historyClosing;
    private HistoryView historyDialog;private long historyUiSerial;private String historyRevision="";
    private boolean playbackPreferencesReady;
    private int appliedTextSpeed=Integer.MIN_VALUE,appliedAutoDelay=Integer.MIN_VALUE;
    private final Runnable tick=()->command("tick",Long.toString(SystemClock.uptimeMillis()));
    @Override protected void attachBaseContext(Context context){super.attachBaseContext(GameLanguage.localized(context));}
    @Override public void onCreate(Bundle state){super.onCreate(state);UiText.refresh(this);prefs=new DisplayPrefs(this);readerHelpSession=new ReaderHelpSession(prefs.values.getBoolean("hideReaderHelp",false));immersive();
        if(Build.VERSION.SDK_INT>=33)getOnBackInvokedDispatcher().registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,this::handleBack);
        try{Installation installation=new Installation(this);if(!installation.ready()||!installation.dataAvailable()){startActivity(new Intent(this,InstallActivity.class));finish();return;}gameRoot=installation.dataRoot(installation.read());}catch(Exception e){startActivity(new Intent(this,InstallActivity.class));finish();return;}gameRoot.mkdirs();
        slotThumbnails=new SlotThumbnailStore(new File(gameRoot.getParentFile(),"savedata"),new File(getFilesDir(),"slot-thumbnails"));
        root=new FrameLayout(this);root.setBackgroundColor(Color.BLACK);setContentView(root);
        surface=new GLSurfaceView(this);surface.setEGLContextClientVersion(3);surface.setPreserveEGLContextOnPause(true);moviePlayer=new MoviePlayer(this,surface,this::assetFile,this::movieEvent);renderer=new SceneRenderer(surface,this,s->ui.post(()->renderCommitted(s)),message->ui.post(()->{if(!destroyed)error(message);}),moviePlayer);surface.setRenderer(renderer);surface.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);root.addView(surface,new FrameLayout.LayoutParams(-1,-1));
        text=new TextOverlay(this);text.setPanelScale(prefs.dialogueScale());text.setOnPanelResize(()->layoutScene(),()->prefs.values.edit().putFloat("dialogueScale",text.panelScale()).apply());text.setOnTypographyChange(this::layoutScene,()->prefs.values.edit().putInt("fontPercent",text.fontPercent()).putInt("marginPercent",text.marginPercent()).apply());text.setOnAdvance(()->{if(teatimeActive()||frontendsActive()||choiceActive())return;if(hidden){hidden=false;layoutScene();}else command("advance","");});root.addView(text,new FrameLayout.LayoutParams(-1,-1));choice=new ChoiceView(this,this::assetFile,this::selectChoice,this::error);root.addView(choice,new FrameLayout.LayoutParams(-1,-1));controls=new FrameLayout(this);root.addView(controls,new FrameLayout.LayoutParams(-1,-1));teatime=new TeatimeControlsView(this,this::teatimeAction);root.addView(teatime,new FrameLayout.LayoutParams(-1,-1));
        archive=new ArchiveView(this,this::assetFile,this::command);root.addView(archive,new FrameLayout.LayoutParams(-1,-1));
        gallery=new GalleryView(this,this::assetFile,this::command,this::error);root.addView(gallery,new FrameLayout.LayoutParams(-1,-1));musicRoom=new MusicView(this,this::assetFile,this::command,this::error);root.addView(musicRoom,new FrameLayout.LayoutParams(-1,-1));
        root.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->{if(r-l!=or-ol||b-t!=ob-ot)layoutScene();});
        surface.setOnTouchListener((v,event)->touch(event));audio=new AudioBus(this,prefs,message->command("audioError",message),message->command("audioEnded",message));audio.setHistoryReplayListener((type,payload)->command(type,payload));
        if(!GameResources.available(this,gameRoot)){missingData();return;}
        dataProgress=new ProgressBar(this);FrameLayout.LayoutParams progress=new FrameLayout.LayoutParams(dp(48),dp(48),Gravity.CENTER);root.addView(dataProgress,progress);
        engine.execute(()->{try{gameData=new GameResources(this,gameRoot);DialogueArt.load(gameData);if(destroyed)return;handle=NativeBridge.createConfigured(gameRoot.getPath(),GameLanguage.edition(this),GameLanguage.effective(this));String snapshot=NativeBridge.snapshot(handle);ui.post(()->{if(!destroyed){dataProgress.setVisibility(View.GONE);apply(snapshot);}});}catch(Exception e){ui.post(()->{if(!destroyed){dataProgress.setVisibility(View.GONE);error(UiText.format(this,"game_data_error",e.getMessage()));}});}});
    }
    private GameAsset assetFile(String name){GameResources data=gameData;if(data==null)throw new IllegalStateException("Game data is not ready");return data.file(name);}
    @Override public GameAsset file(String name){return assetFile(name);}
    @Override public Collection<String> imageFiles(){GameResources data=gameData;return data==null?Collections.emptyList():data.imageFiles();}
    private void missingData(){LinearLayout box=new LinearLayout(this);box.setOrientation(1);box.setPadding(dp(30),dp(25),dp(30),dp(25));box.setGravity(17);TextView title=new TextView(this);title.setText(getString(R.string.setup_missing));title.setTextColor(Color.WHITE);title.setTextSize(17);box.addView(title);Button retry=new Button(this);retry.setText(UiText.text(this,"다시 확인"));retry.setOnClickListener(v->recreate());box.addView(retry);controls.addView(box,new FrameLayout.LayoutParams(-1,-1));}
    private void syncInputRegion(Scene value){
        JSONObject r=value.json.optJSONObject("inputRegion");
        inputSession.update(value.json.optLong("version"),r!=null&&r.optBoolean("active"),r!=null&&r.optBoolean("enabled"),r!=null&&r.optBoolean("readyPending"),
            r==null?"0":r.optString("generation","0"),r==null?"0":r.optString("request","0"),r==null?"0":r.optString("trigger","0"),r==null?"0":r.optString("epoch","0"),r==null?"0":r.optString("jumpRelease","0"));
        if(inputSession.needsClose()){if(historyOpen||historyLoading)closeHistory(true);if(slotUiOpen)dismissSlots();}
    }
    private void stopHeldSkip(){ui.removeCallbacks(startHeldSkip);if(heldSkip){heldSkip=false;command("skip","0");}}
    private void cancelInputHold(){stopHeldSkip();String payload=inputSession.cancel();if(!payload.isEmpty())command("inputCancel",payload);}
    private void layoutInputProgress(){
        if(inputHoldProgress==null){inputHoldProgress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);inputHoldProgress.setMax(192);inputHoldProgress.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);root.addView(inputHoldProgress);}
        JSONObject r=scene.json.optJSONObject("inputRegion");int elapsed=r==null?0:r.optInt("elapsedMs");
        inputHoldProgress.setVisibility(elapsed>0?View.VISIBLE:View.GONE);inputHoldProgress.setProgress(r==null?0:r.optInt("progressLeft",4)-4);
        int width=Math.max(dp(80),Math.round(200*viewport.scale)),height=Math.max(dp(6),Math.round(40*viewport.scale));
        FrameLayout.LayoutParams p=new FrameLayout.LayoutParams(width,height);p.leftMargin=Math.max(0,Math.round(viewport.content.right)-width-dp(12));p.topMargin=Math.max(0,Math.round(viewport.content.bottom)-height-dp(12));FrameLayout.LayoutParams old=(FrameLayout.LayoutParams)inputHoldProgress.getLayoutParams();if(old==null||old.width!=p.width||old.height!=p.height||old.leftMargin!=p.leftMargin||old.topMargin!=p.topMargin)inputHoldProgress.setLayoutParams(p);
    }
    private void immersive(){Window w=getWindow();WindowManager.LayoutParams p=w.getAttributes();android.view.Display display=getWindowManager().getDefaultDisplay();android.view.Display.Mode current=display.getMode();float refresh=0;for(android.view.Display.Mode mode:display.getSupportedModes())if(mode.getPhysicalWidth()==current.getPhysicalWidth()&&mode.getPhysicalHeight()==current.getPhysicalHeight()&&mode.getRefreshRate()>refresh){refresh=mode.getRefreshRate();p.preferredDisplayModeId=mode.getModeId();}p.layoutInDisplayCutoutMode=Build.VERSION.SDK_INT>=30?WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS:WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;w.setAttributes(p);w.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);w.getDecorView().setSystemUiVisibility(5894|View.SYSTEM_UI_FLAG_LAYOUT_STABLE|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);if(Build.VERSION.SDK_INT>=30){w.setDecorFitsSystemWindows(false);w.getInsetsController().hide(WindowInsets.Type.systemBars());w.getInsetsController().setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);}}
    private void command(String type,String payload){command(type,payload,null);}
    private void command(String type,String payload,Consumer<Scene> completion){command(type,payload,completion,null);}
    private void command(String type,String payload,Consumer<Scene> completion,MainHaltReturn.Ticket terminal){
        if(destroyed)return;engine.execute(()->{
            if(handle==0){if(completion!=null)ui.post(()->completion.accept(null));return;}
            if(type.equals("choiceSelect")&&choiceBlocked){if(completion!=null)ui.post(()->completion.accept(null));return;}
            if((errorModalOpen||readerPullBlocksGame())&&(type.equals("choiceSelect")||type.equals("tick")||type.equals("transitionReady")||type.equals("advance"))){if(completion!=null)ui.post(()->completion.accept(null));return;}
            try{if(terminal!=null){JSONObject fresh=new JSONObject(NativeBridge.snapshot(handle));if(!haltReturn.consume(terminal,normalMainHalt(fresh),fresh.optLong("version",-1),fresh.optString("module",""),fresh.optLong("instructions",-1),fresh.optLong("modulePc",-1)))return;}long started=SystemClock.uptimeMillis();NativeBridge.waveClock(handle,started);String result=NativeBridge.command(handle,type,payload);ui.post(()->{Scene next=apply(result,type.equals("tick")?started:0);if(!destroyed&&completion!=null)completion.accept(next);});}
            catch(Exception e){ui.post(()->{error(e.toString());if(!destroyed&&completion!=null)completion.accept(null);});}
        });
    }
    static final class TickCadence {
        // nextTickMs is relative to the snapshot time; parsing/layout must not add another full frame.
        static long delay(long requested,long started,long now){long interval=Math.max(16,Math.min(requested,5000));return Math.max(1,interval-(started>0?Math.max(0,now-started):0));}
    }
    private Scene apply(String source){return apply(source,0);}
    private Scene apply(String source,long tickStarted){
        if(destroyed)return null;
        try{Scene next=new Scene(source);if(scene!=null&&next.json.optLong("version")<scene.json.optLong("version"))return scene;
            String language=next.json.optString("language","");
            if(gameData!=null&&!language.isEmpty()&&!language.equals(gameData.language())){
                languageSnapshot=source;languageTickStarted=tickStarted;
                if(!languageSwitchPending){languageSwitchPending=true;surface.queueEvent(()->{
                    try{renderer.switchLanguage(language,()->gameData.setLanguage(language),()->ui.post(()->{languageSwitchPending=false;String latest=languageSnapshot;long started=languageTickStarted;languageSnapshot=null;if(!destroyed)apply(latest,started);}));}
                    catch(Exception error){ui.post(()->{languageSwitchPending=false;error(error.toString());});}
                });}
                return scene;
            }
            scene=next;haltReturn.observe(next);syncInputRegion(next);archive.state(next.json.optJSONObject("archive"));gallery.state(next.json.optJSONObject("gallery"));musicRoom.state(next.json.optJSONObject("musicRoom"));teatime.state(next.json.optJSONObject("modal"));choice.state(next.json.optJSONObject("choice"));if(readerPullOpen&&!readerPullCommit&&(!next.json.optString("scene").equals("play")||next.movie!=null||choiceActive()||teatimeActive()||inputSession.restricted()))discardReaderPull(false);if(!savingModalId.isEmpty()&&!teatime.saving(savingModalId)){savingModalId="";dismissSlots();}syncHistory(next);traceState(next);if(!playbackPreferencesReady&&handle!=0){playbackPreferencesReady=true;syncPlaybackPreferences(false);}if("exit".equals(next.json.optString("waiting"))){finish();return next;}layoutScene();audio.update(next.json.optJSONArray("audio"),this::assetFile);audio.updateHistoryReplay(historyOpen&&!paused&&!errorModalOpen&&next.json.optString("scene").equals("history")?next.json.optJSONObject("historyReplay"):null,this::assetFile);updateMovie(next.movie);openTeatimeSave();maybeShowReaderHelp();
            String error=next.json.optString("error","");if(!error.isEmpty())error(error);
            String notice=next.json.optString("notice","");if(!notice.isEmpty())Toast.makeText(this,UiText.text(this,notice),Toast.LENGTH_SHORT).show();
            ui.removeCallbacks(tick);long nextTick=next.json.optLong("nextTickMs",0);if(!paused&&!readerPullBlocksGame()&&!slotUiOpen&&!errorModalOpen&&!historyLoading&&!historyOpen&&!historyClosing&&error.isEmpty()&&nextTick>0)ui.postDelayed(tick,TickCadence.delay(nextTick,tickStarted,SystemClock.uptimeMillis()));return next;
        }catch(JSONException|IllegalArgumentException e){error(UiText.format(this,"scene_state_error",e.getMessage()));return null;}
    }
    private void traceState(Scene value){String key=value.json.optString("module")+":"+value.json.optString("waiting")+":"+value.json.optInt("textLine")+":"+(value.transition==null?"":value.transition.id)+":"+(value.movie==null?"":value.movie.optString("id"))+":"+value.json.optBoolean("skipping")+":"+paused;if(key.equals(lastStateKey))return;lastStateKey=key;android.util.Log.i("WoH.State",key+" instructions="+value.json.optLong("instructions")+" nextTick="+value.json.optLong("nextTickMs"));}
    private void renderCommitted(Scene value){
        if(destroyed)return;if(scene!=value){staleRenderCommits++;return;}committedScene=value;archive.presented(value.json.optJSONObject("archive")!=null);gallery.presented(value.json.optJSONObject("gallery")!=null);musicRoom.presented(value.json.optJSONObject("musicRoom")!=null);choice.presented(value.json.optJSONObject("choice"));text.update(value,viewport,hidden);updateMenuStates();
        String inputAck=inputSession.acknowledge(value.json.optLong("version"),scene==value,
            !paused&&!slotUiOpen&&!errorModalOpen&&!historyLoading&&!historyOpen&&!historyClosing&&!frontendsActive()&&!teatimeActive()&&!choiceActive(),
            moviePlayer==null||moviePlayer.releasedForInput(),root.getWidth(),root.getHeight());
        if(!inputAck.isEmpty()){int space=inputAck.indexOf(' ');command(inputAck.substring(0,space),inputAck.substring(space+1));}
        String particleAck=value.particles.readyAck(value.json.optLong("version",-1));
        if(!paused&&renderer.ready(value)&&!particleAck.isEmpty()&&!particleAck.equals(particleReadyAck)){particleReadyAck=particleAck;command("particlesReady",particleAck);}
        if(paused||readerPullBlocksGame()||slotUiOpen||errorModalOpen||historyLoading||historyOpen||historyClosing){transitionHandshake.signal("",false);return;}
        String id=value.transition==null?"":value.transition.id;
        String ack=value.json.optString("alphaRulePrepareAck","");if(ack.isEmpty()||ack.equals("null"))ack=id;
        if(transitionHandshake.signal(ack,"transitionReady".equals(value.json.optString("waiting"))))command("transitionReady",ack);
        maybeReturnToTitle();maybeShowReaderHelp();
    }
    private static boolean normalMainHalt(JSONObject value){
        if(!"play".equals(value.optString("scene"))||!"end".equals(value.optString("waiting"))||!value.optString("error","").isEmpty()||value.optString("module","").isEmpty())return false;
        for(String key:new String[]{"movie","transition","choice","modal","archive","gallery","musicRoom"})if(value.optJSONObject(key)!=null)return false;
        JSONObject region=value.optJSONObject("inputRegion");
        return region==null||(!region.optBoolean("active")&&!region.optBoolean("readyPending")&&"0".equals(region.optString("request","0"))&&"0".equals(region.optString("jumpRelease","0")));
    }
    private void maybeReturnToTitle(){
        Scene value=scene;
        boolean ready=!destroyed&&!paused&&!settingsOpen&&hasWindowFocus()&&!slotUiOpen&&!errorModalOpen&&!historyLoading&&!historyOpen&&!historyClosing
            &&value!=null&&committedScene==value&&normalMainHalt(value.json)&&!frontendsActive()&&!teatimeActive()&&!choiceActive()
            &&renderer!=null&&renderer.ready(value)&&moviePlayer!=null&&moviePlayer.releasedForInput();
        if(value==null)return;
        MainHaltReturn.Ticket ticket=haltReturn.claim(value,ready,value.json.optLong("version",-1),value.json.optString("module",""),value.json.optLong("instructions",-1),value.json.optLong("modulePc",-1));
        if(ticket!=null)command("title","",null,ticket);
    }
    private void openSettings(){settingsOpen=true;haltReturn.invalidate();startActivity(new Intent(this,SettingsActivity.class),android.app.ActivityOptions.makeCustomAnimation(this,R.anim.settings_enter_bottom,R.anim.settings_stay).toBundle());}
    private boolean readerPullBlocksGame(){return readerPullOpen&&readerPullCommit;}
    private void showReaderPull(){
        if(!readerPullOpen){
            readerPullOpen=true;readerPullCommit=false;readerPullOpeningSession=false;readerPullAction=readerGesture.action();long serial=++readerPullSerial;cancelInputHold();
            if(readerPullAction==ReaderGesture.SETTINGS){readerPullLayer=new FrameLayout(this);readerPullPanel=new SettingsPanel(this,0,()->finishReaderPull(false),()->{});readerPullPanel.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);readerPullLayer.addView(readerPullPanel,new FrameLayout.LayoutParams(-1,-1));root.addView(readerPullLayer,new FrameLayout.LayoutParams(-1,-1));}
            else if(readerPullAction==ReaderGesture.HISTORY)requestHistoryPullPreview(serial);
            else{SlotGridDialog dialog=new SlotGridDialog(this,slotThumbnails,readerPullAction==ReaderGesture.SAVE,scene!=null&&scene.json.optBoolean("canSave"),(saving,entry)->{});prepareReaderPullDialog(dialog,serial);dialog.show();readerPullDialogShown(serial);}
        }
        readerPullReveal=readerGesture.reveal();positionReaderPull();
    }
    private void requestHistoryPullPreview(long serial){
        engine.execute(()->{if(handle==0)return;try{String value=NativeBridge.command(handle,"historyPreview","");ui.post(()->{
            if(!readerPullOpen||serial!=readerPullSerial||destroyed)return;
            try{HistoryView dialog=createHistoryView(HistoryModel.parse(new JSONObject(value)));prepareReaderPullDialog(dialog,serial);dialog.show();readerPullDialogShown(serial);}catch(Exception failure){discardReaderPull(false);error(failure.toString());}
        });}catch(Exception failure){ui.post(()->{if(readerPullOpen&&serial==readerPullSerial){discardReaderPull(false);error(failure.toString());}});}});
    }
    private HistoryView createHistoryView(HistoryModel model){final HistoryView[] view={null};view[0]=new HistoryView(this,model,group->replayHistory(view[0],group),this::stopHistoryReplay);view[0].contentArea(viewport==null?null:viewport.content,root.getWidth(),root.getHeight());return view[0];}
    private View readerPullView(){return readerPullDialog!=null?readerPullDialog.pullView():readerPullPanel;}
    private void positionReaderPull(){View view=readerPullView();if(view==null)return;float[] offset=ReaderGesture.offset(readerPullAction,readerPullReveal,root.getWidth(),root.getHeight());view.setTranslationX(offset[0]);view.setTranslationY(offset[1]);if(readerPullLayer!=null)readerPullLayer.setBackgroundColor(Color.argb(Math.round(80*Math.min(1,readerPullReveal/Math.max(1,root.getHeight()))),0,0,0));}
    private void prepareReaderPullDialog(SwipeDialog dialog,long serial){
        if(!readerPullOpen||serial!=readerPullSerial)return;if(readerPullDialog==dialog&&dialog.isShowing())return;readerPullDialog=dialog;float[] offset=ReaderGesture.offset(readerPullAction,readerPullReveal,root.getWidth(),root.getHeight());dialog.preparePull(offset[0],offset[1]);
    }
    private void readerPullDialogShown(long serial){
        if(!readerPullOpen||serial!=readerPullSerial)return;
        if(readerPullOpeningSession){SwipeDialog dialog=readerPullDialog;clearReaderPullState();layoutScene();dialog.commitPull();}
        else{positionReaderPull();if(readerPullCompleting)animateReaderPull();}
    }
    private void finishReaderPull(boolean commit){if(!readerPullOpen||readerPullCompleting)return;readerPullCompleting=true;readerPullCommit=commit;if(commit){updateCorePause();if(readerPullAction==ReaderGesture.SETTINGS&&audio!=null)audio.pause();}animateReaderPull();}
    private void animateReaderPull(){
        View view=readerPullView();if(view==null){if(!readerPullCommit)discardReaderPull(false);return;}
        float[] end=ReaderGesture.offset(readerPullAction,readerPullCommit?Float.MAX_VALUE:0,root.getWidth(),root.getHeight());
        view.animate().translationX(end[0]).translationY(end[1]).setDuration(readerPullCommit?220:160).setInterpolator(new android.view.animation.DecelerateInterpolator()).withEndAction(()->{
            if(readerPullCommit&&!destroyed&&!paused&&readerPullOpen){
                if(readerPullAction==ReaderGesture.SETTINGS){settingsOpen=true;long serial=readerPullSerial;Intent next=new Intent(this,SettingsActivity.class).putExtra("previewReady",new android.os.ResultReceiver(ui){@Override protected void onReceiveResult(int code,Bundle data){settingsPreviewReady(serial);}});startActivity(next,android.app.ActivityOptions.makeCustomAnimation(this,0,0).toBundle());overridePendingTransition(0,0);}
                else{readerPullOpeningSession=true;if(readerPullAction==ReaderGesture.HISTORY)history(true,readerPullSerial);else slots(readerPullAction==ReaderGesture.SAVE?"save":"load",()->{},()->true,true,readerPullSerial);}
            }else discardReaderPull(true);
        }).start();
    }
    private void settingsPreviewReady(long serial){if(settingsOpen&&readerPullOpen&&readerPullAction==ReaderGesture.SETTINGS&&serial==readerPullSerial)discardReaderPull(false);}
    private void clearReaderPullState(){
        View view=readerPullView();if(view!=null)view.animate().cancel();readerPullOpen=false;readerPullCompleting=false;readerPullCommit=false;readerPullOpeningSession=false;readerPullSerial++;if(readerPullLayer!=null)root.removeView(readerPullLayer);readerPullLayer=null;readerPullPanel=null;readerPullDialog=null;readerGesture.cancel();
    }
    private void discardReaderPull(boolean resume){
        if(!readerPullOpen)return;int action=readerPullAction;boolean committed=readerPullCommit,opening=readerPullOpeningSession;SwipeDialog preview=readerPullDialog;clearReaderPullState();
        if(opening){if(action==ReaderGesture.HISTORY)closeHistory(true);else if(action==ReaderGesture.SAVE||action==ReaderGesture.LOAD)dismissSlots();}
        if(preview!=null&&preview.isShowing())preview.dismiss();
        if(committed&&resume&&!paused&&!destroyed&&!settingsOpen){updateCorePause();if(action==ReaderGesture.SETTINGS&&audio!=null)audio.resume();}
    }
    private void maybeShowReaderHelp(){if(scene==null||committedScene!=scene||renderer==null||!renderer.ready(scene)||!scene.json.optString("waiting").equals("input")||!readerGestureEligible(viewport==null?0:viewport.content.centerX(),viewport==null?0:viewport.content.centerY())||!readerHelpSession.take(prefs.values.getBoolean("hideReaderHelp",false)))return;cancelInputHold();readerHelpDialog=new ReaderHelpDialog(this);readerHelpDialog.setOnDismissListener(dialog->readerHelpDialog=null);readerHelpDialog.show();}

    static final class TransitionHandshake {
        private String sent="";
        boolean signal(String id,boolean waiting){if(id.isEmpty()){sent="";return false;}if(!waiting||id.equals(sent))return false;sent=id;return true;}
    }
    private void syncPlaybackPreferences(boolean force){
        if(!playbackPreferencesReady||prefs==null||destroyed)return;
        int speed=prefs.textSpeed(),delay=prefs.autoDelay();
        if(force||speed!=appliedTextSpeed){appliedTextSpeed=speed;command("textSpeed",Integer.toString(speed));}
        if(force||delay!=appliedAutoDelay){appliedAutoDelay=delay;command("autoDelay",Integer.toString(delay));}
    }
    private void error(String message){
        ui.removeCallbacks(tick);if(destroyed||isFinishing()||message==null||message.isEmpty()||message.equals(lastError))return;haltReturn.invalidate();lastError=message;android.util.Log.e("WoH.Runtime",message);
        if(errorDialog!=null&&errorDialog.isShowing()){errorDialog.setMessage(message);return;}
        errorModalOpen=true;stopHistoryReplay();transitionHandshake.signal("",false);AlertDialog shown=new AlertDialog.Builder(this).setTitle(UiText.text(this,"호환 런타임")).setMessage(message).setPositiveButton(UiText.text(this,"확인"),null).create();errorDialog=shown;shown.setOnDismissListener(dialog->{if(errorDialog==shown){errorDialog=null;errorModalOpen=false;updateCorePause();}});shown.show();updateCorePause();
    }
    private void updateCorePause(){if(destroyed)return;haltReturn.invalidate();updateChoiceInput();ui.removeCallbacks(tick);command(paused||readerPullBlocksGame()||slotUiOpen||errorModalOpen||historyLoading||historyOpen||historyClosing?"pause":"resume",Long.toString(SystemClock.uptimeMillis()));}
    @Override public boolean dispatchTouchEvent(MotionEvent e){
        if(e.getActionMasked()==MotionEvent.ACTION_UP)stopHeldSkip();
        if(readerPullOpen){int action=e.getActionMasked();if(readerPullCompleting)return true;
            if(action==MotionEvent.ACTION_MOVE){readerGesture.move(e.getX(),e.getY());readerPullReveal=readerGesture.reveal();positionReaderPull();}
            else if(action==MotionEvent.ACTION_UP){readerGesture.move(e.getX(),e.getY());readerPullReveal=readerGesture.reveal();positionReaderPull();boolean commit=readerGesture.committed();readerGesture.cancel();finishReaderPull(commit);}
            else if(action==MotionEvent.ACTION_CANCEL||action==MotionEvent.ACTION_POINTER_DOWN){twoFingerTap.cancel();readerGesture.cancel();finishReaderPull(false);}return true;}
        if(e.getActionMasked()==MotionEvent.ACTION_POINTER_DOWN||e.getActionMasked()==MotionEvent.ACTION_CANCEL)cancelInputHold();
        if(slotUiOpen||errorModalOpen||historyLoading||historyOpen||historyClosing)return true; // Dialog has its own Window; never advance the underlying game during a slot transaction.
        if(teatimeActive()||frontendsActive()){twoFingerTap.cancel();return super.dispatchTouchEvent(e);}
        if(text!=null&&text.resizeHandle(e)){cancelInputHold();readerGesture.cancel();twoFingerTap.cancel();return true;}
        if(text!=null&&text.pinch(e)){cancelInputHold();readerGesture.cancel();twoFingerTap.cancel();return true;}
        int action=e.getActionMasked(),index=e.getActionIndex();boolean alreadyMultiple=twoFingerTap.multiple();
        if(action==MotionEvent.ACTION_DOWN)twoFingerTap.start(e.getPointerId(index),e.getX(index),e.getY(index),e.getEventTime(),dp(25));
        if(action==MotionEvent.ACTION_POINTER_DOWN)twoFingerTap.pointerDown(e.getPointerId(index),e.getX(index),e.getY(index));
        for(int i=0;i<e.getPointerCount();i++)twoFingerTap.move(e.getPointerId(i),e.getX(i),e.getY(i));
        if(!alreadyMultiple&&twoFingerTap.multiple()){
            MotionEvent cancel=MotionEvent.obtain(e);cancel.setAction(MotionEvent.ACTION_CANCEL);super.dispatchTouchEvent(cancel);cancel.recycle();
        }
        if(action==MotionEvent.ACTION_POINTER_UP)twoFingerTap.pointerUp(e.getPointerId(index));
        if(action==MotionEvent.ACTION_CANCEL){boolean consume=twoFingerTap.multiple();twoFingerTap.cancel();readerGesture.cancel();return consume||super.dispatchTouchEvent(e);}
        if(action==MotionEvent.ACTION_UP){boolean consume=twoFingerTap.multiple();boolean toggle=twoFingerTap.finish(e.getPointerId(index),e.getEventTime());if(consume){if(toggle)toggleMenu();return true;}}
        if(action==MotionEvent.ACTION_DOWN){holdX=e.getX();holdY=e.getY();boolean eligible=readerGestureEligible(holdX,holdY);readerGesture.start(holdX,holdY,getResources().getDisplayMetrics().density,text.backlogScrollBudget(holdX,holdY),text.forwardScrollBudget(holdX,holdY),eligible);ui.removeCallbacks(startHeldSkip);if(eligible)ui.postDelayed(startHeldSkip,1000);}
        if(action==MotionEvent.ACTION_POINTER_DOWN)readerGesture.cancel();
        if(twoFingerTap.multiple()&&action!=MotionEvent.ACTION_DOWN)return true;
        int gesture=action==MotionEvent.ACTION_MOVE?readerGesture.move(e.getX(),e.getY()):ReaderGesture.NONE;
        if(gesture==ReaderGesture.PULL){if(!readerPullOpen){MotionEvent cancel=MotionEvent.obtain(e);cancel.setAction(MotionEvent.ACTION_CANCEL);super.dispatchTouchEvent(cancel);cancel.recycle();}showReaderPull();return true;}
        if(readerGesture.claimed()){if(action==MotionEvent.ACTION_UP)readerGesture.cancel();return true;}
        return super.dispatchTouchEvent(e);
    }
    private boolean readerGestureEligible(float x,float y){return !paused&&!destroyed&&!settingsOpen&&!readerPullOpen&&hasWindowFocus()&&!slotUiOpen&&!errorModalOpen&&!historyLoading&&!historyOpen&&!historyClosing&&scene!=null&&scene.json.optString("scene").equals("play")&&scene.movie==null&&scene.transition==null&&!hidden&&!inputSession.restricted()&&!choiceActive()&&viewport!=null&&viewport.content.contains(x,y);}
    private boolean touch(MotionEvent e){
        if(inputSession.captures()){
            int action=e.getActionMasked();
            if(action==MotionEvent.ACTION_CANCEL||e.getPointerCount()!=1){cancelInputHold();return true;}
            if(action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_UP){
                boolean inside=viewport!=null&&viewport.content.contains(e.getX(),e.getY());
                String payload=inputSession.pointer(action==MotionEvent.ACTION_DOWN,inside);if(!payload.isEmpty())command("inputPointer",payload);
            }
            return true;
        }
        if(teatimeActive()||frontendsActive()||choiceActive())return true;
        int action=e.getActionMasked();if(action==MotionEvent.ACTION_DOWN){touchX=e.getX();touchY=e.getY();}
        if(action==MotionEvent.ACTION_UP){if(Math.hypot(e.getX()-touchX,e.getY()-touchY)>dp(25))return true;if(hidden){hidden=false;layoutScene();return true;}if(scene==null||viewport==null)return true;float x=(e.getX()-viewport.x)/viewport.scale,y=(e.getY()-viewport.y)/viewport.scale;
            for(JSONObject a:scene.actions){float l=(float)a.optDouble("x"),t=(float)a.optDouble("y"),w=(float)a.optDouble("w"),h=(float)a.optDouble("h");if(x>=l&&x<=l+w&&y>=t&&y<=t+h){String id=a.optString("id");if(id.equals("settings")||id.equals("config"))openSettings();else if(id.equals("load"))slots("load");else if(id.equals("extra"))extra();else command("action",id);return true;}}command("advance","");}
        return true;
    }
    private void toggleMenu(){if(inputSession.restricted())return;if(teatimeActive()||frontendsActive())return;menu=!menu;layoutScene();}
    private void extra(){
        String[] labels={"書庫","図録","楽曲","戻る"};for(int i=0;i<labels.length;++i)labels[i]=UiText.text(this,labels[i]);
        new AlertDialog.Builder(this).setTitle(UiText.text(this,"閲覧")).setItems(labels,(dialog,index)->{if(index<3)command(new String[]{"archiveOpen","galleryOpen","musicOpen"}[index],"");}).show();
    }
    private boolean frontendsActive(){return archive!=null&&archive.active()||gallery!=null&&gallery.active()||musicRoom!=null&&musicRoom.active();}
    private boolean choiceActive(){return choice!=null&&choice.active();}
    private void updateChoiceInput(){choiceBlocked=destroyed||paused||readerPullBlocksGame()||slotUiOpen||errorModalOpen||historyLoading||historyOpen||historyClosing||scene==null||!scene.json.optString("error").isEmpty();if(choice!=null)choice.interactive(!choiceBlocked);}
    private void selectChoice(ChoiceSession.Request request){
        if(destroyed||paused||slotUiOpen||errorModalOpen||historyLoading||historyOpen||historyClosing){choice.complete(request);return;}
        command("choiceSelect",request.payload(),ignored->{if(!destroyed){choice.complete(request);updateChoiceInput();}});
    }
    private void layoutScene(){
        if(scene==null||root.getWidth()==0)return;int width=root.getWidth(),height=root.getHeight();int bar=dp(width>height*16f/9f?68:58);if(!menuInitialized||menuDefaultWidth!=width||menuDefaultHeight!=height){menu=prefs.menuVisible(width,height,bar);menuInitialized=true;menuDefaultWidth=width;menuDefaultHeight=height;}
        int bufferWidth=prefs.renderDimension(width),bufferHeight=prefs.renderDimension(height);
        if(bufferWidth!=surfaceBufferWidth||bufferHeight!=surfaceBufferHeight){surfaceBufferWidth=bufferWidth;surfaceBufferHeight=bufferHeight;surface.getHolder().setFixedSize(bufferWidth,bufferHeight);}
        boolean rails=width>height*16f/9f;float density=getResources().getDisplayMetrics().density,scaledDensity=getResources().getDisplayMetrics().scaledDensity;
        boolean catalog=frontendsActive(),intermission=teatimeActive(),effectiveMenu=!inputSession.restricted()&&!catalog&&TeatimeControlsView.reserve(intermission,menu);int contentWidth=width-(effectiveMenu&&rails?2*bar:0),contentHeight=height-(effectiveMenu&&!rails?bar:0);
        int panelHeight=text.panelHeight(scene,prefs,contentWidth,contentHeight,density,scaledDensity);
        viewport=new Viewport(width,height,bar,effectiveMenu,prefs.dynamic(),scene.width,scene.height,panelHeight);if(historyDialog!=null)historyDialog.contentArea(viewport.content,width,height);TextOverlay.RenderState textState=text.prepare(scene,viewport,hidden);renderer.update(scene,viewport,prefs,hidden,textState);choice.bounds(viewport.content);updateChoiceInput();layoutInputProgress();text.setVisibility(scene.movie==null&&!intermission&&!catalog?View.VISIBLE:View.GONE);if(renderer.ready(scene))text.present(scene,viewport,hidden);controls.setVisibility(intermission||catalog?View.GONE:View.VISIBLE);if(catalog)return;if(intermission){teatime.layout(width,height,bar,rails);return;}
        updateMenuStates();
        if(!menuGeometry.changed(width,height,bar,effectiveMenu,density))return;
        controls.removeAllViews();menuButtons.clear();if(!effectiveMenu)return;
        String[] labels={"セーブ","ロード","履歴","オート","スキップ","非表示","タイトル","設定"};String[] ids={"save","load","history","auto","skip","hide","title","settings"};
        for(int group=0;group<(rails?2:1);group++){
            LinearLayout strip=new LinearLayout(this);strip.setOrientation(rails?LinearLayout.VERTICAL:LinearLayout.HORIZONTAL);strip.setGravity(Gravity.CENTER);
            GradientDrawable panel=menuShape(0xcc101e33,rails?16:0);
            strip.setBackground(rails?new InsetDrawable(panel,dp(4),dp(8),dp(4),dp(8)):panel);strip.setPadding(dp(rails?8:4),dp(rails?16:4),dp(rails?8:4),dp(rails?16:4));
            // Empty rail space belongs to the menu and must not advance the story.
            strip.setClickable(true);strip.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            FrameLayout.LayoutParams sp=new FrameLayout.LayoutParams(rails?bar:-1,rails?-1:bar);sp.gravity=rails?(group==0?Gravity.LEFT:Gravity.RIGHT):Gravity.BOTTOM;controls.addView(strip,sp);
            for(int i=rails?group*4:0;i<(rails?group*4+4:8);i++){
                final String id=ids[i];MenuButton button=new MenuButton(id,labels[i],rails);menuButtons.put(id,button);
                LinearLayout.LayoutParams bp=rails?new LinearLayout.LayoutParams(-1,railItemHeight(height,density)):new LinearLayout.LayoutParams(0,-1,1);
                if(rails){if(i%4!=0)bp.topMargin=railGap(height,density);}else bp.setMargins(dp(2),0,dp(2),0);
                strip.addView(button,bp);button.setOnClickListener(v->menuAction(id));
            }
        }
        updateMenuStates();
    }
    private void menuAction(String id){if(readerPullOpen)return;if(inputSession.restricted()&&!id.equals("settings"))return;if(id.equals("settings"))cancelInputHold();if(teatimeActive()||frontendsActive())return;if(choiceActive()&&(id.equals("auto")||id.equals("skip")||id.equals("history")))return;if(id.equals("skip")&&scene!=null&&scene.movie!=null){command("movieSkip",scene.movie.optString("id"));return;}switch(id){case "settings":openSettings();break;case "hide":hidden=!hidden;layoutScene();break;case "save":case "load":slots(id);break;case "title":new AlertDialog.Builder(this).setMessage(UiText.text(this,"タイトルに戻りますか？")).setNegativeButton(UiText.text(this,"キャンセル"),null).setPositiveButton(UiText.text(this,"戻る"),(d,w)->command("title","")).show();break;case "history":history();break;default:command(id,"");}}
    private void updateMenuStates(){if(scene==null)return;for(Map.Entry<String,MenuButton> item:menuButtons.entrySet()){String id=item.getKey();item.getValue().setEnabled(!id.equals("save")||scene.json.optBoolean("canSave"));if(choiceActive()&&(id.equals("auto")||id.equals("skip")||id.equals("history")))item.getValue().setEnabled(false);item.getValue().active(id.equals("auto")?scene.json.optBoolean("automatic"):id.equals("skip")?scene.movie==null&&scene.json.optBoolean("skipping"):id.equals("hide")&&hidden);}}
    private GradientDrawable menuShape(int color,float radius){GradientDrawable shape=new GradientDrawable();shape.setColor(color);shape.setCornerRadius(dp(radius));return shape;}
    static int railGap(int height,float density){return Math.round((height<300*density?4:8)*density);}
    static int railItemHeight(int height,float density){return Math.max(1,Math.min(Math.round(72*density),(height-Math.round(32*density)-3*railGap(height,density))/4));}
    private final class MenuButton extends LinearLayout {
        private final String id,title;
        private final ImageView image;
        private final TextView label;
        private boolean active,bound;
        MenuButton(String id,String title,boolean rails){
            super(MainActivity.this);this.id=id;this.title=UiText.text(MainActivity.this,title);setOrientation(VERTICAL);setGravity(Gravity.CENTER);setClickable(true);setFocusable(true);setDefaultFocusHighlightEnabled(false);
            StateListDrawable colors=new StateListDrawable();colors.addState(new int[]{android.R.attr.state_selected},menuShape(0xcc355f88,13));colors.addState(new int[]{android.R.attr.state_focused},menuShape(0xcc294867,13));colors.addState(new int[]{},menuShape(rails?0x00000000:0x402c4668,13));
            setBackground(new RippleDrawable(ColorStateList.valueOf(0x3fa4c8ee),colors,menuShape(Color.WHITE,13)));setPadding(dp(2),dp(3),dp(2),dp(3));
            image=new ImageView(MainActivity.this);image.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);addView(image,new LinearLayout.LayoutParams(dp(rails?22:20),dp(rails?22:20)));
            label=new TextView(MainActivity.this);label.setTextSize(11);label.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));label.setGravity(Gravity.CENTER);label.setIncludeFontPadding(false);label.setSingleLine(true);label.setEllipsize(android.text.TextUtils.TruncateAt.END);label.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.topMargin=dp(rails?5:3);addView(label,lp);
            setAccessibilityDelegate(new View.AccessibilityDelegate(){@Override public void onInitializeAccessibilityNodeInfo(View host,android.view.accessibility.AccessibilityNodeInfo info){super.onInitializeAccessibilityNodeInfo(host,info);info.setClassName(Button.class.getName());boolean toggle=id.equals("auto")||id.equals("skip")||id.equals("hide");info.setCheckable(toggle);if(toggle)info.setChecked(active);}});
            active(false);
        }
        void active(boolean next){
            if(bound&&active==next)return;bound=true;active=next;setSelected(next);String caption=id.equals("hide")&&next?UiText.text(MainActivity.this,"表示"):title;
            label.setText(caption);label.setTextColor(next?0xfff1f6ff:0xffcfdded);image.setImageDrawable(new IconDrawable(next&&(id.equals("auto")||id.equals("skip"))?"pause":id.equals("hide")&&next?"show":id));image.setImageTintList(ColorStateList.valueOf(next?0xffd7e9ff:0xffdceaff));
            String description=id.equals("hide")?(next?"メッセージを表示":"メッセージを隠す"):id.equals("title")?"タイトルに戻る":title+(next?UiText.text(MainActivity.this,"・オン"):"");setContentDescription(UiText.text(MainActivity.this,description));setTooltipText(getContentDescription());
        }
    }
    private void slots(String action){slots(action,()->{},()->true);}
    private void slots(String action,Runnable closed,java.util.function.BooleanSupplier valid){slots(action,closed,valid,false,0);}
    private void slots(String action,Runnable closed,java.util.function.BooleanSupplier valid,boolean pull,long pullSerial){
        if(slotUiOpen||destroyed||action.equals("save")&&(scene==null||!scene.json.optBoolean("canSave"))){if(pull)discardReaderPull(true);return;}slotUiOpen=true;updateChoiceInput();long serial=++slotSessionSerial;SlotSession flow=new SlotSession();
        java.util.function.BooleanSupplier live=()->!destroyed&&slotUiOpen&&serial==slotSessionSerial&&valid.getAsBoolean();
        Runnable finish=()->{if(!flow.finish())return;slotUiOpen=false;slotCancel=null;slotSessionSerial++;releaseSlotThumbnail();if(!destroyed&&!paused)command(errorModalOpen||historyLoading||historyOpen||historyClosing?"pause":"resume",Long.toString(SystemClock.uptimeMillis()),ignored->closed.run());else closed.run();};
        slotCancel=()->{flow.listClosed();flow.operationEnded();finish.run();};ui.removeCallbacks(tick);
        command("pause",Long.toString(SystemClock.uptimeMillis()),pausedScene->{
            if(!live.getAsBoolean()||pausedScene==null){flow.listClosed();finish.run();if(pull&&readerPullOpen&&pullSerial==readerPullSerial)discardReaderPull(true);return;}
            final SlotGridDialog[] picker={null};
            java.util.function.BiConsumer<Boolean,SlotThumbnailStore.Entry> selection=(saving,entry)->{
                if(!live.getAsBoolean())return;flow.confirmOpened();picker[0].dismiss();
                AlertDialog confirm=new AlertDialog.Builder(this).setTitle(UiText.text(this,saving?"セーブ":"ロード")).setMessage(UiText.format(this,saving?(entry.exists?"confirm_overwrite":"confirm_save"):"confirm_load",entry.slot)).setNegativeButton(UiText.text(this,"キャンセル"),null).setPositiveButton(UiText.text(this,"OK"),(dialog,n)->{
                    if(!live.getAsBoolean())return;flow.operationStarted();
                    if(saving)captureSlotPreview(scene,live,image->{
                        if(!live.getAsBoolean()){if(image!=null)image.recycle();flow.operationEnded();finish.run();return;}
                        pendingSlotThumbnail=image;
                        command("save",Integer.toString(entry.slot),next->{
                            android.graphics.Bitmap captured=pendingSlotThumbnail;pendingSlotThumbnail=null;
                            if(live.getAsBoolean()&&next!=null&&next.json.optString("notice").equals("Saved"))engine.execute(()->slotThumbnails.saved(entry.slot,captured));else if(captured!=null)captured.recycle();
                            flow.operationEnded();finish.run();
                        });
                    });else command("load",Integer.toString(entry.slot),next->{flow.operationEnded();finish.run();});
                }).create();slotConfirmation=confirm;confirm.setOnDismissListener(dialog->{if(slotConfirmation==confirm)slotConfirmation=null;flow.confirmClosed();finish.run();});confirm.show();
            };
            boolean available=action.equals("save")||pausedScene.json.optBoolean("canSave");
            picker[0]=pull&&readerPullDialog instanceof SlotGridDialog?(SlotGridDialog)readerPullDialog:new SlotGridDialog(this,slotThumbnails,action.equals("save"),available,selection);picker[0].selection(available,selection);

            slotDialog=picker[0];picker[0].setOnDismissListener(dialog->{if(slotDialog==picker[0])slotDialog=null;flow.listClosed();finish.run();});if(pull)prepareReaderPullDialog(picker[0],pullSerial);if(!picker[0].isShowing())picker[0].show();if(pull)readerPullDialogShown(pullSerial);
        });
    }
    private void captureSlotPreview(Scene expected,java.util.function.BooleanSupplier live,Consumer<android.graphics.Bitmap> completion){
        if(expected==null||!expected.json.optBoolean("canSave")||expected.movie!=null||expected.transition!=null){completion.accept(null);return;}
        awaitSlotFrame(expected,live,SystemClock.uptimeMillis()+1000,completion);
    }
    private void awaitSlotFrame(Scene expected,java.util.function.BooleanSupplier live,long deadline,Consumer<android.graphics.Bitmap> completion){
        if(!live.getAsBoolean()||paused||errorModalOpen||scene!=expected||SystemClock.uptimeMillis()>deadline){completion.accept(null);return;}
        if(committedScene!=expected||!renderer.ready(expected)){ui.postDelayed(()->awaitSlotFrame(expected,live,deadline,completion),16);return;}
        Viewport capturedViewport=viewport;if(capturedViewport==null){completion.accept(null);return;}
        boolean[] submitted={false};Runnable timeout=()->{if(!submitted[0]){submitted[0]=true;completion.accept(null);}};ui.postDelayed(timeout,1500);
        // A queued GL event runs after the last onDrawFrame and EGL swap. Paused
        // Core clocks + blocked game input keep that Surface and save state fixed.
        surface.queueEvent(()->ui.post(()->{
            if(submitted[0])return;submitted[0]=true;ui.removeCallbacks(timeout);
            java.util.function.BooleanSupplier same=()->live.getAsBoolean()&&!paused&&!errorModalOpen&&scene==expected&&committedScene==expected&&viewport==capturedViewport&&renderer.ready(expected);
            if(!same.getAsBoolean()){completion.accept(null);return;}
            RectF area=capturedViewport.content;android.graphics.Rect crop=new android.graphics.Rect(Math.max(0,Math.round(area.left)),Math.max(0,Math.round(area.top)),Math.min(surface.getWidth(),Math.round(area.right)),Math.min(surface.getHeight(),Math.round(area.bottom)));
            SlotThumbnailCapture.request(surface,text,crop,same,completion);
        }));
    }
    private void releaseSlotThumbnail(){if(pendingSlotThumbnail!=null){pendingSlotThumbnail.recycle();pendingSlotThumbnail=null;}}
    static final class SlotSession {
        private boolean list=true,confirmation,operation,finished;
        void confirmOpened(){confirmation=true;}void listClosed(){list=false;}void confirmClosed(){confirmation=false;}void operationStarted(){operation=true;}void operationEnded(){operation=false;}
        boolean finish(){if(finished||list||confirmation||operation)return false;finished=true;return true;}
    }
    private boolean teatimeActive(){return teatime!=null&&teatime.active()||scene!=null&&"teatime".equals(scene.json.optString("scene",""));}
    private void teatimeAction(TeatimeControlsView.Request request){command("modalAction",request.payload(),next->teatime.complete(request));}
    private void openTeatimeSave(){if(paused||destroyed||isFinishing()||teatime==null)return;String key=teatime.takeSave();if(key==null)return;String id=teatime.generation();savingModalId=id;slots("save",()->{TeatimeControlsView.Request returned=teatime.finishSave(key);if(returned!=null)teatimeAction(returned);},()->teatime.saving(id));}
    private void dismissSlots(){slotSessionSerial++;AlertDialog confirm=slotConfirmation;Dialog list=slotDialog;slotConfirmation=null;slotDialog=null;if(confirm!=null)confirm.dismiss();if(list!=null)list.dismiss();Runnable cancel=slotCancel;if(cancel!=null)cancel.run();releaseSlotThumbnail();}
    private void history(){history(false,0);}
    private void history(boolean pull,long pullSerial){
        if(scene==null||!scene.json.optString("scene").equals("play")||historyLoading||historyOpen||historyClosing||slotUiOpen||errorModalOpen||teatimeActive()||frontendsActive()||choiceActive()){if(pull)discardReaderPull(true);return;}
        historyLoading=true;historyRevision="";long serial=++historyUiSerial;updateCorePause();
        command("history","",next->{
            if(serial!=historyUiSerial||!historyLoading){if(historyClosing)finishHistoryClose(next);return;}
            if(next==null||!next.json.optString("scene").equals("history")){historyLoading=false;updateCorePause();if(pull&&readerPullOpen&&pullSerial==readerPullSerial)discardReaderPull(true);return;}
            if(paused||errorModalOpen||isFinishing()||!next.json.optString("error").isEmpty()){closeHistory(true);finishHistoryClose(next);return;}
            HistoryModel model=HistoryModel.parse(next.json);historyRevision=model.revision;historyLoading=false;historyOpen=true;
            final HistoryView[] view={pull&&readerPullDialog instanceof HistoryView?(HistoryView)readerPullDialog:createHistoryView(model)};view[0].replaceModel(model);historyDialog=view[0];view[0].setOnDismissListener(dialog->{if(historyDialog==view[0])closeHistory(true);});view[0].contentArea(viewport==null?null:viewport.content,root.getWidth(),root.getHeight());if(pull)prepareReaderPullDialog(view[0],pullSerial);if(!view[0].isShowing())view[0].show();if(pull)readerPullDialogShown(pullSerial);view[0].replay(next.json.optJSONObject("historyReplay"));
        });
    }
    private void replayHistory(HistoryView view,HistoryModel.Group group){
        if(view!=historyDialog||!historyOpen||paused||errorModalOpen||!group.playable){view.pending(false);return;}
        long serial=historyUiSerial;command("historyReplay",group.payload(view.model.revision),next->{if(serial==historyUiSerial&&historyDialog==view){view.pending(false);if(next!=null)view.replay(next.json.optJSONObject("historyReplay"));}});
    }
    private void stopHistoryReplay(){
        if(audio!=null)audio.updateHistoryReplay(null,this::assetFile);String pair=scene==null?"":HistoryModel.replayPair(scene.json.optJSONObject("historyReplay"));if(pair.isEmpty())return;
        HistoryView view=historyDialog;long serial=historyUiSerial;if(view!=null)view.pending(true);command("historyReplayStop",pair,next->{if(view!=null&&view==historyDialog&&serial==historyUiSerial){view.pending(false);if(next!=null)view.replay(next.json.optJSONObject("historyReplay"));}});
    }
    private void syncHistory(Scene next){
        if(historyOpen&&historyDialog!=null){if(!next.json.optString("scene").equals("history"))closeHistory(false);else if(!next.json.optString("historyRevision").equals(historyRevision))closeHistory(true);else historyDialog.replay(next.json.optJSONObject("historyReplay"));}
    }
    private void closeHistory(boolean closeCore){
        if(!historyOpen&&!historyLoading&&!historyClosing)return;boolean opening=historyLoading;HistoryView old=historyDialog;historyDialog=null;historyUiSerial++;historyOpen=historyLoading=false;historyClosing=closeCore&&!destroyed;
        if(audio!=null)audio.updateHistoryReplay(null,this::assetFile);if(old!=null&&old.isShowing())old.dismiss();
        if(!historyClosing){updateCorePause();return;}if(!opening)finishHistoryClose(scene);
    }
    private void finishHistoryClose(Scene current){
        if(!historyClosing)return;long serial=historyUiSerial;String revision=current==null?"":current.json.optString("historyRevision");
        if(current!=null&&current.json.optString("scene").equals("history")&&!revision.isEmpty())command("historyClose",revision,next->{if(serial==historyUiSerial){historyClosing=false;historyRevision="";updateCorePause();}});
        else{historyClosing=false;historyRevision="";updateCorePause();}
    }
    private void updateMovie(JSONObject movie){
        if(moviePlayer==null)return;
        if(movie!=null)try{movie=new JSONObject(movie.toString());movie.put("volume",movie.optDouble("volume",.4)*prefs.volume("bgm")/100.);}catch(JSONException e){error(UiText.format(this,"movie_state_error",e.getMessage()));return;}
        moviePlayer.update(movie,paused);
    }
    private void movieEvent(MoviePlayer.Event event){
        if(destroyed)return;String type,payload;
        switch(event.type){
            case "prepared":type="moviePrepared";payload=event.id+" "+event.durationMs;break;
            case "position":type="moviePosition";payload=moviePayload(event);break;
            case "seeked":type="movieSeeked";payload=moviePayload(event);break;
            case "ended":type="movieEnded";payload=moviePayload(event);break;
            case "error":type="movieError";payload=event.id+" "+event.message;break;
            default:return;
        }
        command(type,payload);
    }
    private static String moviePayload(MoviePlayer.Event event){return event.id+" "+event.positionMs+" "+event.durationMs+" "+event.seekSerial;}
    private void refreshUiLanguage(){
        UiText.refresh(this);String selected=UiText.language();if(selected.equals(activityUiLanguage))return;
        activityUiLanguage=selected;menuGeometry.invalidate();if(root!=null)UiText.refreshTree(root);
    }
    @Override protected void onResume(){super.onResume();if(root==null)return;try{Installation install=new Installation(this);if(!install.ready()||!install.dataAvailable()||!gameRoot.equals(install.dataRoot(install.read()))){recreate();return;}}catch(Exception error){recreate();return;}discardReaderPull(false);refreshUiLanguage();paused=false;settingsOpen=false;immersive();if(surface!=null)surface.onResume();if(audio!=null)audio.resume();syncPlaybackPreferences(true);if(handle!=0)command("language",GameLanguage.effective(this));updateCorePause();if(scene!=null){layoutScene();audio.update(scene.json.optJSONArray("audio"),this::assetFile);if(!slotUiOpen&&!errorModalOpen&&!historyLoading&&!historyOpen&&!historyClosing&&scene.json.optString("error","").isEmpty()&&scene.json.optLong("nextTickMs",0)>0)ui.post(tick);}if(moviePlayer!=null)moviePlayer.setPaused(false);openTeatimeSave();maybeReturnToTitle();}
    @Override protected void onPause(){if(root==null){super.onPause();return;}if(!settingsOpen)discardReaderPull(false);haltReturn.invalidate();cancelInputHold();paused=true;updateChoiceInput();ui.removeCallbacks(tick);command("pause",Long.toString(SystemClock.uptimeMillis()));if(audio!=null)audio.pause();if(moviePlayer!=null)moviePlayer.setPaused(true);if(surface!=null)surface.onPause();super.onPause();}
    @Override public void dump(String prefix,FileDescriptor fd,PrintWriter writer,String[] args){super.dump(prefix,fd,writer,args);writer.println(prefix+"WoHUiError "+JSONObject.quote(lastError));writer.println(prefix+"WoHActivity paused="+paused+" destroyed="+destroyed+" menu="+menu+" hidden="+hidden+" historyLoading="+historyLoading+" historyOpen="+historyOpen+" staleRenderCommits="+staleRenderCommits+" transitionAck="+transitionHandshake.sent+" tickScheduled="+(Build.VERSION.SDK_INT>=29&&ui.hasCallbacks(tick)));Scene value=scene;if(value!=null)writer.println(prefix+"WoHState "+value.json);value=committedScene;if(value!=null)writer.println(prefix+"WoHRendered "+value.json);if(audio!=null)writer.println(prefix+"WoHAudio "+audio.debugState());if(renderer!=null){writer.println(prefix+"WoHWave "+renderer.waveDebugState());writer.println(prefix+"WoHNoise "+renderer.noiseDebugState());}}
    @Override public void onConfigurationChanged(Configuration config){if(root==null){super.onConfigurationChanged(config);return;}if(!settingsOpen)discardReaderPull(true);cancelInputHold();super.onConfigurationChanged(config);refreshUiLanguage();if(handle!=0)command("language",GameLanguage.effective(this));immersive();if(root!=null)root.post(this::layoutScene);}
    private void handleBack(){if(root==null){finish();return;}if(readerPullOpen){if(readerPullCompleting)discardReaderPull(true);else finishReaderPull(false);return;}if(inputSession.restricted()){cancelInputHold();openSettings();return;}if(slotConfirmation!=null&&slotConfirmation.isShowing()){slotConfirmation.cancel();return;}if(slotDialog!=null&&slotDialog.isShowing()){slotDialog.cancel();return;}if(slotUiOpen)return;if(historyOpen||historyLoading){closeHistory(true);return;}if(historyClosing)return;if(gallery.active()){gallery.back();return;}if(musicRoom.active()){musicRoom.back();return;}if(archive.active()){archive.back();return;}if(teatimeActive()){teatime.back();return;}if(scene!=null&&scene.movie!=null){command("movieSkip",scene.movie.optString("id"));return;}toggleMenu();}
    @Override public void onWindowFocusChanged(boolean focused){super.onWindowFocusChanged(focused);if(root==null)return;if(!focused){if(!settingsOpen)discardReaderPull(true);haltReturn.invalidate();cancelInputHold();}else{maybeReturnToTitle();maybeShowReaderHelp();}}
    @Override public void onBackPressed(){handleBack();}
    @Override protected void onDestroy(){if(root==null){destroyed=true;ui.removeCallbacksAndMessages(null);engine.shutdown();super.onDestroy();return;}discardReaderPull(false);destroyed=true;if(readerHelpDialog!=null)readerHelpDialog.dismiss();haltReturn.invalidate();if(choice!=null)choice.close();closeHistory(false);if(errorDialog!=null)errorDialog.dismiss();dismissSlots();ui.removeCallbacksAndMessages(null);if(audio!=null)audio.close();if(moviePlayer!=null)moviePlayer.close();if(renderer!=null)renderer.shutdown();if(archive!=null)archive.close();if(gallery!=null)gallery.close();if(musicRoom!=null)musicRoom.close();engine.execute(()->{if(handle!=0)NativeBridge.destroy(handle);handle=0;});engine.shutdown();if(slotThumbnails!=null)slotThumbnails.close();super.onDestroy();}
    static String historyPlainText(String value){return value.replaceAll("<([^<>|]+)\\|[^<>]*>","$1");}
    static final class MenuGeometry {
        private int width=-1,height=-1,bar=-1,densityBits;
        private boolean menu;
        void invalidate(){width=-1;}
        boolean changed(int width,int height,int bar,boolean menu,float density){
            int bits=Float.floatToIntBits(density);if(this.width==width&&this.height==height&&this.bar==bar&&this.menu==menu&&densityBits==bits)return false;
            this.width=width;this.height=height;this.bar=bar;this.menu=menu;densityBits=bits;return true;
        }
    }
    static final class TwoFingerTap {
        private final Map<Integer,float[]> starts=new HashMap<>();
        private final Set<Integer> active=new HashSet<>();
        private long startTime;
        private float movementLimit;
        private int downs;
        private boolean multiple,valid,finished;
        void start(int id,float x,float y,long time,float limit){starts.clear();active.clear();starts.put(id,new float[]{x,y});active.add(id);startTime=time;movementLimit=limit;downs=1;multiple=false;valid=true;finished=false;}
        void pointerDown(int id,float x,float y){multiple=true;downs++;if(downs!=2||active.contains(id)||finished)valid=false;starts.put(id,new float[]{x,y});active.add(id);}
        void move(int id,float x,float y){float[] first=starts.get(id);if(first==null||!Float.isFinite(x)||!Float.isFinite(y)||Math.hypot(x-first[0],y-first[1])>movementLimit)valid=false;}
        void pointerUp(int id){if(!active.remove(id))valid=false;}
        boolean multiple(){return multiple;}
        boolean finish(int id,long time){boolean tap=multiple&&valid&&!finished&&downs==2&&active.size()==1&&active.contains(id)&&time>=startTime&&time-startTime<=700;active.remove(id);finished=true;return tap;}
        void cancel(){valid=false;finished=true;active.clear();}
    }
    private int dp(float value){return Math.round(value*getResources().getDisplayMetrics().density);}
}
