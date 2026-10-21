package org.mahoyo.port;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.*;
import android.widget.*;
import org.json.JSONObject;
import java.io.File;
import java.util.function.*;

final class MusicView extends FrontendPanel {
    private ExtraModels.Music model;private final BiConsumer<String,String> command;private final GridView grid;
    private final TextView status;private final ImageButton previous,play,stop,next,repeat;
    private long displayed;private String stateKey="";private boolean busy;
    MusicView(Context c,Function<String,GameAsset> files,BiConsumer<String,String> command,Consumer<String> errors){super(c,"楽曲",files,errors);this.command=command;status=new TextView(c);status.setTextColor(0xffb9cbb7);status.setTextSize(13);toolbar.addView(status,new LinearLayout.LayoutParams(-2,-2));
        grid=new GridView(c);grid.setPadding(dp(12),dp(12),dp(12),dp(20));grid.setClipToPadding(false);grid.setHorizontalSpacing(dp(8));grid.setVerticalSpacing(dp(8));grid.setStretchMode(GridView.STRETCH_COLUMN_WIDTH);grid.setOnItemClickListener((p,v,pos,id)->{if(model!=null&&!busy&&!model.closing()&&model.tracks.get(pos).unlocked){busy=true;command.accept("musicAction",model.play(pos));}});body.addView(grid,new FrameLayout.LayoutParams(-1,-1));
        LinearLayout controls=new LinearLayout(c);controls.setGravity(Gravity.CENTER);controls.setPadding(dp(12),dp(4),dp(12),dp(4));controls.setBackgroundColor(0xe61a2a41);previous=button("previous","前の曲",()->send("previous"));play=button("auto","再生",()->send("play"));stop=button("stop","停止",()->send("stop"));next=button("next","次の曲",()->send("next"));repeat=button("repeat","同じ曲を繰り返す",()->send("repeat"));for(ImageButton b:new ImageButton[]{previous,play,stop,next,repeat}){LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(dp(56),dp(48));lp.setMargins(dp(3),0,dp(3),0);controls.addView(b,lp);}addView(controls,new LinearLayout.LayoutParams(-1,dp(64)));
    }
    @Override boolean active(){return model!=null;}
    void state(JSONObject value){ExtraModels.Music next=ExtraModels.music(value);shown(next!=null);if(next==null){model=null;return;}model=next;busy=false;if(displayed!=model.id){displayed=model.id;images.invalidate();grid.setNumColumns(ExtraModels.columns(getWidth(),getResources().getDisplayMetrics().density,280));grid.setAdapter(new Tracks());if(model.selected>=0)grid.setSelection(model.selected);}
        String key=model.id+":"+model.phase+":"+model.row+":"+model.repeat;if(!key.equals(stateKey)){stateKey=key;if(grid.getAdapter()!=null)((BaseAdapter)grid.getAdapter()).notifyDataSetChanged();}refresh();
    }
    private void send(String action){if(model==null||busy||model.closing())return;busy=true;command.accept("musicAction",model.action(action));refresh();}
    @Override void back(){send("back");}
    @Override void release(){grid.setAdapter(null);displayed=0;stateKey="";}
    @Override void resized(){if(model!=null)grid.setNumColumns(ExtraModels.columns(getWidth(),getResources().getDisplayMetrics().density,280));}
    private void refresh(){if(model==null)return;boolean available=false;for(ExtraModels.Track t:model.tracks)available|=t.unlocked;boolean enabled=!busy&&!model.closing();previous.setEnabled(enabled&&available);next.setEnabled(enabled&&available);play.setEnabled(enabled&&available&&model.row<0);stop.setEnabled(enabled&&model.row>=0);repeat.setEnabled(enabled);backButton.setEnabled(enabled);repeat.setSelected(model.repeat);repeat.setImageTintList(ColorStateList.valueOf(model.repeat?0xffb6d4a1:0xffdceaff));repeat.setBackground(shape(model.repeat?0xcc355f88:0,14));repeat.setContentDescription(UiText.text(getContext(),model.repeat?"同じ曲を繰り返す：オン":"同じ曲を繰り返す：オフ"));status.setText(UiText.text(getContext(),model.phase.equals("switching")?"切替中…":model.phase.equals("closing")?"閉じています…":model.phase.equals("playing")?"再生中":model.phase.equals("stopping")?"停止中…":""));}
    private final class Tracks extends BaseAdapter {
        public int getCount(){return model==null?0:model.tracks.size();}public Object getItem(int p){return model.tracks.get(p);}public long getItemId(int p){return p;}public boolean isEnabled(int p){return model!=null&&model.tracks.get(p).unlocked&&!model.closing();}
        public View getView(int position,View recycled,ViewGroup parent){LinearLayout cell;ImageView indicator,label;if(recycled==null){cell=new LinearLayout(getContext());cell.setGravity(Gravity.CENTER_VERTICAL);cell.setPadding(dp(8),dp(7),dp(10),dp(7));indicator=new ImageView(getContext());cell.addView(indicator,new LinearLayout.LayoutParams(dp(23),dp(23)));label=new ImageView(getContext());label.setScaleType(ImageView.ScaleType.FIT_CENTER);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(43),1);lp.leftMargin=dp(7);cell.addView(label,lp);}else cell=(LinearLayout)recycled;indicator=(ImageView)cell.getChildAt(0);label=(ImageView)cell.getChildAt(1);ExtraModels.Track track=model.tracks.get(position);boolean selected=model.row==position;cell.setBackground(shape(selected?0xcc355f88:0x9920324c,12));cell.setAlpha(track.unlocked?1:.38f);indicator.setImageDrawable(new IconDrawable(track.unlocked?(selected?"sound":"auto"):"lock"));indicator.setImageTintList(ColorStateList.valueOf(selected?0xffd7e9ff:0xffa8bdd7));cell.setContentDescription(UiText.format(getContext(),"track_number",position+1)+(track.unlocked?(selected?" · "+UiText.text(getContext(),"再生中"):""):" · "+UiText.text(getContext(),"未開放")));if(track.unlocked)images.bind(track.atlas,track.crop,label,dp(320),dp(44),null);else{label.setTag(null);label.setImageDrawable(null);}return cell;}
    }
}
