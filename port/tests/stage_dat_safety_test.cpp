// SPDX-License-Identifier: GPL-2.0-or-later
#include "stage_dat_safety.h"
#include "stage_graft.h"
#include "stage_floor_fit.h"
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

static void put(std::vector<uint8_t>& b, size_t at, uint32_t n) {
  b[at] = n >> 24; b[at+1] = n >> 16; b[at+2] = n >> 8; b[at+3] = n;
}
// Three public gameplay roots; one model, one joint and an arbitrary render mesh.
static std::vector<uint8_t> fixture(uint32_t shift, uint8_t mesh) {
  const uint32_t size = 384 + shift;
  const std::vector<uint32_t> reloc{8+shift, 48+shift, 96+16+shift};
  const std::vector<std::pair<std::string,uint32_t>> roots{{"map_head",shift},{"coll_data",256+shift},{"grGroundParam",288+shift}};
  const size_t table = 32 + size + reloc.size()*4, strings = table + roots.size()*8;
  std::vector<uint8_t> b(strings,0);
  for (const auto& r:roots) b.insert(b.end(),r.first.c_str(),r.first.c_str()+r.first.size()+1);
  put(b,0,(uint32_t)b.size()); put(b,4,size); put(b,8,(uint32_t)reloc.size()); put(b,12,(uint32_t)roots.size());
  put(b,32+shift+8,48+shift); put(b,32+shift+12,1);
  put(b,32+shift+48,96+shift); put(b,32+shift+96+16,192+shift);
  b[32+192+shift]=mesh;
  for(size_t i=0;i<reloc.size();++i) put(b,32+size+4*i,reloc[i]);
  uint32_t name=0;
  for(size_t i=0;i<roots.size();++i) { put(b,table+8*i,roots[i].second); put(b,table+8*i+4,name); name+=(uint32_t)roots[i].first.size()+1; }
  return b;
}
static void pointer(std::vector<uint8_t>& b, uint32_t field, uint32_t target) {
  using host::cosmetics::stage_safety::word;
  const uint32_t size = word(b.data()+4), count = word(b.data()+8);
  put(b,32ull+field,target);
  const size_t at=32ull+size+count*4ull;
  b.insert(b.begin()+at,4,0); put(b,at,field);
  put(b,0,(uint32_t)b.size()); put(b,8,count+1);
}
// A stage with one floor line from (-30, 0) to (30, 0) and one quad of art: level on the line, or
// leaning by `slope`, drawn or not (the joint's opaque pass bit). For stage_floor_fit.h.
static std::vector<uint8_t> floor_stage(float slope, bool drawn) {
  const uint32_t size = 0x240;
  const std::vector<uint32_t> reloc{0x008,0x040,0x090,0x0CC,0x0E8,0x0F0,0x114,0x1A0,0x1A8};
  const std::vector<std::pair<std::string,uint32_t>> roots{{"map_head",0x000},{"coll_data",0x1A0},{"grGroundParam",0x230}};
  const size_t table = 32 + size + reloc.size()*4, strings = table + roots.size()*8;
  std::vector<uint8_t> b(strings,0);
  for (const auto& r:roots) b.insert(b.end(),r.first.c_str(),r.first.c_str()+r.first.size()+1);
  put(b,0,(uint32_t)b.size()); put(b,4,size); put(b,8,(uint32_t)reloc.size()); put(b,12,(uint32_t)roots.size());
  auto f=[&](size_t at,float v){ uint32_t w; std::memcpy(&w,&v,4); put(b,32+at,w); };
  put(b,32+0x008,0x040); put(b,32+0x00C,1);                 // map_head: one model
  put(b,32+0x040,0x080);                                    // the model's joint
  put(b,32+0x084,drawn?0x40000u:0u); put(b,32+0x090,0x0C0); // joint: opaque pass bit, its mesh
  f(0x080+0x20,1); f(0x080+0x24,1); f(0x080+0x28,1);        // scale 1
  put(b,32+0x0CC,0x0E0);                                    // mesh part
  put(b,32+0x0E8,0x100); b[32+0x0EF]=1; put(b,32+0x0F0,0x180);   // vertex format, 1 block of drawing, the drawing
  put(b,32+0x100,9); put(b,32+0x104,2); put(b,32+0x108,1); put(b,32+0x10C,4);   // positions: 8-bit index, xyz, float
  b[32+0x112]=0; b[32+0x113]=12; put(b,32+0x114,0x140);
  put(b,32+0x118,0xFF);
  const float xs[4]={-30,30,30,-30}, zs[4]={-5,-5,5,5};
  for(int i=0;i<4;++i) { f(0x140+12*i,xs[i]); f(0x144+12*i,xs[i]*slope); f(0x148+12*i,zs[i]); }
  b[32+0x180]=0x80; b[32+0x182]=4; for(int i=0;i<4;++i) b[32+0x183+i]=(uint8_t)i;   // one quad
  put(b,32+0x1A0,0x200); put(b,32+0x1A8,0x220); b[32+0x1B3]=1;                       // points, lines, one floor
  f(0x200,-30); f(0x204,0); f(0x208,30); f(0x20C,0);
  b[32+0x223]=1;                                                                     // line from point 0 to 1
  for(size_t i=0;i<reloc.size();++i) put(b,32+size+4*i,reloc[i]);
  uint32_t name=0;
  for(size_t i=0;i<roots.size();++i) { put(b,table+8*i,roots[i].second); put(b,table+8*i+4,name); name+=(uint32_t)roots[i].first.size()+1; }
  return b;
}
int main(int argc,char** argv) {
  using host::cosmetics::stage_safety::matches;
  std::string reason;
  // --floor-fixture <out>: the synthetic level stage the floor tests use, for a look with --floor-line.
  if(argc==3 && std::string(argv[1])=="--floor-fixture") {
    const auto b=floor_stage(0.0f,true);
    std::ofstream(argv[2],std::ios::binary).write((const char*)b.data(),(std::streamsize)b.size());
    host::cosmetics::stage_safety::Archive z{b};
    std::printf("parse %s\n",z.parse()?"ok":"failed");
    return 0;
  }
  // --floor-line <file> <line>: that floor line's points, with MELEE_FLOOR_DEBUG's detail.
  if(argc==4 && std::string(argv[1])=="--floor-line") {
    using namespace host::cosmetics::stage_safety;
    std::ifstream fa(argv[2],std::ios::binary);
    const std::vector<uint8_t> file{std::istreambuf_iterator<char>(fa),{}};
    Archive z{file}; std::vector<floor_fit::Line> lines;
    if(!z.parse() || !floor_fit::floors(z,&lines)) { std::printf("unread\n"); return 1; }
    floor_fit::Mesh mesh{z}; mesh.build();
    const size_t i=(size_t)std::atoi(argv[3]);
    if(i>=lines.size()) { std::printf("no line %zu of %zu\n",i,lines.size()); return 1; }
    std::printf("line %zu: (%.1f, %.1f) to (%.1f, %.1f), %zu triangles\n",i,lines[i].x0,lines[i].y0,lines[i].x1,lines[i].y1,mesh.tris.size());
    {   // drawn triangles near the middle of the line: where its art is
      const float mx=(lines[i].x0+lines[i].x1)/2, my=(lines[i].y0+lines[i].y1)/2;
      int shown=0;
      for(const auto& t:mesh.tris) {
        float x0=1e9f,x1=-1e9f,y0=1e9f,y1=-1e9f,z0=1e9f,z1=-1e9f;
        for(const auto& v:t.v) { x0=std::min(x0,v.x); x1=std::max(x1,v.x); y0=std::min(y0,v.y); y1=std::max(y1,v.y); z0=std::min(z0,v.z); z1=std::max(z1,v.z); }
        if(x0>mx || x1<mx || y1<my-25 || y0>my+25 || shown>=12) continue;
        ++shown; std::printf("  near: x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f\n",x0,x1,y0,y1,z0,z1);
      }
      std::printf("  %d near shown\n",shown);
      int level=0, level_any=0;
      for(const auto& t:mesh.tris) {
        float x0=1e9f,x1=-1e9f,y0=1e9f,y1=-1e9f,z0=1e9f,z1=-1e9f;
        for(const auto& v:t.v) { x0=std::min(x0,v.x); x1=std::max(x1,v.x); y0=std::min(y0,v.y); y1=std::max(y1,v.y); z0=std::min(z0,v.z); z1=std::max(z1,v.z); }
        if(std::fabs(y0-my)<1 && std::fabs(y1-my)<1) { ++level_any; if(x0<=mx && x1>=mx) { ++level; std::printf("  level: x %.1f..%.1f z %.1f..%.1f\n",x0,x1,z0,z1); } }
      }
      std::printf("  %d level triangles cover the middle, %d level in all\n",level,level_any);
    }
    std::fflush(stdout);
    _putenv_s("MELEE_FLOOR_DEBUG","1");
    std::printf("score %.2f\n",floor_fit::score(mesh.tris,lines[i],kFloorFitTolerance));
    return 0;
  }
  // --floor <original> <online form> [tolerance]: each floor line's art fit (stage_floor_fit.h).
  if(argc>=4 && std::string(argv[1])=="--floor") {
    std::ifstream fa(argv[2],std::ios::binary), fb(argv[3],std::ios::binary);
    const std::vector<uint8_t> clean{std::istreambuf_iterator<char>(fa),{}}, form{std::istreambuf_iterator<char>(fb),{}};
    const float tol=argc>=5?(float)std::atof(argv[4]):host::cosmetics::stage_safety::kFloorFitTolerance;
    std::vector<float> a,b;
    const bool ra=host::cosmetics::stage_safety::floor_fit::scores(clean,&a,tol), rb=host::cosmetics::stage_safety::floor_fit::scores(form,&b,tol);
    std::printf("original%s:",ra?"":" (unread)"); for(float x:a) std::printf(" %.2f",x);
    std::printf("\nform%s:",rb?"":" (unread)"); for(float x:b) std::printf(" %.2f",x);
    std::string why; const bool fits=host::cosmetics::stage_safety::floor_art_fits(clean,form,&why,tol);
    std::printf("\n%s%s%s\n",fits?"FITS":"REFUSED",fits?"":": ",why.c_str());
    return fits?0:3;
  }
  // <original> <skin>: the skin as installed, then its online form (restore_gameplay).
  // <original> --exporter-flags <out>: the original with CLASSICAL_SCALE (0x8) and the root draw bits
  // (0x30000000) cleared on every joint, as some exporters write a stage, for a check on a real file.
  if(argc==3 || argc==4) {
    std::ifstream a(argv[1],std::ios::binary);
    const std::vector<uint8_t> clean{std::istreambuf_iterator<char>(a),{}};
    std::vector<uint8_t> skin;
    if(argc==4) {
      if(std::string(argv[2])!="--exporter-flags") return 2;
      host::cosmetics::stage_safety::Archive parsed{clean};
      if(!parsed.parse()) { std::printf("FAIL: original does not parse\n"); return 1; }
      skin=clean;
      for(uint32_t j:parsed.joint_offsets) put(skin,32ull+j+4,parsed.get(j+4)&~0x30000008u);
      std::ofstream(argv[3],std::ios::binary).write((const char*)skin.data(),(std::streamsize)skin.size());
      std::printf("%zu joints flagged\n",parsed.joint_offsets.size());
    } else {
      std::ifstream b(argv[2],std::ios::binary);
      skin.assign(std::istreambuf_iterator<char>(b),{});
    }
    const bool raw=matches(clean,skin,&reason);
    std::printf("as installed: %s: %s\n",raw?"PASS":"FAIL",reason.c_str());
    if(raw) return 0;
    std::vector<uint8_t> online;
    const bool ok=host::cosmetics::stage_safety::restore_gameplay(clean,skin,&online,&reason);
    std::printf("online form: %s: %s\n",ok?"PASS":"FAIL",reason.c_str());
    if(ok) return 0;
    // Then drawn over the original, as the game tries next: from the repaired copy, then the skin,
    // with the models the skin's file name (its stage) allows whole.
    const auto whole=host::cosmetics::stage_safety::visual_only_models(argc==3?argv[2]:argv[1]);
    for(const std::vector<uint8_t>* base:{&online,&skin}) {
      if(base->empty()) continue;
      std::vector<uint8_t> out; std::string note;
      const bool grafted=host::cosmetics::stage_safety::graft_visuals(clean,*base,&out,&note,whole,
          host::cosmetics::stage_safety::same_motion_models(argc==3?argv[2]:argv[1]));
      std::printf("drawn over the original (%s): %s: %s\n",base==&online?"repaired":"skin",grafted?"PASS":"FAIL",note.c_str());
      if(grafted) return 0;
    }
    return 1;
  }
  int failed=0;
  const auto check=[&](bool ok,const char* name) { if(!ok) { std::fprintf(stderr,"FAIL %s: %s\n",name,reason.c_str()); ++failed; } };
  auto a=fixture(0,1), b=fixture(32,2);
  check(matches(a,b,&reason),"relocated visual mesh");
  {
    using namespace host::cosmetics::stage_safety;
    auto clean=fixture(0,1), skin=fixture(32,2);
    put(clean,32+96+4,0x70000008u);
    put(skin,32+128+4,0x50000000u);
    std::vector<uint8_t> out; std::string note;
    check(graft_visuals(clean,skin,&out,&note),"graft skin with different opaque/translucent masks");
    check(word(out.data()+32+96+4)==0x50000008u,"skin draw masks survive while original transform flags stay");
    check(matches(clean,out,&reason),"draw-mask repair preserves gameplay");
    pointer(skin,224+4,128);
    check(graft_visuals(clean,skin,&out,&note),"mesh binding to a paired stage joint is supported");
    const uint32_t mesh=word(out.data()+32+96+16);
    check(word(out.data()+32+mesh+4)==96,"skin mesh binds to the original gameplay joint");
    check(matches(clean,out,&reason),"remapped mesh binding preserves gameplay");
  }
  {
    using namespace host::cosmetics::stage_safety;
    auto clean=fixture(0,1), skin=fixture(32,2);
    pointer(clean,96+8,320); pointer(clean,320+16,192); put(clean,32+320+4,8);
    pointer(skin,128+8,352); pointer(skin,352+16,224);
    std::vector<uint8_t> out; std::string note;
    check(graft_visuals(clean,skin,&out,&note),"exporter flag differences still pair an unchanged child transform");
    check(word(out.data()+32+320+16)!=0,"paired child keeps its skin mesh");
    check(word(out.data()+32+320+4)==8,"paired child retains original transform flags");
    check(matches(clean,out,&reason),"flag-tolerant pairing preserves gameplay");
  }
  {
    // A rebuilt stage that moved a platform's art half a unit (Animelee CE's Battlefield: 34.0 -> 33.5):
    // still the same joint, drawn at the original's transform.
    using namespace host::cosmetics::stage_safety;
    auto clean=fixture(0,1), skin=fixture(32,2);
    pointer(clean,96+8,320); pointer(clean,320+16,192);
    pointer(skin,128+8,352); pointer(skin,352+16,224);
    put(clean,32+320+48,0x42080000u);   // translate y 34.0
    put(skin,32+352+48,0x42060000u);    // 33.5
    std::vector<uint8_t> out; std::string note;
    check(graft_visuals(clean,skin,&out,&note),"a joint moved within tolerance still pairs");
    check(word(out.data()+32+320+16)!=0,"the near joint shows the skin mesh");
    check(word(out.data()+32+320+48)==0x42080000u,"the near joint keeps the original transform");
    check(matches(clean,out,&reason),"near pairing preserves gameplay");
    put(skin,32+352+48,0x42480000u);    // 50.0: moved far, at the same place in a branch of the same shape
    // Paired by its place (a custom Onett lowered two groups by 7 units, 11-23): drawn at the original's
    // transform, so its art sits on the original's collision; the gameplay proof still has to pass.
    check(graft_visuals(clean,skin,&out,&note) && word(out.data()+32+320+16)!=0,"a joint moved far at the same place is paired");
    check(word(out.data()+32+320+48)==0x42080000u,"the far joint keeps the original transform");
    check(matches(clean,out,&reason),"place pairing preserves gameplay");
  }
  {
    // same_motion_models (Dream Land's Whispy, ImpX 11-23): a model whose code reads only its root and
    // its animations' end is the skin's whole when the root and every animation slot are the original's.
    // Then a child the skin moved (flowers sunk below the ground) stays where the skin put it.
    using namespace host::cosmetics::stage_safety;
    auto clean=fixture(0,1), skin=fixture(32,2);
    pointer(clean,96+8,320); pointer(clean,320+16,192);
    pointer(skin,128+8,352); pointer(skin,352+16,224);
    put(clean,32+320+48,0x42080000u);   // child translate y 34.0
    put(skin,32+352+48,0x42480000u);    // 50.0
    std::vector<uint8_t> out; std::string note;
    check(graft_visuals(clean,skin,&out,&note,{},{0}) && note.find("drawn whole")!=std::string::npos,"same root and motion: the model is the skin's whole");
    const uint32_t root=word(out.data()+32+48);
    check(root!=96 && word(out.data()+32+word(out.data()+32+root+8)+48)==0x42480000u,"the moved child keeps the skin's place");
    check(matches(clean,out,&reason,{0}),"whole same-motion model passes the gameplay proof");
    auto moved_root=skin; put(moved_root,32+128+48,0x3F800000u);   // the root itself moved: not whole
    check(graft_visuals(clean,moved_root,&out,&note,{},{0}) && note.find("drawn whole")==std::string::npos,"a moved root is not taken whole");
    check(word(out.data()+32+320+48)==0x42080000u,"then the child keeps the original's place");
    auto new_motion=skin; put(new_motion,32+80+48,1);   // a model entry value that differs: not whole
    check(graft_visuals(clean,new_motion,&out,&note,{},{0}) && note.find("drawn whole")==std::string::npos,"other animations are not taken whole");
  }
  {
    using namespace host::cosmetics::stage_safety;
    auto clean=fixture(0,1), skin=fixture(32,2);
    pointer(clean,40,320);   // bound the unused tail without adding a gameplay joint
    pointer(skin,128+8,352); pointer(skin,352+16,256);
    std::vector<uint8_t> out; std::string note;
    const bool grafted=graft_visuals(clean,skin,&out,&note);
    reason=note;
    check(!grafted,"a conversion missing skin geometry is refused");
    check(note.find("1 of the skin's 2 meshes")!=std::string::npos,"missing geometry is explained");
    check(out.empty(),"an incomplete skin cannot be served online");
  }
  {
    // Append one draw-only sibling without moving any original joint or gameplay object.
    auto scenery = a;
    const uint32_t size = 384, nr = 3;
    scenery.insert(scenery.begin() + 32 + size, 64, 0);
    scenery.insert(scenery.begin() + 32 + size + 64 + nr * 4, 8, 0);
    put(scenery, 0, (uint32_t)scenery.size()); put(scenery, 4, size + 64); put(scenery, 8, nr + 2);
    put(scenery, 32 + 96 + 12, size);
    put(scenery, 32 + size + 16, 192);
    put(scenery, 32 + size + 64 + nr * 4, 96 + 12);
    put(scenery, 32 + size + 64 + (nr + 1) * 4, size + 16);
    check(matches(a, scenery, &reason), "trailing scenery keeps original joint indices");
    auto dangerous = scenery; put(dangerous, 32 + size + 4, 0x4000);
    check(!matches(a, dangerous, &reason), "added spline is rejected");
    dangerous = scenery; put(dangerous, 32 + size + 16, 256);
    check(!matches(a, dangerous, &reason), "added scenery cannot reach collision data");
    dangerous = scenery; put(dangerous, 32 + size + 16, size);
    check(!matches(a, dangerous, &reason), "added scenery cannot reach itself");
    std::vector<uint8_t> grafted; std::string note;
    check(host::cosmetics::stage_safety::graft_visuals(a, scenery, &grafted, &note), "graft retains trailing scenery");
    check(matches(a, grafted, &reason), "grafted scenery retains gameplay proof");
  }
  auto changed=b; changed[32+256+32]=1;
  check(!matches(a,changed,&reason),"changed collision");
  changed=b; changed[32+288+32]=1;
  check(!matches(a,changed,&reason),"changed stage parameters");
  changed=b; changed[32+96+32+44]=1;
  check(!matches(a,changed,&reason),"changed platform transform");
  changed=b; put(changed,32+96+32+4,0x4000);
  check(!matches(a,changed,&reason),"spline behavior flag");
  changed=b; put(changed,32+96+32+4,0x10);
  check(!matches(a,changed,&reason),"hidden collision joint");
  // Stage exporters can change CLASSICAL_SCALE (0x8) on every joint. Online must
  // restore the original flag while keeping the skin's render mesh, rather than
  // either accepting changed transforms or discarding most meshes during grafting.
  {
    using host::cosmetics::stage_safety::restore_gameplay;
    auto exported = b;
    put(exported, 32 + 96 + 32 + 4, 0x30000008u);
    check(!matches(a, exported, &reason), "raw classical-scale change remains unsafe");
    std::vector<uint8_t> repaired;
    check(restore_gameplay(a, exported, &repaired, &reason), "restore exporter transform flag for online");
    check(matches(a, repaired, &reason), "repaired joint passes full gameplay comparison");
    check(repaired.size() == exported.size() && repaired[32 + 192 + 32] == 2,
          "repair retains the skin mesh and archive layout");
    check(host::cosmetics::stage_safety::word(repaired.data() + 32 + 96 + 32 + 4) == 0x30000000u,
          "repair retains visual flags and restores the original gameplay flags");
    auto moved = exported;
    moved[32 + 96 + 32 + 44] = 1;
    check(!restore_gameplay(a, moved, &repaired, &reason), "changed joint position still fails gameplay proof");
  }
  changed=b; put(changed,32+32+8,0xfffffffc);
  check(!matches(a,changed,&reason),"out of bounds pointer");
  changed=b; put(changed,16,1);
  check(!matches(a,changed,&reason),"external reference");
  changed=b; changed.resize(16);
  check(!matches(a,changed,&reason),"truncation");
  // A skin drawn over the original stage: the original's gameplay, the skin's mesh.
  {
    using host::cosmetics::stage_safety::graft_visuals;
    std::vector<uint8_t> out; std::string note;
    const bool grafted = graft_visuals(a, b, &out, &note);
    reason = note;
    check(grafted, "graft of a mesh-only skin");
    check(grafted && matches(a, out, &reason), "the grafted stage matches the original's gameplay");
    // The joint's mesh now points at the skin's mesh (its first byte is the skin's 2).
    const auto word = [&](size_t at) { return uint32_t(out[at]) << 24 | uint32_t(out[at+1]) << 16 | uint32_t(out[at+2]) << 8 | out[at+3]; };
    check(grafted && out.size() > a.size() && out[32 + word(32 + 96 + 16)] == 2, "the grafted joint draws the skin's mesh");
    auto moved = b; moved[32 + 256 + 32 + 0x10] = 1;   // a collision line range of the skin
    check(!graft_visuals(a, moved, &out, &note), "a skin that changes the collision is not grafted");
  }
  // Final Destination's models 4 to 9 belong to the match's background transformation (grlast.c), which
  // plays their animations by index and waits for each to end: only the backdrop (1, 2) is taken whole.
  check(host::cosmetics::stage_safety::visual_only_models("Sys/GrNLa.dat")==std::set<uint32_t>{1,2},
        "Final Destination takes only its backdrop whole");
  {
    // stage_floor_fit.h (CHARA 11-23: 20XX's tilting Battlefield online): art on the floor line fits;
    // art leaning across it, or not drawn at all, is refused; a short line is not judged.
    using namespace host::cosmetics::stage_safety;
    const auto level=floor_stage(0.0f,true), leaning=floor_stage(0.3f,true), hidden=floor_stage(0.0f,false);
    std::vector<float> scores;
    check(floor_fit::scores(level,&scores,kFloorFitTolerance) && scores.size()==1 && scores[0]>=0.95f,"level art scores on its line");
    std::string why;
    check(floor_art_fits(level,level,&why),"level art fits");
    check(!floor_art_fits(level,leaning,&why) && why.find("floor 0")!=std::string::npos,"art leaning across the floor is refused");
    check(!floor_art_fits(level,hidden,&why),"art not drawn (no pass bit) is refused");
    check(floor_art_fits(level,std::vector<uint8_t>{1,2,3},&why),"a form this cannot read is left to the other checks");
  }
  if(!failed) std::puts("stage gameplay safety tests passed");
  return failed?1:0;
}
