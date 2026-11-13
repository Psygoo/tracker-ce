from pathlib import Path
import hashlib,json,re,sys,difflib,subprocess
r=Path(sys.argv[1]);o=Path(sys.argv[2]);o.mkdir(parents=True,exist_ok=True);manifest=[]
def extract(path,sig,start=0):
 raw=(r/path).read_bytes();s=raw.decode().replace('\r\n','\n');a=s.index(sig,start);b=s.index('{',a);e=b+1;depth=1
 while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
 body=s[a:e];manifest.append(dict(path=path,line=s.count('\n',0,a)+1,signature=sig,source_sha256=hashlib.sha256(raw).hexdigest(),body_sha256=hashlib.sha256(body.encode()).hexdigest(),bytes=len(body.encode())))
 return body,e
note='core/AudioEngine/mtAudioNoteOnHandle.cpp';fx='core/AudioEngine/mtAudioSequencer.cpp';env='core/libs/Audio/effect_envelope.cpp'
chunks=[]
for path,sigs in [(note,['uint8_t playerEngine ::noteOn(','AUDIO_CORE_RAM uint8_t playerEngine ::noteOn(','AUDIO_CORE_RAM void playerEngine::noteOff(','AUDIO_CORE_RAM void playerEngine::noteOffFade(','AUDIO_CORE_RAM void playerEngine::noteOffCut(','AUDIO_CORE_RAM void playerEngine::noteOffOrdinary(']),(fx,['AUDIO_CORE_RAM void playerEngine::seqFx(','AUDIO_CORE_RAM void playerEngine::endFx(','void playerEngine::fxSampleSlice(','void playerEngine::endFxSlice(','void playerEngine::setFxSlice(','void playerEngine::clearFxSlice(','void playerEngine::fxVolumeLFO(','void playerEngine::endFxVolumeLFO(','void playerEngine::syncFxAmpLFO(']),(env,['AUDIO_CORE_FLASH void AudioPlayMemory::envelopeNoteOn(','AUDIO_CORE_FLASH void AudioPlayMemory::envelopeNoteOnForPrev(uint8_t','AUDIO_CORE_FLASH void AudioPlayMemory::envelopeNoteOff(','AUDIO_CORE_FLASH void AudioPlayMemory::envelopeSetIdle(','AUDIO_CORE_RAM void AudioPlayMemory::envelopeUpdate('])]:
 for sig in sigs:chunks.append(extract(path,sig)[0])
for path,sig in [(env,'AUDIO_CORE_RAM void AudioPlayMemory::envelopeSwitchPhase('),(env,'AUDIO_CORE_FLASH uint16_t AudioPlayMemory::envelopeMilliseconds2count('),(fx,'AUDIO_CORE_RAM uint8_t playerEngine::isFxVelocity('),('core/libs/Audio/play_memory_single_shot.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::playSingleShot('),('core/libs/Audio/play_memory_beat_slice.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::playBeatSlice('),('core/libs/Audio/play_memory.cpp','uint32_t AudioPlayMemory::calculatePointPositionAndSnapToZero('),('core/libs/Audio/play_memory.cpp','std::tuple<int16_t*, int16_t*, int16_t*> AudioPlayMemory::calculatePositionZoneAddresses('),('core/libs/Audio/play_memory.cpp','bool AudioPlayMemory::isSampleInZeroCrossing('),('core/libs/Audio/play_memory.cpp','void AudioPlayMemory::setForcedPoints('),('core/libs/Audio/play_memory.cpp','void AudioPlayMemory::setPointsForceFlag(')]:chunks.append(extract(path,sig)[0])
chunks.append(extract('core/libs/Audio/play_memory_loop_forward.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::playLoopForward(')[0])
for path,sig in [('core/libs/Audio/play_memory_slice.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::playSlice('),('core/libs/Audio/play_memory_wavetable.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::playWavetable('),('core/libs/Audio/play_memory_granular.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::playGranular('),('core/libs/Audio/play_memory.cpp','std::tuple<uint16_t, uint16_t, uint16_t, uint16_t> AudioPlayMemory::calculateGranularPoints(')]:chunks.append(extract(path,sig)[0])
for path,sig in [(fx,'void playerEngine::clearFxAmpRateLFO('),(env,'AUDIO_CORE_FLASH void AudioPlayMemory::envelopeSetSustain('),(env,'AUDIO_CORE_FLASH uint8_t AudioPlayMemory::envelopeGetState(')]:chunks.append(extract(path,sig)[0])
# Rename declarations only to put explicit fixture seams around unchanged bodies.
replacements={'void playerEngine::syncFxAmpLFO()':'void playerEngine::syncFxAmpLFOBody()','void AudioPlayMemory::envelopeNoteOn(':'void AudioPlayMemory::envelopeNoteOnBody(','void AudioPlayMemory::envelopeNoteOff(':'void AudioPlayMemory::envelopeNoteOffBody(','uint32_t AudioPlayMemory::calculatePointPositionAndSnapToZero(':'uint32_t AudioPlayMemory::snapNativeBody('}
generated='\n\n'.join(('#ifdef A1_REAL_AMP\n'+body+'\n#endif') if idx>=35 else body for idx,body in enumerate(chunks))
(o/'native-unmodified.inc').write_text(generated,encoding='utf-8')
for a,b in replacements.items():
 assert generated.count(a)==1,(a,generated.count(a));generated=generated.replace(a,b)
