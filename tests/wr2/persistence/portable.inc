static void repairCompanion(std::vector<uint8_t>&c){const auto sum=crc(c.data(),284)^0xffffffffu;for(unsigned i=0;i<4;++i)c[284+i]=static_cast<uint8_t>(sum>>(8*i));}
static void portableTests(){
 MemoryModel source;source.populate();MemoryIO exported;ptiFiles(exported,source,"Original");savePortable(exported,source,"Original");
 auto emptyDestination=[&](){MemoryModel m;m.populate();m.slots[17]=source.slots[0];return m;};
 const char*original="Original/instruments/1 sample-0.wav.pti";
 const auto bytes=exported.files[original];const auto companion=exported.files[std::string(original)+".w2i"];
 CHECK(companion.size()==288&&companion[8]==2);
 CHECK(companion==exported.files["Original/wr2_companions/instrument_01.a.w2i"]&&companion==exported.files["Original/wr2_companions/instrument_01.b.w2i"]);
 auto pair=[&](const std::string&path){MemoryIO io;io.files[path]=bytes;io.files[path+".w2i"]=companion;return io;};
 auto check=[&](MemoryIO io,const char*path,bool good){auto target=emptyDestination();CHECK(importPortable(io,target,17,path)==good);if(good)CHECK(!std::memcmp(&target.rows[17],&source.rows[0],sizeof(InstrumentState)));else CHECK(target.rows[17].sliceCount==0);};
 const char*renamed="Card_B/Library/Completely renamed.pti";auto independent=pair(renamed);
 check(independent,renamed,true);CHECK(independent.files.size()==2);
 check(pair("Root.pti"),"Root.pti",true);
 // All source slots transfer by content to arbitrary destination slots and names.
 for(unsigned i=0;i<48;++i){
  const auto src="Original/instruments/"+std::to_string(i+1)+" "+source.slots[i].name+".pti";
  const auto dst="OtherCard/Favorites/Renamed_"+std::to_string(48-i)+".pti";MemoryIO io;io.files[dst]=exported.files[src];io.files[dst+".w2i"]=exported.files[src+".w2i"];
  MemoryModel target;target.populate();const unsigned slot=(i*11+17)%48;target.slots[slot]=source.slots[i];CHECK(importPortable(io,target,slot,dst.c_str()));CHECK(!std::memcmp(&target.rows[slot],&source.rows[i],sizeof(InstrumentState)));
 }
 // This is an actual WR2LIFE-format A cache, copied/renamed without its root.
 {auto io=pair(renamed);io.files[std::string(renamed)+".w2i"]=exported.files["Original/wr2_companions/instrument_01.a.w2i"];check(io,renamed,true);}
 // Root content and original prefix/leaf are not standalone dependencies.
 {const char*p="DifferentProject/instruments/48 New name.pti";auto io=pair(p);io.files["DifferentProject/wr2_groups.wr2"]={1,2,3};check(io,p,true);io.files["DifferentProject/wr2_groups.wr2"]=exported.files["Original/wr2_groups.wr2"];check(io,p,true);io.files["DifferentProject/wr2_groups.wr2.pending"]={0};check(io,p,false);io.files.erase("DifferentProject/wr2_groups.wr2.pending");io.files["DifferentProject/wr2_groups.wr2.tmp"]={0};check(io,p,false);}
 {auto io=independent;auto&c=io.files[std::string(renamed)+".w2i"];c[20]^=1;repairCompanion(c);check(io,renamed,true);c[10]=47;c[178]=47;repairCompanion(c);check(io,renamed,true);}
 // Explicit bad metadata never falls back to the valid legacy A/B groups.
 {auto io=exported;io.files[std::string(original)+".w2i"][200]^=1;check(io,original,false);io.files.erase(std::string(original)+".w2i");check(io,original,true);}
 {auto io=independent;io.files.erase(std::string(renamed)+".w2i");check(io,renamed,false);}
 {auto io=independent;io.files[std::string(renamed)+".w2i.tmp"]={0};check(io,renamed,false);}
 {auto io=independent;io.files[renamed][100]^=1;check(io,renamed,false);io=independent;io.files[renamed].push_back(0);check(io,renamed,false);}
 {auto io=independent;io.files[std::string(renamed)+".w2i"]=exported.files["Original/instruments/2 sample-1.wav.pti.w2i"];check(io,renamed,false);}
 for(unsigned i=0;i<288;++i){auto io=independent;io.files[std::string(renamed)+".w2i"][i]^=1;check(io,renamed,false);}
 for(unsigned pos:{0u,7u,8u,9u,11u,12u,16u,52u,172u,175u,176u,177u,178u,179u,180u,184u,232u,280u,283u}){auto io=independent;auto&c=io.files[std::string(renamed)+".w2i"];c[pos]^=0xff;repairCompanion(c);check(io,renamed,false);}
 {auto io=independent;auto&c=io.files[std::string(renamed)+".w2i"];c[10]=48;c[178]=48;repairCompanion(c);check(io,renamed,false);}
 {auto io=independent;auto&c=io.files[std::string(renamed)+".w2i"];std::memset(c.data()+84,'x',88);repairCompanion(c);check(io,renamed,false);}
 for(int delta:{-1,1}){auto io=independent;auto&c=io.files[std::string(renamed)+".w2i"];c.resize(static_cast<std::size_t>(288+delta));check(io,renamed,false);}
 // The loaded PTI must still agree with the record, including conversion cases.
 for(unsigned field=0;field<6;++field){auto m=emptyDestination();auto&s=m.slots[17];if(field==0)++s.frames;if(field==1)++s.count;if(field==2)s.channels=2;if(field==3)++s.cuts[3];if(field==4)++s.type;if(field==5)s.name[0]='Z';CHECK(!importPortable(independent,m,17,renamed)&&m.rows[17].sliceCount==0);}
 {auto io=independent;io.files["Card_B/Library/Only PTI renamed.pti"]=io.files[renamed];io.files.erase(renamed);check(io,"Card_B/Library/Only PTI renamed.pti",false);}
 // Every standalone stat/read/digest failure clears old destination metadata.
 auto count=independent;count.reset();auto target=emptyDestination();CHECK(importPortable(count,target,17,renamed));
 for(unsigned i=1;i<=count.operations;++i){auto io=independent;io.reset();io.fault=static_cast<int>(i);check(io,renamed,false);}
 // Removing groups then saving removes portable files, including padded names.
 {auto io=exported;MemoryModel removed=source;removed.rows[0].reset(48);io.files["Original/instruments/01 sample-0.wav.pti.w2i"]=companion;io.files[std::string(original)+".w2i.tmp"]={0};savePortable(io,removed,"Original");CHECK(!io.files.count(std::string(original)+".w2i")&&!io.files.count(std::string(original)+".w2i.tmp")&&!io.files.count("Original/instruments/01 sample-0.wav.pti.w2i"));check(io,original,false);}
 // Existing zero-padded native PTI filenames also receive portable companions.
 {auto io=exported;const char*p="Original/instruments/01 sample-0.wav.pti";io.files[p]=io.files[original];io.files.erase(original);savePortable(io,source,"Original");CHECK(io.files.count(std::string(p)+".w2i"));check(io,p,true);}
 // Portable -> destination Save/Save As -> boot -> portable again, without source.
 {auto io=independent;auto dest=emptyDestination();dest.clear();CHECK(importPortable(io,dest,17,renamed));ptiFiles(io,dest,"Target");savePortable(io,dest,"Target");ptiFiles(io,dest,"TargetCopy");savePortable(io,dest,"TargetCopy");MemoryModel boot;boot.slots[17]=dest.slots[17];CHECK(load(io,boot,"TargetCopy")==wr2::persist::Result::Loaded&&!std::memcmp(&boot.rows[17],&source.rows[0],sizeof(InstrumentState)));
  const char*p="TargetCopy/instruments/18 sample-0.wav.pti";MemoryIO newCard;newCard.files["Card_C/Again.pti"]=io.files[p];newCard.files["Card_C/Again.pti.w2i"]=io.files[std::string(p)+".w2i"];check(newCard,"Card_C/Again.pti",true);
 }
 std::printf("PASS independent portability: two-file CE v2, 48 slots, renamed/moved/cards, root absent/corrupt, old v2 A/B migration by copy, explicit priority, invalid metadata, %u IO fault boundaries, cleanup, SaveAs/boot/re-export\n",count.operations);
}
