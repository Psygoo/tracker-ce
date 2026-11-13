static void ptiFiles(MemoryIO&io,const MemoryModel&model,const char*folder){
 for(unsigned i=0;i<48;++i){std::string p=std::string(folder)+"/instruments/"+std::to_string(i+1)+" "+model.slots[i].name+".pti";auto&b=io.files[p];b.resize(257+i);for(unsigned j=0;j<b.size();++j)b[j]=static_cast<uint8_t>(i+j*17);}
}
static void savePortable(MemoryIO&io,MemoryModel&model,const char*folder){Storage storage{};Service s(io,model,storage);CHECK(s.beginSave(folder));CHECK(s.commitSave(exportProject));}
static bool importPortable(MemoryIO&io,MemoryModel&model,unsigned destination,const char*pti){Storage storage{};return importInstrument(io,model,storage,destination,pti);}
static void companionTests(){
 // Python hashlib oracles, including padding/block boundaries and long input.
 struct Vector{unsigned n;const char*hex;};
 const Vector vectors[]={
#include "sha-vectors.inc"
 };
 for(const auto&v:vectors){std::vector<uint8_t> data(v.n);for(unsigned i=0;i<v.n;++i)data[i]=static_cast<uint8_t>(i*17+3);uint8_t digest[32];wr2_sha256_t h;wr2_sha256_init(&h);for(unsigned i=0;i<data.size();i+=13)CHECK(wr2_sha256_update(&h,data.data()+i,std::min<unsigned>(13,static_cast<unsigned>(data.size())-i)));wr2_sha256_final(&h,digest);char text[65]{};for(unsigned i=0;i<32;++i)std::snprintf(text+i*2,3,"%02x",digest[i]);CHECK(!std::strcmp(text,v.hex));}
 MemoryModel source;source.populate();MemoryIO base;ptiFiles(base,source,"P");savePortable(base,source,"P");
 for(auto it=base.files.begin();it!=base.files.end();){if(it->first.size()>=8&&it->first.substr(it->first.size()-8)==".pti.w2i")it=base.files.erase(it);else ++it;}
 for(unsigned i=0;i<48;++i){
  const unsigned dst=(i+7)%48;MemoryModel target;target.populate();target.slots[dst]=source.slots[i];
  const std::string pti="P/instruments/"+std::to_string(i+1)+" "+source.slots[i].name+".pti";
  CHECK(importPortable(base,target,dst,pti.c_str()));CHECK(!std::memcmp(&target.rows[dst],&source.rows[i],sizeof(InstrumentState)));
 }
 const char*pti="P/instruments/1 sample-0.wav.pti";const char*a="P/wr2_companions/instrument_01.a.w2i",*b="P/wr2_companions/instrument_01.b.w2i";
 auto loadCase=[&](MemoryIO io,bool good){MemoryModel target;target.populate();target.slots[7]=source.slots[0];CHECK(importPortable(io,target,7,pti)==good);if(good)CHECK(!std::memcmp(&target.rows[7],&source.rows[0],sizeof(InstrumentState)));else CHECK(target.rows[7].sliceCount==0);};
 {auto io=base;io.files.erase(a);loadCase(io,true);io.files.erase(b);loadCase(io,false);}
 for(unsigned i=0;i<288;++i){auto io=base;io.files[a][i]^=0xff;loadCase(io,true);io.files[b][i]^=0xff;loadCase(io,false);}
 {auto io=base;io.files[a].pop_back();loadCase(io,true);}
 for(const char*guard:{"P/wr2_groups.wr2.pending","P/wr2_groups.wr2.tmp","P/wr2_companions/instrument_01.a.w2i.tmp","P/wr2_companions/instrument_01.b.w2i.tmp"}){auto io=base;io.files[guard]={0};loadCase(io,false);}
 {auto io=base;io.files[pti][200]^=1;loadCase(io,false);io=base;io.files["P/wr2_groups.wr2"][100]^=1;loadCase(io,false);}
 {auto io=base;io.files["P/wr2_groups.wr2"][32+56]=99;repair(io.files["P/wr2_groups.wr2"]);loadCase(io,false);}
 // Malformed fields remain invalid even with a freshly recomputed companion CRC.
 for(unsigned pos:{0u,7u,8u,9u,10u,11u,12u,16u,20u,52u,84u,171u,172u,175u,176u,180u,279u,280u,283u}){
  auto io=base;for(const char*name:{a,b}){auto&c=io.files[name];c[pos]^=1;const auto sum=crc(c.data(),284)^0xffffffffu;for(unsigned j=0;j<4;++j)c[284+j]=static_cast<uint8_t>(sum>>(8*j));}loadCase(io,false);
 }
 // Positive end-to-end transfer: import -> target Save/Save As -> cold Open -> re-export/import.
 {auto io=base;MemoryModel dest;dest.slots[7]=source.slots[0];CHECK(importPortable(io,dest,7,pti));ptiFiles(io,dest,"Target");savePortable(io,dest,"Target");const auto oldRoot=io.files["Target/wr2_groups.wr2"];
  ptiFiles(io,dest,"TargetCopy");savePortable(io,dest,"TargetCopy");CHECK(io.files["Target/wr2_groups.wr2"]==oldRoot);MemoryModel cold;cold.slots[7]=dest.slots[7];CHECK(load(io,cold,"TargetCopy")==wr2::persist::Result::Loaded&&!std::memcmp(&cold.rows[7],&source.rows[0],sizeof(InstrumentState)));
  MemoryModel next;next.slots[20]=source.slots[0];CHECK(importPortable(io,next,20,"TargetCopy/instruments/8 sample-0.wav.pti")&&!std::memcmp(&next.rows[20],&source.rows[0],sizeof(InstrumentState)));
 }
 // Root generation changes even when the source row did not change.
 {auto io=base;MemoryModel newer=source;setWeight(newer.rows[2],2,2,91);save(io,newer,"P");loadCase(io,false);}
 {MemoryModel target;target.slots[7]=source.slots[0];++target.slots[7].cuts[3];CHECK(!importPortable(base,target,7,pti)&&target.rows[7].sliceCount==0);}
 {MemoryModel target;target.slots[7]=source.slots[0];target.slots[7].channels=2;CHECK(!importPortable(base,target,7,pti));}
 {auto io=base;auto renamed=io.files[pti];io.files["P/instruments/1 renamed.pti"]=renamed;MemoryModel target;target.slots[7]=source.slots[0];CHECK(!importPortable(io,target,7,"P/instruments/1 renamed.pti"));}
 {MemoryModel target;target.slots[7]=source.slots[0];target.reject=7;CHECK(!importPortable(base,target,7,pti)&&target.rows[7].sliceCount==0);}
 // Every read/stat/digest error must reject metadata, preserving native PTI.
 auto count=base;count.reset();MemoryModel target;target.slots[7]=source.slots[0];CHECK(importPortable(count,target,7,pti));
 for(unsigned n=1;n<=count.operations;++n){auto io=base;io.reset();io.fault=static_cast<int>(n);loadCase(io,false);}
 // Copy is independent, empty source clears destination, mismatched topology rejects.
 {MemoryModel m=source;m.slots[7]=m.slots[0];CHECK(copyInstrument(m,0,7));const auto before=m.rows[0];setWeight(m.rows[7],0,0,77);CHECK(!std::memcmp(&before,&m.rows[0],sizeof(before)));m.rows[0].reset(48);CHECK(copyInstrument(m,0,7)&&m.rows[7].sliceCount==0);m=source;m.slots[7]=m.slots[0];++m.slots[7].count;CHECK(!copyInstrument(m,0,7)&&m.rows[7].sliceCount==0);CHECK(!copyInstrument(m,48,0));CHECK(copyInstrument(m,0,0));m=source;m.slots[7]=m.slots[0];m.slots[0].frames=m.slots[7].frames=0;CHECK(!copyInstrument(m,0,7)&&m.rows[7].sliceCount==0);}
 // Save with derived caches: fault/cut every boundary on one grouped instrument.
 MemoryModel one=source;for(unsigned i=1;i<48;++i)one.rows[i].reset(0);MemoryIO prior;ptiFiles(prior,one,"P");savePortable(prior,one,"P");MemoryModel changed=one;setWeight(changed.rows[0],0,0,71);
 auto countSave=prior;countSave.reset();savePortable(countSave,changed,"P");const unsigned total=countSave.operations;
 for(unsigned n=1;n<=total;++n)for(unsigned mode=0;mode<3;++mode){
  auto io=prior;io.reset();if(!mode)io.fault=static_cast<int>(n);else{io.cut=static_cast<int>(n);io.after=mode==2;}
  Storage storage{};Service s(io,changed,storage);bool complete=false;try{complete=s.beginSave("P")&&s.commitSave(exportProject);}catch(const Crash&){}
  if(!mode)CHECK(!complete);
  CHECK(changed.rows[0].weight[0]==71);io.reset();MemoryModel result;result.slots[7]=one.slots[0];
  if(importPortable(io,result,7,pti))CHECK(result.rows[7].weight[0]==one.rows[0].weight[0]||result.rows[7].weight[0]==71);else CHECK(result.rows[7].sliceCount==0);
 }
 // Workspace replacement mask never authorizes groups, and survives dirty restarts.
 {auto io=base;CHECK(readReplacements(io)==0);CHECK(markReplacement(io,0));CHECK(markReplacement(io,47));CHECK(readReplacements(io)==((uint64_t(1)<<47)|1));
  MemoryModel boot;boot.populate();++boot.slots[0].frames;Storage bfr{};Service bootService(io,boot,bfr);CHECK(bootService.stageOpen("P"));CHECK(bootService.finishOpen(readReplacements(io))==wr2::persist::Result::Loaded);CHECK(boot.rows[0].sliceCount==0&&boot.rows[47].sliceCount==0&&boot.rows[1].owner[1]==1);
  io.files["Workspace/wr2_replaced.mask"][20]=1;CHECK(readReplacements(io)==((uint64_t(1)<<48)-1));
  CHECK(bootService.stageOpen("P"));CHECK(bootService.finishOpen(readReplacements(io))==wr2::persist::Result::Loaded&&boot.empty());
  CHECK(clearReplacements(io)&&readReplacements(io)==0);CHECK(!markReplacement(io,48));
 }
 for(int fail=1;fail<=2;++fail){auto io=base;CHECK(markReplacement(io,0));io.reset();io.fault=fail;CHECK(!clearReplacements(io));}
 for(int n=1;n<=4;++n){auto io=base;CHECK(markReplacement(io,0));io.reset();io.fault=n;const bool marked=markReplacement(io,47);if(marked){io.reset();CHECK(readReplacements(io)&(uint64_t(1)<<47));}}
 std::printf("PASS companions/lifecycle: all 48 slots, SHA vectors, A/B corruption, missing/stale/PCM/identity, copy independence, %u derived-Save boundaries with errors/cuts\n",total);
}