# Baseline comes from the specified original CE commit, not the WR2 seams or
# even the later native robustness patch. Valid-input scenario is identical.
baseline_commit='998b06d40bd0d99de9ce92874bf2732298ca3fa7'
original=[];original_manifest=[];cache={}
for entry in manifest:
 path=entry['path'];sig=entry['signature']
 if path not in cache:cache[path]=subprocess.run(['git','-C',str(r),'show',baseline_commit+':'+path],check=True,capture_output=True).stdout.decode().replace('\r\n','\n')
 source=cache[path];a=source.index(sig);b=source.index('{',a);e=b+1;depth=1
 while depth:depth+=(source[e]=='{')-(source[e]=='}');e+=1
 body=source[a:e];original.append(body);original_manifest.append(dict(commit=baseline_commit,path=path,signature=sig,body_sha256=hashlib.sha256(body.encode()).hexdigest()))
baseline='\n\n'.join(('#ifdef A1_REAL_AMP\n'+body+'\n#endif') if idx>=35 else body for idx,body in enumerate(original))
for a,b in replacements.items():baseline=baseline.replace(a,b)
(o/'native-baseline.inc').write_text(baseline,encoding='utf-8')
(o/'native-original-manifest.json').write_text(json.dumps(original_manifest,indent=2),encoding='utf-8')
# Candidate-only outer FX preparation.
generated=generated.replace('void playerEngine::seqFx(', 'void playerEngine::seqFxBody(',1)
# Deliberately test the proposed 'only fence the stereo loop' placement.
for noteBody in chunks[:2]:
 p=noteBody.index('for (int channel = 0;',noteBody.index('bool isAmpRandom'))
 replacement=noteBody[:p]+'std::optional<wr2minimal::StereoFence<Registers>> pairFence;\n    if (fenceAtPair) pairFence.emplace();\n    '+noteBody[p:]
 assert noteBody in generated;generated=generated.replace(noteBody,replacement,1)
# Plain old-FX cleanup belongs to the original native domain, before WR2 scope.
prefix='    endFx(lastSeqFx[0], 0);\n    endFx(lastSeqFx[1], 1);'
assert generated.count(prefix)==1
generated=generated.replace(prefix,'    if (!(nativeDomains && current && current->preclosed)) {\n'+prefix+'\n    }',1)
# Two local prepared-points seams and metadata commit before native IRQ enable.
for original,startName,endName in [(chunks[23],'startPoint','endPoint'),(chunks[24],'startSlice','endSlice')]:
 changed=original.replace('if (samplePointsSource != nullptr)',f'if (applyPrepared({startName}, {endName})) {{}}\n    else if (samplePointsSource != nullptr)',1)
 changed=changed.replace('    playing = 1;',f'    commitNativeGeometry(instrIdx, {startName}, {endName});\n    playing = 1;',1)
 assert original in generated;generated=generated.replace(original,changed,1)
