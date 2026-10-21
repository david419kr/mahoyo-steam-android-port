#include "effects.h"
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <chrono>
using namespace mahoyo::effects;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void close(double actual,double expected,const char* message){check(std::abs(actual-expected)<1e-12,message);}
static void liveWindowFrames(){
    Clip clip;clip.duration=1000;Frame a,b;a.time=0;b.time=1000;a.values.fill(-32767);b.values.fill(-32767);a.values[Alpha]=255;b.values[Alpha]=0;a.globalIndex=692;b.globalIndex=693;
    a.window={870,0,-32767,-32767,-32767,-32767};b.window=a.window;clip.frames={a,b};
    close(sample(clip,500,WindowWidth,889),889,"window hold uses the playback initial width");
    close(sample(clip,500,Alpha,0),127.5,"original alpha before global capture");
    FrameOverrides global;global[692]=FrameOverride{20,30,80,{5,6,700,800}};
    close(sample(clip,500,Alpha,0,&global),40,"live global capture changes an existing playback");
    close(sample(clip,500,WindowWidth,889,&global),700,"captured width flows through later hold sentinel");
    global[692].window[2]=640;close(sample(clip,500,WindowWidth,889,&global),640,"repeated capture remains a live frame lookup");
    close(sample(clip,0,X,240,&global),19.5,"captured X key uses native endpoint pixel-grid truncation");
    close(sample(clip,0,WindowSoft,75,&global),75,"capture does not replace soft or color");
    check(clip.frames[0].values[Alpha]==255&&clip.frames[0].window[2]==-32767,"immutable source frame remains unchanged");
    auto path=std::filesystem::temp_directory_path()/("mahoyo-window-frames-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
    {std::ofstream out(path);out<<"205 0 692 0x4144080 870 0 -32767 -32767 -32767 -32767\n205 1 693 0x4144080 870 0 -32767 -32767 -32767 -32767\n";}
    std::map<int,Clip> clips{{205,clip}};loadWindows(clips,path);check(clips[205].windowMetadata&&clips[205].descriptor==0x4144080,"sidecar records descriptor and native identity");
    {std::ofstream out(path);out<<"205 0 692 0x4144080 0 0 1 1 0 0\n";}
    bool failed=false;try{loadWindows(clips,path);}catch(const std::exception&){failed=true;}check(failed&&clips[205].frames[0].window[0]==870,"incomplete sidecar fails atomically");
    std::filesystem::remove(path);
}
static void polynomialCurves(){
    // Independently enumerated quarter/half/three-quarter values. These expose
    // the degree and in/out order, including native0xd's fourth power.
    const double expected[][3]={
        {.25,.5,.75},{.25,.5,.75},{.0625,.25,.5625},{.4375,.75,.9375},
        {.125,.5,.875},{.375,.5,.625},{.015625,.125,.421875},{.578125,.875,.984375},
        {.0625,.5,.9375},{.4375,.5,.5625},{.00390625,.0625,.31640625},{.68359375,.9375,.99609375},
        {.03125,.5,.96875},{.46875,.5,.53125},{.0009765625,.03125,.2373046875},{.7626953125,.96875,.9990234375},
        {.015625,.5,.984375},{.484375,.5,.515625}
    };
    for(int curve=0;curve<=17;++curve){
        close(ease(0,curve),0,"polynomial start endpoint");close(ease(1,curve),1,"polynomial end endpoint");
        close(ease(-.25,curve),0,"normalized lower clamp");close(ease(1.25,curve),1,"normalized upper clamp");
        for(int i=0;i<3;++i)close(ease((i+1)*.25,curve),expected[curve][i],"independent polynomial sample");
        Clip clip;clip.duration=1000;Frame start;start.values.fill(-32766);start.values[Alpha]=250;start.curve=curve;
        Frame end=start;end.time=1000;end.values[Alpha]=50;end.curve=-32767;clip.frames={start,end};
        close(sample(clip,250,Alpha,0),250-200*expected[curve][0],"keyframe sampler shares easing for descending values");
    }
    for(const auto pair:{std::array<int,2>{2,3},{6,7},{10,11},{14,15}})
        for(double p:{.125,.375,.625,.875})close(ease(p,pair[0])+ease(1-p,pair[1]),1,"in/out reversal symmetry");
    for(int curve:{4,5,8,9,12,13,16,17}){
        for(double p:{.125,.375,.625,.875})close(ease(p,curve)+ease(1-p,curve),1,"two-part curve midpoint symmetry");
        close(ease(.5,curve),.5,"two-part curve midpoint");
        check(std::abs(ease(.5-1e-8,curve)-.5)<6e-8&&std::abs(ease(.5+1e-8,curve)-.5)<6e-8,"two-part curve continuity");
    }
    for(int curve:{-1,18,71}){bool rejected=false;try{ease(.25,curve);}catch(const std::runtime_error&){rejected=true;}check(rejected,"unsupported native curves remain explicit errors");}
    Clip unsupported;unsupported.duration=100;Frame a;a.values.fill(-32766);a.values[Alpha]=0;a.curve=18;Frame b=a;b.time=100;b.values[Alpha]=100;unsupported.frames={a,b};
    bool rejected=false;try{sample(unsupported,50,Alpha,0);}catch(const std::runtime_error&){rejected=true;}check(rejected,"sampler preserves unsupported easing errors");
}
static void nativeLinearFrameAliases(){
    for(int curve=0;curve<=32767;++curve){
        if(!(curve<=1||(curve>=42&&curve<=69)||curve>=71))continue;
        for(float elapsed:{0.f,1.f,99.f,200.f}){
            check(interpolateNativeCurve(curve,elapsed,1.3f,-4.7f,200)==interpolateNativeCurve(0,elapsed,1.3f,-4.7f,200),"linear frame aliases reuse original0/1 float multiplication/division/addition");
        }
    }
    for(int curve:{-32768,-32767,-32766,-1,18,41,32768,INT32_MAX}){
        bool rejected=false;try{interpolateNativeCurve(curve,1,0,1,200);}catch(const std::runtime_error&){rejected=true;}
        if(curve==41)check(!rejected,"existing nonlinear41 remains supported");
        else check(rejected,"negative/unreviewed/outside-signed16 curves remain strict");
    }
    check(ease(.25,70)!=.25,"special70 is not classified as a linear alias");
    bool genericRejected=false;try{ease(.25,62);}catch(const std::runtime_error&){genericRejected=true;}check(genericRejected,"unrelated normalized animation API is not broadened by frame aliases");
    Clip clip;clip.duration=200;Frame a,b;a.values.fill(-32767);b.values.fill(-32767);a.time=0;b.time=200;a.curve=62;b.curve=0;a.mode=0;b.mode=-32767;a.values[X]=1132;b.values[X]=861;a.values[Y]=-84;b.values[Y]=-62;clip.frames={a,b};
    check(sample(clip,50,X,1132)==1064.0625&&sample(clip,50,Y,-84)==-78.375,"actual5134 selected XY applies Native grid then normalized curve62 float affine");
    check(clip.frames[0].curve==62,"sampling never rewrites the raw curveID to0");
    clip.frames[0].mode=1;auto plain=clip;plain.frames[0].curve=0;for(uint64_t tick:{1,50,99,100,199,200})check(sample(clip,tick,X,1132)==sample(plain,tick,X,1132),"mode1 frame aliases retain existing0/1 progress policy");
}
static void nativeRotationEndpointAndCurveFour(){
    Clip mode;mode.duration=2700;
    const int times[]={0,400,1500,2100,2700},values[]={-3000,-100001,4600,-100001,0},modes[]={0,2,1,-32767,0};
    for(int i=0;i<5;++i){Frame f;f.time=times[i];f.values.fill(-32767);f.values[Rotation]=values[i];f.mode=modes[i];f.curve=i?-32767:0;mode.frames.push_back(f);}
    for(int initial:{0,1333,-3000,16000}){
        check(sample(mode,2700,Rotation,initial)==-.000244140625,"Native mode1 evaluates a real endpoint with endpoint/endpoint inputs instead of snapping to its key");
        check(sample(mode,2717,Rotation,initial)==-.000244140625,"nonloop phase clamp retains the Native mode1 endpoint residue");
    }
    mode.frames.back().values[Rotation]=-100001;check(sample(mode,2700,Rotation,0)==4600,"legacy HOLD endpoint stays unchanged");
    mode.frames.back().values[Rotation]=-100000;check(sample(mode,2700,Rotation,0)==0,"Native final SKIP endpoint returns the supplied initial before real-key backtracking");
    mode.frames.back().values[Rotation]=0;mode.frames[2].mode=0;check(sample(mode,2700,Rotation,0)==0,"other-mode endpoints keep their existing exact key behavior");
    check(interpolateNativeCurve(4,4999,0,1,5000)==.9999998807907104f,"Native curve4 keeps its intermediate float32 progress rather than a final-cast double formula");
    Clip curve;curve.duration=10000;curve.loopStart=1;
    for(int i=0;i<3;++i){Frame f;f.time=i*5000;f.values.fill(-32767);f.values[Rotation]=i==1?2000:16000;f.curve=i==2?-32767:4;f.mode=i?-32767:0;curve.frames.push_back(f);}
    check(sample(curve,4999,Rotation,1333)==2000.001953125,"actual rotation uses Native normalized curve4 followed by float32 affine operations");
    check(sample(curve,10000,Rotation,1333)==16000,"curve4 exact endpoint remains unchanged");
    check(sample(curve,10001,Rotation,1333)==15999.9951171875,"original loop1 normalization precedes the curve4 sample");
}
static void nativeExponentialAndBounce(){
    //Original084D90 quarter samples;27 includes the native1.001 correction.
    const float expected[][3]={{.8240465521812439f,.9697187542915344f,.9954702258110046f},
        {.47265625f,.765625f,.97265625f},{.3828125f,.5f,.6171875f}};
    const int curves[]={27,39,41};
    for(size_t i=0;i<3;++i){
        int curve=curves[i];check(ease(-1,curve)==0&&ease(2,curve)==1,"new easing IDs retain normalized time bounds");
        check(interpolateNativeCurve(curve,0,-3,7,100)==-3&&interpolateNativeCurve(curve,100,-3,7,100)==4,"original new-curve endpoints");
        for(int j=0;j<3;++j)check(interpolateNativeCurve(curve,float(25*(j+1)),0,1,100)==expected[i][j],"original exponential/bounce float sample");
    }
    check(interpolateNativeCurve(27,99.99989318847656f,0,1,100)==1.0000224113464355f,"native27 near-final overshoot is preserved before exact endpoint snap");
    check(ease(4./11,39)>.99&&ease(.5,39)<.8,"bounce39 retains its reversal rather than monotonic smoothing");
    auto segment=[](int curve,int duration){Clip c;c.duration=duration;Frame a,b;a.values.fill(-32767);b.values.fill(-32767);a.curve=curve;a.mode=0;b.time=duration;c.frames={a,b};return c;};
    auto c=segment(41,950);c.frames[0].values[X]=419;c.frames[1].values[X]=540;c.frames[0].values[Y]=46;c.frames[1].values[Y]=70;c.frames[0].values[Rotation]=-2558;c.frames[1].values[Rotation]=-1139;
    check(sample(c,475,X,0)==479.25&&sample(c,475,Y,0)==57.75&&sample(c,475,Rotation,1333)==-1848.5,"effect1814 curve41 uses native normalized affine after endpoint grid");
    c=segment(39,2500);c.frames[0].values[X]=133;c.frames[1].values[X]=25;c.frames[0].values[Y]=204;c.frames[1].values[Y]=272;c.frames[0].values[Rotation]=-1100;c.frames[1].values[Rotation]=-6100;
    check(sample(c,1250,X,0)==50.0625&&sample(c,1250,Y,0)==255.6796875&&sample(c,1250,Rotation,1333)==-4928.125,"effect1970 inherited mode0 uses the same bounce progress for selected fields");
    c=segment(27,3200);c.frames[0].values[X]=208;c.frames[1].values[X]=298;c.frames[0].values[Y]=340;c.frames[1].values[Y]=349;c.frames[0].values[ScaleX]=-1000;c.frames[1].values[ScaleX]=-1500;c.frames[0].values[ScaleY]=-200;c.frames[1].values[ScaleY]=-1000;
    check(sample(c,1600,X,0)==295.0246887207031&&sample(c,1600,Y,0)==348.47747802734375&&sample(c,1600,ScaleX,0)==-1484.859375&&sample(c,1600,ScaleY,0)==-975.7750244140625,"effect1983 positions and negative scales preserve normalized f32 affine order");
    for(auto row:{std::array<int,4>{0,12000,-3232,-172},{1,12000,-3232,-172},{10,500,-4500,-200},{6,400,-36000,0}}){
        c=segment(row[0],row[1]);c.frames[0].values[Rotation]=row[2];c.frames[1].values[Rotation]=row[3];
        double expected=row[0]<2?-172.2548828125:row[0]==10?-234.29638671875:-269.32421875;
        check(sample(c,uint64_t(row[1]-1),Rotation,1333)==expected,"actual1904/1920/2019/2020 cancellation uses native f32 progress and affine boundaries");
    }
    for(int curve:{26,28,38,42}){bool rejected=false;try{ease(.5,curve);}catch(const std::runtime_error&){rejected=true;}check(rejected,"neighboring unreviewed easing IDs stay unsupported");}
}
static void nativeMaskProgress(){
    // Native14c840 quantizes elapsed*256/duration before the root transform.
    // Values below were independently calculated with sqrt/sqrt(sqrt), not
    // by calling the production pow expression to produce expected results.
    const uint32_t at[]={0,1,64,128,192,240,255,256};
    const uint32_t quarter[]={0,1,18,41,75,128,192,256};
    const uint32_t square[]={0,1,35,75,128,192,240,256};
    for(int strength:{0,1,49,50,99,100,255,256}){
        for(size_t i=0;i<8;++i){
            auto expected=strength<50?quarter[i]:strength<100?square[i]:at[i];
            check(maskProgress(at[i],256,strength)==expected,"native independently computed mask progress sample");
        }
        check(maskProgress(0,0,strength)==256,"zero duration commits immediately without division");
        check(maskProgress(257,256,strength)==256,"elapsed beyond duration remains at final progress");
        check(maskProgress(0,1000,strength)==0&&maskProgress(1000,1000,strength)==256,"mask progress exact endpoints");
        check(maskProgress(3,1000,strength)==0,"elapsed quantization precedes nonlinear progress transform");
        check(maskProgress(4,1000,strength)==maskProgress(1,256,strength),"first1/256 boundary uses native integer quotient");
        uint32_t prior=0;
        for(uint64_t t=0;t<=1000;++t){auto p=maskProgress(t,1000,strength);check(p>=prior&&p<=256,"mask progress stays bounded and monotonic");prior=p;}
    }
    check(maskProgress(128,256,49)==41&&maskProgress(128,256,50)==75,"strength50 selects square rather than fourth root");
    check(maskProgress(128,256,99)==75&&maskProgress(128,256,100)==128,"strength100 selects linear progress");
    for(int strength:{-1,257}){bool rejected=false;try{maskProgress(0,1000,strength);}catch(const std::runtime_error&){rejected=true;}check(rejected,"invalid native mask strength remains explicit error");}
}
static Clip propertyClip(){
    Clip c;c.duration=1000;c.propertiesMetadata=true;
    c.descriptor=0x10002|0x20004|0x02000000|0x200|0x20000000|0x40000000|0x40|0x100|1;
    Frame a,b;a.values.fill(-32767);b.values.fill(-32767);b.time=1000;c.frames={a,b};return c;
}
static void nativeProperties(){
    static_assert(WindowSoft==11&&OriginX==12&&Priority==24,"existing field IDs stay stable");
    auto c=propertyClip();auto& a=c.frames[0];
    auto set=[&](Field field,int value){a.properties[size_t(field-OriginX)]=value;};
    set(OriginX,29);set(OriginY,386);set(BlendMode,17);set(ColorMode,1);set(ColorR,229);set(ColorG,255);set(ColorB,255);
    set(VariantX,10);set(VariantY,10);set(Brightness,-25);set(Contrast,40);set(Priority,7890);
    Properties initial;initial.originX=-40;initial.originY=31;initial.blendMode=5;initial.colorMode=4;initial.rgb=0x112233;
    initial.brightness=8;initial.contrast=9;initial.priority=450;
    for(uint64_t t:{0,500,1000,2000}){
        auto p=sampleProperties(c,t,initial);
        check(p.originX==38&&p.originY==514,"effect193 origin keys convert to native integer resource origins");
        check(p.blendMode==17&&p.colorMode==1&&p.rgb==0xe5ffff,"effect652 native blend and RGB state");
        check(p.variantX==10&&p.variantY==10,"effect652 preserves native variant index pair");
        check(p.brightness==-25&&p.contrast==40&&p.priority==7890,"signed filter and priority values persist after hold and EOF");
    }
    set(ColorMode,2);check(sampleProperties(c,500,initial).rgb==0,"native non-solid color modes clear RGB cache fields");
    set(ColorMode,1);set(ColorR,-32767);set(ColorG,-32767);set(ColorB,-32767);
    check(sampleProperties(c,500,initial).rgb==0,"native mode1 RGB fallback is zero rather than initial packed RGB");
    set(BlendMode,26);check(sampleProperties(c,500,initial).blendMode==0,"A1830 maps out-of-range mode to0");
    c.descriptor=0;auto held=sampleProperties(c,500,initial);
    check(held.rgb==initial.rgb&&held.brightness==8&&held.originX==-40&&held.priority==450,"unselected descriptor fields do not mutate layer properties");
    c.descriptor=0x1000;c.frames[0].properties[size_t(AuxResourceAndScale-OriginX)]=1;
    bool rejected=false;try{sampleProperties(c,0,initial);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"active unknown field26 remains an explicit error");
    c.frames[0].properties[size_t(AuxResourceAndScale-OriginX)]=-32767;
    check(sampleProperties(c,0,initial).priority==450,"all-hold auxiliary channel does not invent a new operation");
    Clip missing;rejected=false;try{sampleProperties(missing,0,initial);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"absent property metadata cannot silently omit native channels");
}
static void nativeElastic31(){
    close(ease(0,31),0,"native elastic31 exact beginning");close(ease(1,31),1,"native elastic31 exact final value");
    close(ease(-1,31),0,"elastic clamps negative time");close(ease(2,31),1,"elastic clamps time beyond duration");
    //Independent angles: sin(pi/6),sin(5pi/6),sin(13pi/6),sin(17pi/6).
    close(ease(.1,31),1.25,"native elastic first overshoot");close(ease(.2,31),1.125,"native elastic second angular sample");
    close(ease(.4,31),1.03125,"native elastic decaying overshoot");close(ease(.5,31),1.015625,"native elastic midpoint");
    check(ease(.001,31)>0&&ease(.001,31)<.01,"one millisecond of1000ms follows native start branch");
    check(ease(.125,31)>1.36&&ease(.25,31)<.92,"elastic overshoot and undershoot are not clipped or linearized");
    check(ease(.999,31)>1,"native final snap is preserved rather than smoothed away");
    Clip c;c.duration=800;Frame a,b;a.values.fill(-32767);b.values.fill(-32767);a.values[Y]=861;b.values[Y]=449;a.curve=31;b.curve=0;b.time=800;c.frames={a,b};
    close(sample(c,0,Y,0),861,"effect699 initial Y is quantized before elastic sampling");
    close(sample(c,800,Y,0),448.5,"effect699 exact endpoint uses future key and not first curve overshoot");
    check(sample(c,100,Y,0)<311,"effect699 real Y movement retains native elastic overshoot");
}
static void nativeSamplingModes(){
    Clip c;c.duration=1000;Frame a,b;a.values.fill(-32766);b.values.fill(-32766);
    a.values[Alpha]=0;b.values[Alpha]=100;b.time=1000;a.mode=2;c.frames={a,b};
    close(sample(c,500,Alpha,7),0,"native mode2 retains the preceding endpoint");
    close(sample(c,1000,Alpha,7),100,"mode2 switches at the exact endpoint");
    c.frames[0].mode=0;c.frames[1].curve=18;
    close(sample(c,500,Alpha,7),50,"curve comes from preceding raw frame, not the future endpoint");
    c.frames[0].mode=1;close(sample(c,500,Alpha,7),50,"mode1 with fewer than3 effective points uses native linear fallback");
    c.frames[0].mode=3;bool rejected=false;try{sample(c,500,Alpha,7);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"unreviewed mode3 remains an explicit error");
    c.frames[0].mode=0;c.frames[1].curve=0;
    for(auto field:{X,Y})for(int sign:{-1,1}){
        c.frames[0].values[field]=0;c.frames[1].values[field]=20*sign;
        close(sample(c,500,field,0),9.75*sign,"native X/Y endpoints are truncated on the internal pixel grid before interpolation");
        close(sample(c,1000,field,0),19.5*sign,"native grid applies at EOF and preserves truncation direction");
    }
    auto props=propertyClip();props.frames[0].properties[size_t(BlendMode-OriginX)]=0;props.frames[1].properties[size_t(BlendMode-OriginX)]=19;
    close(sample(props,500,BlendMode,7),0,"native blend field forces mode2 even when frame requests0");
    props.frames[0].properties[size_t(VariantX-OriginX)]=0;props.frames[1].properties[size_t(VariantX-OriginX)]=20;
    close(sample(props,500,VariantX,7),10,"image variant follows the reviewed frame mode rather than forced step");
    Frame skipped=props.frames[0];skipped.time=300;skipped.mode=2;skipped.curve=2;skipped.properties.fill(-32766);
    props.frames.insert(props.frames.begin()+1,skipped);
    close(sample(props,300,VariantX,7),6,"at a skipped raw-frame boundary metadata still comes from the preceding raw frame");
    close(sample(props,301,VariantX,7),0,"after skipped raw frame its mode2 governs the next segment");
    c.frames[0].values[X]=-32767;c.frames[1].values[X]=20;
    close(sample(c,500,X,30.25),30.25,"leading hold does not invent interpolation from playback initial state");
    close(sample(c,1000,X,30.25),19.5,"leading hold ends when the numeric key is reached");
    c.frames[0].values[X]=-32766;
    close(sample(c,0,X,30.25),19.5,"native leading skip exposes the first usable key at startup");
    close(sample(c,500,X,30.25),30.25,"leading skip has no earlier numeric interpolation endpoint");
    c.frames[0].values[Alpha]=0;c.frames[1].values[Alpha]=100;c.loopStart=10;
    close(sample(c,1000,Alpha,0),100,"loop preserves final key at exact duration");
    close(sample(c,1001,Alpha,0),1.100000023841858,"loop wraps only after duration and retains native f32 affine rounding");
}
static Clip modeOneClip(Field field,const std::vector<int>& values,const std::vector<uint32_t>& times){
    Clip c;c.duration=times.back();c.descriptor=UINT64_MAX;c.windowMetadata=true;c.propertiesMetadata=true;
    for(size_t i=0;i<values.size();++i){
        Frame f;f.time=times[i];f.curve=0;f.mode=1;f.globalIndex=int(i);
        f.values.fill(-32767);f.values[Rotation]=-100001;
        if(field<WindowX)f.values[size_t(field)]=values[i];
        else if(field<OriginX)f.window[size_t(field-WindowX)]=values[i];
        else f.properties[size_t(field-OriginX)]=values[i];
        c.frames.push_back(f);
    }
    return c;
}
static void nativeModeOne(){
    auto c=modeOneClip(Rotation,{-6200,-5700,-5900},{0,100,300});c.frames[1].mode=0;c.frames[1].curve=7;c.frames[2].mode=-32767;
    close(sample(c,25,Rotation,1333),-6033.984375,"actual889 mode1 quarter");
    close(sample(c,50,Rotation,-2000),-5884.375,"actual889 mode1 midpoint ignores unused baseline");
    close(sample(c,200,Rotation,0),-5875,"actual889 inherits mode0 and curve7 after100ms");
    close(sample(c,301,Rotation,0),-5900,"actual889 terminal clamp");
    c=modeOneClip(Rotation,{5,10,-20,30},{0,100,200,300});
    close(sample(c,50,Rotation,20),12.756250381469727,"four points preserve native elimination bug");
    close(sample(c,150,Rotation,20),-7.643749237060547,"raw-index map selects middle coefficient pair");
    c.frames[0].curve=3;close(sample(c,50,Rotation,20),13.349218368530273,"curve3 runs before mode1 cubic evaluation");
    c.frames[0].curve=7;close(sample(c,50,Rotation,20),12.24951171875,"curve7 runs before mode1 cubic evaluation");
    c=modeOneClip(Rotation,{0,100,200,300},{0,100,200,300});
    close(sample(c,50,Rotation,0),49.97500228881836,"native mode1 is not a standard natural spline");
    c=modeOneClip(Rotation,{5,5,5,10},{0,100,200,300});
    close(sample(c,50,Rotation,0),4.53125,"triple-equal middle key compression retains native undershoot");
    close(sample(c,150,Rotation,0),4.53125,"compressed raw keys share the same coefficient index");
    c=modeOneClip(Rotation,{5,-100000,15,-5},{0,100,200,300});
    close(sample(c,150,Rotation,20),14.9609375,"SKIP is omitted from coefficient points and interpolation interval");
    c=modeOneClip(Rotation,{-100001,9,15},{0,100,300});
    close(sample(c,50,Rotation,20),20,"leading HOLD remains initial until the real key");
    close(sample(c,200,Rotation,20),10.40625,"leading HOLD baseline influences later cubic curvature");
    close(sample(c,200,Rotation,0),12.28125,"mode1 coefficients cannot be cached independently of initial state");
    for(auto field:{BlendMode,ColorMode,ColorR,ColorG,ColorB,Brightness,Contrast,AuxResourceAndScale,WindowColor}){
        c=modeOneClip(field,{5,10,20},{0,100,200});close(sample(c,50,field,0),5,"categorical fields override mode1 with native step2");
    }
    for(auto field:{VariantX,VariantY,Priority}){
        c=modeOneClip(field,{5,10,-20,30},{0,100,200,300});
        close(sample(c,50,field,0),12.756250381469727,"variant and priority do not force mode2");
    }
    c=modeOneClip(OriginX,{5,10,-20,30},{0,100,200,300});Properties initial;
    check(sampleProperties(c,50,initial).originX==17,"mode1 native integer P conversion follows scalar interpolation");
    c=modeOneClip(Rotation,{0,100,200,300,400,500,600},{0,50,100,150,200,250,300});c.loopStart=1;
    close(sample(c,301,Rotation,0),sample(c,2,Rotation,0),"mode1 retains native positive-loop normalization");
    c=modeOneClip(Alpha,{0,0,0,0},{0,100,200,300});
    auto expected=modeOneClip(Alpha,{5,10,-20,30},{0,100,200,300});FrameOverrides overrides;
    for(int i=0;i<4;++i)overrides[i].alpha=expected.frames[size_t(i)].values[Alpha];
    close(sample(c,50,Alpha,0,&overrides),sample(expected,50,Alpha,0),"coefficient builder uses live frame overrides before quantization");
    c=modeOneClip(Alpha,std::vector<int>(129,3),std::vector<uint32_t>(129));c.duration=128;
    for(size_t i=0;i<c.frames.size();++i)c.frames[i].time=uint32_t(i);
    bool rejected=false;try{sample(c,50,Alpha,0);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"mode1 exceeds reviewed native frame-array bound explicitly");
}
static void propertySidecars(){
    auto path=std::filesystem::temp_directory_path()/("mahoyo-effect-properties-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
    Clip c=propertyClip();c.propertiesMetadata=false;c.windowMetadata=true;c.descriptor=1;c.frames[0].globalIndex=1959;c.frames[1].globalIndex=1960;
    std::map<int,Clip> clips{{647,c}};
    auto row=[&](int index,int identity,const std::string& descriptor,int priority){
        std::ofstream out(path,index?std::ios::app:std::ios::out);out<<"647 "<<index<<' '<<identity<<' '<<descriptor;
        for(int i=0;i<12;++i)out<<" -32767";out<<' '<<priority<<'\n';
    };
    row(0,1959,"0x1",11290);row(1,1960,"0x1",-32767);loadProperties(clips,path);
    check(clips.at(647).propertiesMetadata&&clips.at(647).frames[0].properties[12]==11290,"property sidecar retains global identity and original signed key");
    Properties initial;initial.priority=450;check(sampleProperties(clips.at(647),500,initial).priority==11290,"loaded effect647 actual priority sample");
    auto reject=[&](const char* message){bool failed=false;try{loadProperties(clips,path);}catch(const std::exception&){failed=true;}check(failed&&clips.at(647).frames[0].properties[12]==11290,message);};
    row(0,1959,"0x1",3);reject("incomplete property sidecar is rejected atomically");
    row(0,1959,"0x2",3);row(1,1960,"0x2",-32767);reject("window and property descriptors must agree");
    row(0,1958,"0x1",3);row(1,1960,"0x1",-32767);reject("sidecar global identity cannot drift from window frames");
    row(0,1959,"0x1",32768);row(1,1960,"0x1",-32767);reject("property values retain signed16 bounds");
    row(0,1959,"0x1",3);row(1,1960,"0x1",-32767);{std::ofstream out(path,std::ios::app);out<<"extra\n";}reject("unexpected property rows fail before replacing valid data");
    auto unknown=clips;unknown.at(647).windowMetadata=false;unknown.at(647).propertiesMetadata=false;
    {std::ofstream out(path);out<<"647 0 1959 0x1000";for(int i=0;i<13;++i)out<<' '<<(i==11?1:-32767);out<<'\n';}
    bool auxiliary=false;try{loadProperties(unknown,path);}catch(const std::exception&){auxiliary=true;}
    check(auxiliary&&!unknown.at(647).propertiesMetadata,"active auxiliary channel rejects property load before mutation");
    std::filesystem::remove(path);
}
static void nativeClockAndEffectIdentity(){
    const float times[]={0,1,50,100,150,200,299,300,301};
    const float expected[]={0,.012999999336898327f,.6499999761581421f,1.2999999523162842f,1.126562476158142f,1.0374999046325684f,1,1,1};
    for(size_t i=0;i<9;++i)check(progress70(times[i],300)==expected[i],"original curve70 elapsed samples preserve overshoot and float boundaries");
    check(ease(1./3,70)>1.29&&ease(.5,70)>1.12&&ease(.5,70)!=ease(.5,6),"70 is independent clock overshoot, not flag64 pluscurve6");
    for(int id:{1,243,646,915,916,4096,8372})check(hasIdentityTimeRemap(id),"full native map admits reviewed identity-time effect");
    for(int id:{-1,0,8373,3096,6675})check(!hasIdentityTimeRemap(id),"true-remap effects and IDs outside native count remain unsupported");
    auto path=std::filesystem::temp_directory_path()/("mahoyo-effect-count-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
    for(int id:{0,1,4096,8372,8373}){
        {std::ofstream output(path);output<<id<<" 0 0 0 0 1000 1000 0 0 0 0 100\n";}
        bool accepted=false;try{auto clips=load(path);accepted=clips.size()==1&&clips.count(id);}catch(const std::runtime_error&){}
        check(accepted==(id>=1&&id<=8372),"effect parser uses original8372 count, not artificial4095 limit");
    }
    std::filesystem::remove(path);
}
static void specialEffectTime(){
    Clip c;c.duration=4000;c.timeRemap=true;c.timeCurve=4;
    Frame a;a.values.fill(-32766);a.values[Alpha]=0;a.curve=4;
    Frame b=a;b.time=4000;b.values[Alpha]=1000;b.curve=-32767;c.frames={a,b};
    auto time=prepareTime(c,1000);check(time.phase==1000&&time.mapped==500&&time.segmentLinear&&!time.done,"3120 native overall time is mapped once after preserving raw phase");
    close(sample(c,1000,Alpha,7),125,"remap disables segment easing rather than applying quadratic twice");
    close(samplePrepared(c,time,Alpha,7),125,"shared prepared-time sampler does not remap its mapped time again");
    check(!prepareTime(c,4000).done&&prepareTime(c,4001).done,"native nonloop completion is strict elapsed>duration");
    auto sticky=prepareTime(c,1000,true);check(sticky.mapped==4000&&sticky.done,"completed native callback keeps its endpoint on earlier input");
    check(prepareTime(c,1000).mapped==500,"fresh seek remains distinct from a completed callback");
    c.duration=1200;c.loopStart=1;c.timeCurve=6;c.frames.back().time=1200;
    auto loop=prepareTime(c,1201);check(loop.phase==2&&loop.mapped==0&&!loop.done,"6673 remap may fall below loop1 without any second clamp");
    c=modeOneClip(Alpha,{5,10,-20,30},{0,100,200,300});c.frames[0].curve=6;
    auto linear=c;linear.frames[0].curve=0;c.timeRemap=true;c.timeCurve=0;
    close(sample(c,50,Alpha,0),sample(linear,50,Alpha,0),"special bool1 suppresses segment curve but preserves native mode1 coefficients");
    c=modeOneClip(ColorMode,{5,10,20},{0,100,200});c.timeRemap=true;c.timeCurve=3;
    close(sample(c,50,ColorMode,0),5,"categorical step remains independent of global time remap");
    c=modeOneClip(Alpha,{0,100,200},{0,100,200});c.timeRemap=true;c.timeCurve=4;FrameOverrides overrides;overrides[0].alpha=111;
    auto prepared=prepareTime(c,50);close(sample(c,50,Alpha,0,&overrides),samplePrepared(c,prepared,Alpha,0,&overrides),"captured first-frame values share the same static time mapping");
    auto path=std::filesystem::temp_directory_path()/("mahoyo-special-time-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
    {std::ofstream out(path);for(int id:{915,3469})out<<id<<" 0 0 0 0 1000 1000 0 -32767 0 0 1000\n"<<id<<" 1000 100 100 255 1000 1000 0 6 0 0 1000\n";}
    auto clips=load(path);check(!clips.at(915).timeRemap&&clips.at(3469).timeRemap&&clips.at(3469).timeCurve==0,"time flag is ID-derived and first raw SKIP curve becomes0 rather than later6");
    check(prepareTime(clips.at(915),250).mapped==250&&prepareTime(clips.at(3469),250).mapped==250,"ordinary clips and first-SKIP special clips retain raw phase");
    std::filesystem::remove(path);
}
static void nativeExtraCurvesAndModeOne() {
    //Pinned084D90/A3E70 numbers; overshoot and float stage boundaries matter.
    check(interpolateNativeCurve(19,30000,0,1,60000)==0.7071067690849304f,"Native19 float-ratio then double sine");
    check(interpolateNativeCurve(34,150,0,1,300)==-0.08769750595092773f,"Native34 negative back overshoot is retained");
    for(int curve:{19,34}){check(interpolateNativeCurve(curve,0,0,1,300)==0,"new curve start");check(interpolateNativeCurve(curve,300,0,1,300)==1,"new curve end");}
    Clip c;c.duration=300;Frame a,b;a.values.fill(-32767);b.values.fill(-32767);a.values[X]=229;b.values[X]=139;a.curve=34;b.time=300;c.frames={a,b};
    check(sample(c,150,X,0)==236.6427764892578,"476 normalized34 progress before Native float affine");
    c.duration=60000;c.frames[0].values[X]=215;c.frames[1].values[X]=8;c.frames[0].curve=19;c.frames[1].time=60000;
    check(sample(c,30000,X,0)==68.12890625,"5523 normalized19 progress before Native float affine");
    c.frames[0].values[X]=564;c.frames[1].values[X]=657;c.frames[0].curve=0;c.frames[1].curve=19;c.frames[1].mode=2;
    check(sample(c,30000,X,0)==610.5,"5522 final raw19 is not a segment19 request");
    const uint32_t times[]={0, 400, 700, 1000, 1400, 1600, 1800, 2000, 2200, 2400, 2700, 3000};
    const int values[]={-3666, -100001, -100001, 16741, 38384, -100001, 37100, 36318, -100001, 35644, 32100, 33700};
    const int curves[]={0, -32767, 7, 2, 0, -32767, -32767, -32767, -32767, 8, -32767, -32767};
    const int modes[]={2, 0, 1, -32767, 0, -32767, -32767, -32767, -32767, 1, -32767, -32767};
    c={};c.duration=3000;
    for(size_t i=0;i<12;++i){Frame f;f.time=times[i];f.values.fill(-32767);f.values[Rotation]=values[i];f.curve=curves[i];f.mode=modes[i];c.frames.push_back(f);}
    const uint32_t ticks[]={699, 700, 701, 749, 750, 751, 999, 1000, 1001};
    const double expected[]={-3666.0, -3666.0, -3578.248779296875, 2248.31787109375, 2383.853515625, 2519.35546875, 16740.998046875, 16741.0, 16741.1484375};
    for(size_t i=0;i<9;++i)check(sample(c,ticks[i],Rotation,1333)==expected[i],"368 mode1 uses Native curve7 float progress");
}
int main() {
    nativeExtraCurvesAndModeOne();
    nativeLinearFrameAliases();
    nativeRotationEndpointAndCurveFour();
    nativeExponentialAndBounce();
    assert(interpolateNativeCurve(3,1799,0,1,1800)==0.9999996423721313f);
    {
        Clip c;c.duration=1800;Frame a,b;a.values.fill(-32767);b.values.fill(-32767);
        a.values[Rotation]=-4858;b.values[Rotation]=0;a.curve=3;a.mode=0;b.time=1800;c.frames={a,b};
        assert(sample(c,1799,Rotation,1333)==-0.001953125);
    }
    assert(interpolateNativeCurve(40,1,0,1,300)==0.002123594284057617f);
    assert(interpolateNativeCurve(32,606,0,1,1500)==-0.11834794282913208f);
    assert(interpolateNativeCurve(32,894,0,1,1500)==1.1183480024337769f);
    for(int curve:{32,40}){assert(interpolateNativeCurve(curve,0,-3,7,300)==-3);assert(interpolateNativeCurve(curve,300,-3,7,300)==4);}
    assert(interpolateNativeCurve(11,1,0,1,700)==0.005701899528503418f);
    {
        Clip c;c.duration=1000;Frame a,b,d;a.values.fill(-32767);b.values.fill(-32767);d.values.fill(-32767);
        a.values[ScaleX]=2000;b.values[ScaleX]=500;d.values[ScaleX]=2000;
        a.curve=2;a.mode=0;b.curve=11;b.mode=-32767;b.time=300;d.time=1000;c.frames={a,b,d};
        assert(sample(c,301,ScaleX,900)==508.5528564453125);
    }
    assert(interpolateNativeCurve(2,999,1865,-1865,1000)==3.7281494140625f);
    assert(interpolateNativeCurve(2,999,-3734,3734,1000)==-7.464111328125f);
    {
        Clip c;c.duration=1000;Frame a,b;a.values.fill(-32767);b.values.fill(-32767);
        a.values[Rotation]=1865;b.values[Rotation]=0;a.curve=2;a.mode=0;b.time=1000;c.frames={a,b};
        assert(sample(c,999,Rotation,1333)==3.72802734375);
    }
    //Original Native samples, including circular cancellation near zero and back overshoot.
    assert(interpolateNativeCurve(25,.001f,0,1000,1000)==1.006617546081543f);
    assert(interpolateNativeCurve(25,250,0,1000,1000)==433.0126953125f);
    assert(interpolateNativeCurve(35,500,0,1000,1000)==1087.697509765625f);
    assert(interpolateNativeCurve(7,1,-3000,22000,3400)==-2980.595458984375f);
    assert(ease(.5,35)>1);
    assert(ease(.25,24)>0&&ease(.25,24)<.1);
    assert(ease(.25,36)<0&&ease(.75,36)>1);
    for(int curve:{24,36}){assert(ease(0,curve)==0);assert(ease(1,curve)==1);}
    for(int curve:{25,35}){assert(ease(0,curve)==0);assert(ease(1,curve)==1);}
    nativeClockAndEffectIdentity();
    specialEffectTime();
    liveWindowFrames();
    polynomialCurves();
    nativeMaskProgress();
    nativeProperties();nativeElastic31();nativeSamplingModes();nativeModeOne();propertySidecars();
    Clip clip;clip.duration=1800;clip.loopStart=1;
    auto key=[&](uint32_t t,int alpha){Frame f;f.time=t;f.values.fill(-32766);f.values[Alpha]=alpha;f.curve=clip.frames.empty()?0:-32767;clip.frames.push_back(f);};
    key(0,0);key(450,-32767);key(600,255);key(1050,-32767);key(1200,0);key(1800,-32767);
    assert(sample(clip,225,Alpha,100)==0);
    assert(std::abs(sample(clip,525,Alpha,100)-127.5)<.001);
    assert(sample(clip,900,Alpha,100)==255);
    assert(std::abs(sample(clip,1125,Alpha,100)-127.5)<.001);
    assert(sample(clip,1500,Alpha,100)==0);
    assert(sample(clip,1800,Alpha,100)==0);
    assert(sample(clip,2700,Alpha,100)==255);
    assert(sample(clip,100,X,240)==240);
    clip.frames[1].curve=2;assert(std::abs(sample(clip,525,Alpha,100)-63.75)<.001);
    clip.frames[1].curve=3;assert(std::abs(sample(clip,525,Alpha,100)-191.25)<.001);
    clip.frames[1].curve=6;assert(std::abs(sample(clip,525,Alpha,100)-31.875)<.001);
    clip.frames[1].curve=7;assert(std::abs(sample(clip,525,Alpha,100)-223.125)<.001);
    Clip once;once.duration=100;once.loopStart=0;
    Frame start;start.time=0;start.values.fill(-32766);start.values[Alpha]=0;
    Frame end=start;end.time=100;end.values[Alpha]=255;
    once.frames={start,end};
    assert(sample(once,50,Alpha,0)==127.5);
    assert(sample(once,100,Alpha,0)==255);
    assert(sample(once,150,Alpha,0)==255);
    assert(sample(once,100000,Alpha,0)==255);
    once.frames[0].curve=8;once.frames[1].curve=-32767;
    assert(std::abs(sample(once,25,Alpha,0)-15.9375)<.001);
    assert(std::abs(sample(once,50,Alpha,0)-127.5)<.001);
    assert(std::abs(sample(once,75,Alpha,0)-239.0625)<.001);
}
