// Native mode0/2 share branches, restricted to the audited argument domain.
static void nativeTransitionModeTwoDomain(){
 for(auto args:std::vector<std::array<int,6>>{{{2,101,4,-1,256,0}},{{2,100,3,-1,256,0}},{{2,100,4,-1,255,0}},{{2,100,4,-1,256,1}},{{2,250,5,0,256,1}},{{2,200,5,0,80,0}},{{2,100,5,0,256,0}},{{2,900,11,-1,256,0}}}){transition200Profile(args,false);args[0]=0;transition200Profile(args,false);}
 // Boundary acceptance only: the one-day maximum must not be simulated with
 // millions of callbacks. Native clock arithmetic is unchanged and reused.
 for(auto args:std::vector<std::array<int,6>>{{{2,0,0,-1,0,0}},{{2,1,14,0,256,1}},{{2,2,1,1,1,0}},{{2,17,7,-1,255,1}},{{2,50,4,0,128,0}},{{2,200,5,0,80,0}},{{2,100,5,0,256,0}},{{2,86400000,14,1,256,1}},{{2,1,0,1,0,1}},{{2,86400000,1,-1,0,0}}}){
  Program p;p.image();transition200Request(p,args);p.dialogue(0);Fixture f(std::move(p),true);transition200Masks(f);auto state=f.command("action","start");check(waiting(state,"transitionReady")&&number(state,"nativeMode")==2,"reviewed mode2 boundary uses existing actual prepared scene path");f.passed=true;
 }
 for(auto args:std::vector<std::array<int,6>>{{{2,-1,5,0,256,0}},{{2,86400001,5,0,256,0}},{{2,100,-1,0,256,0}},{{2,100,15,0,256,0}},{{2,100,5,-2,256,0}},{{2,100,5,2,256,0}},{{2,100,5,0,-1,0}},{{2,100,5,0,257,0}},{{2,100,5,0,256,-1}},{{2,100,5,0,256,2}},{{3,100,5,0,256,0}}}){
  Program p;p.image();transition200Request(p,args);Fixture f(std::move(p),true);transition200Masks(f);auto state=std::string(woh_command(f.handle,"action","start"));check(!transition200Quoted(state,"error").empty(),"unreviewed mode2 field adjacent to domain is still rejected");f.passed=true;
 }
 {Program p;p.image();transition200Request(p,{{2,900,11,-1,256,0}});Fixture f(std::move(p),true);auto state=std::string(woh_command(f.handle,"action","start"));check(state.find("Missing prepared native mask map")!=std::string::npos,"mode2 domain acceptance never bypasses the shared mode0 resource guard");f.passed=true;}
}