# Event-local slice seam; never save/set/restore live S2 fields.
start=generated.index('const uint8_t nativeSlice =' if 'const uint8_t nativeSlice =' in generated else 'const uint8_t activeSlice =');end=generated.index(';',start)
expr=generated[start:end];decl,rhs=expr.split('=',1);generated=generated[:start]+decl+'= fixtureUseSelection ? fixtureSelection.slice : '+rhs+generated[end:]
def instrument(text):
 sig='uint32_t AudioPlayMemory::snapNativeBody('
 at=text.index('{',text.index(sig))+1
 text=text[:at]+'\n    ++snapTotal; if(!Registers::nvic) ++snapOutputMasked; if(Registers::mask) ++snapBasepriMasked;\n'+text[at:]
 sig='bool AudioPlayMemory::isSampleInZeroCrossing('
 at=text.index('{',text.index(sig))+1
 return text[:at]+'\n    ++pcmVisits; if(!Registers::nvic) ++pcmOutputMasked; if(Registers::mask) ++pcmBasepriMasked;\n'+text[at:]
baseline=(o/'native-baseline.inc').read_text(encoding='utf-8')
(o/'native-baseline.inc').write_text(instrument(baseline),encoding='utf-8')
(o/'native.inc').write_text(instrument(generated),encoding='utf-8')
(o/'candidate-seams.patch').write_text(''.join(difflib.unified_diff('\n\n'.join(chunks).splitlines(True),generated.splitlines(True),fromfile='native-unmodified.inc',tofile='native.inc')),encoding='utf-8')
(o/'fixture-seams.json').write_text(json.dumps(dict(definition_renames=list(replacements.items()),note_on_insertions='optional late-pair fence, disabled except placement experiment',beat_slice_change='event-local activeSlice ternary; live S2 fields are never overwritten',geometry_seams='prepared points and commit before native IRQ enable in Single Shot and Beat Slice',pcm_model='fixed candidate executes extracted native snap/zone/zero-crossing against host PCM; legacy negative tests retain arithmetic mock; no DMA/streaming or timing claim',other_fx_stubs='fx-stubs.inc',headers={p:hashlib.sha256((r/p).read_bytes()).hexdigest()for p in ['core/libs/Audio/play_memory.h','core/source/mtSequencer.h']}),indent=2),encoding='utf-8')
s=(r/'core/source/mtSequencer.h').read_text(encoding='utf-8');a=s.index('enum enFxType');b=s.index('{',a);e=s.index('}',b)
enum=re.sub(r'//[^\n]*','',s[b+1:e]);(o/'fx-enum.inc').write_text('enum enFxType {'+enum+'};',encoding='utf-8')
# Generate inert DSP endpoints for the control dispatcher, but NEVER S2/LFO.
dispatch=chunks[6]+chunks[7];special={'fxSampleSlice','endFxSlice','fxVolumeLFO','endFxVolumeLFO'}
stubs=[]
for method,args in re.findall(r'\b((?:fx|endFx)[A-Z]\w*)\(([^()]*)\)',dispatch):
 if method in special:continue
 count=args.count(',')+1 if args.strip() else 0
 signature='void '+method+'('+','.join('int' for _ in range(count))+'){++stubCalls;}'
 if signature not in stubs:stubs.append(signature)
(o/'fx-stubs.inc').write_text('\n'.join(stubs),encoding='utf-8')
s=(r/'core/libs/Audio/play_memory.h').read_text(encoding='utf-8');a=s.index('struct strEnvelopeData');b=s.index('{',a);e=s.index('} envelope;',b)
(o/'envelope-fields.inc').write_text(s[a:e+1]+' envelope{};',encoding='utf-8')
(o/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('Extracted',len(chunks),'COMPLETE functions;',sum(x['bytes'] for x in manifest),'bytes. DSP/other FX endpoints remain explicit stubs.')
