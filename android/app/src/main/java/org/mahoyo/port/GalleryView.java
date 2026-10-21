package org.mahoyo.port;

import android.content.Context;
import android.graphics.Typeface;
import android.view.*;
import android.widget.*;
import org.json.JSONObject;
import java.io.File;
import java.util.function.*;

final class GalleryView extends FrontendPanel {
    private ExtraModels.Gallery model;private final BiConsumer<String,String> command;
    private final GridView grid;private final ZoomImageView viewer;private final TextView loading,counter;
    private final ImageButton previous,next,fit;private String key="";private boolean busy;
    GalleryView(Context c,Function<String,GameAsset> files,BiConsumer<String,String> command,Consumer<String> errors){super(c,"図録",files,errors);this.command=command;
        counter=new TextView(c);counter.setTextColor(0xffafc2dc);counter.setTextSize(13);toolbar.addView(counter,new LinearLayout.LayoutParams(-2,-2));previous=button("previous","前の差分",()->send("previous"));next=button("next","次の差分",()->send("next"));fit=button("fit","全体表示",this::fitViewer);for(ImageButton b:new ImageButton[]{previous,next,fit})toolbar.addView(b,new LinearLayout.LayoutParams(dp(44),dp(48)));
        grid=new GridView(c);grid.setPadding(dp(12),dp(12),dp(12),dp(20));grid.setClipToPadding(false);grid.setHorizontalSpacing(dp(8));grid.setVerticalSpacing(dp(8));grid.setStretchMode(GridView.STRETCH_COLUMN_WIDTH);grid.setOnItemClickListener((p,v,pos,id)->{if(model!=null&&!busy&&model.groups.get(pos).count>0){busy=true;command.accept("galleryAction",model.select(model.groups.get(pos).id));}});body.addView(grid,new FrameLayout.LayoutParams(-1,-1));
        viewer=new ZoomImageView(c);body.addView(viewer,new FrameLayout.LayoutParams(-1,-1));loading=new TextView(c);loading.setText(UiText.text(getContext(),"読込中…"));loading.setTextColor(0xffdceaff);loading.setGravity(Gravity.CENTER);body.addView(loading,new FrameLayout.LayoutParams(-1,-1));
    }
    @Override boolean active(){return model!=null;}
    private void fitViewer(){viewer.fit();}
    void state(JSONObject value){ExtraModels.Gallery next=ExtraModels.gallery(value);shown(next!=null);if(next==null){model=null;return;}model=next;busy=false;String nextKey=model.id+":"+model.group+":"+model.variant;if(!nextKey.equals(key)){key=nextKey;render();}}
    private void send(String action){if(model!=null&&!busy){busy=true;command.accept("galleryAction",model.action(action));}}
    @Override void back(){send("back");}
    @Override void release(){grid.setAdapter(null);viewer.setImageDrawable(null);key="";}
    @Override void resized(){if(model!=null){grid.setNumColumns(ExtraModels.columns(getWidth(),getResources().getDisplayMetrics().density,196));if(model.group>=0)loadViewer();}}
    private void render(){if(model==null)return;images.invalidate();boolean detail=model.group>=0;grid.setVisibility(detail?GONE:VISIBLE);viewer.setVisibility(detail?VISIBLE:GONE);loading.setVisibility(detail?VISIBLE:GONE);backButton.setContentDescription(UiText.text(getContext(),detail?"一覧に戻る":"タイトルに戻る"));backButton.setTooltipText(backButton.getContentDescription());for(ImageButton b:new ImageButton[]{previous,next,fit})b.setVisibility(detail?VISIBLE:GONE);previous.setEnabled(model.variants.size()>1);next.setEnabled(model.variants.size()>1);
        if(detail){int position=0;for(int i=0;i<model.variants.size();i++)if(model.variants.get(i).index==model.variant)position=i+1;counter.setText(position+" / "+model.variants.size());loadViewer();}
        else{counter.setText(UiText.text(getContext(),""));viewer.setImageDrawable(null);grid.setNumColumns(ExtraModels.columns(getWidth(),getResources().getDisplayMetrics().density,196));grid.setAdapter(new Cards());if(model.focus>=0)for(int i=0;i<model.groups.size();i++)if(model.groups.get(i).id==model.focus){grid.setSelection(i);break;}}
    }
    private void loadViewer(){if(model==null||model.current()==null)return;if(body.getWidth()==0||body.getHeight()==0){post(this::loadViewer);return;}loading.setVisibility(VISIBLE);images.bind(model.current().name,null,viewer,body.getWidth(),body.getHeight(),()->loading.setVisibility(GONE));}
    private final class Cards extends BaseAdapter {
        public int getCount(){return model==null?0:model.groups.size();}public Object getItem(int p){return model.groups.get(p);}public long getItemId(int p){return model.groups.get(p).id;}
        public boolean isEnabled(int p){return model!=null&&model.groups.get(p).count>0;}
        public View getView(int position,View recycled,ViewGroup parent){LinearLayout cell;ImageView image;TextView label;if(recycled==null){cell=new LinearLayout(getContext());cell.setOrientation(VERTICAL);cell.setPadding(dp(5),dp(5),dp(5),dp(6));cell.setBackground(shape(0xb32a405e,11));image=new ImageView(getContext());image.setScaleType(ImageView.ScaleType.CENTER_CROP);cell.addView(image,new LinearLayout.LayoutParams(-1,dp(104)));label=new TextView(getContext());label.setTextColor(0xffcfdded);label.setTextSize(12);label.setTypeface(Typeface.create("sans-serif-medium",0));label.setGravity(Gravity.CENTER);cell.addView(label,new LinearLayout.LayoutParams(-1,dp(25)));}else cell=(LinearLayout)recycled;image=(ImageView)cell.getChildAt(0);label=(TextView)cell.getChildAt(1);ExtraModels.Group group=model.groups.get(position);label.setText(String.format(java.util.Locale.ROOT,"%03d",group.id+1)+(group.count>1?" · "+group.count:""));cell.setAlpha(group.count==0?.45f:1);cell.setContentDescription(group.count==0?UiText.text(getContext(),"未開放"):UiText.format(getContext(),"gallery_description",group.id+1,group.count));if(group.count>0)images.bind(group.preview,null,image,dp(240),dp(135),null);else{image.setTag(null);image.setImageDrawable(new IconDrawable("lock"));image.setScaleType(ImageView.ScaleType.CENTER_INSIDE);}if(group.count>0)image.setScaleType(ImageView.ScaleType.CENTER_CROP);return cell;}
    }
}
