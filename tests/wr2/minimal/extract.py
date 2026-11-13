from pathlib import Path
import hashlib,json,sys
r=Path(sys.argv[1]);o=Path(sys.argv[2]);o.mkdir(parents=True,exist_ok=True);manifest=[]
def function(path,sig):
 raw=(r/path).read_bytes();s=raw.decode().replace('\r\n','\n');a=s.index(sig);b=s.index('{',a);e=b+1;depth=1
 while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
 body=s[a:e];manifest.append(dict(path=path,line=s.count('\n',0,a)+1,source_sha256=hashlib.sha256(raw).hexdigest(),body_sha256=hashlib.sha256(body.encode()).hexdigest()))
 return body
(o/'envelope-native.inc').write_text(function('core/libs/Audio/effect_envelope.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::envelopeNoteOn('))
note=function('core/AudioEngine/mtAudioNoteOnHandle.cpp','uint8_t playerEngine ::noteOn(')
a=note.index('for (int channel = 0;');b=note.index('{',a);e=b+1;depth=1
while depth:depth+=(note[e]=='{')-(note[e]=='}');e+=1
(o/'stereo-native.inc').write_text(note[a:e])
p=r/'platforms/imx/source/interruptsControl.cpp';raw=p.read_bytes()
(o/'irq-native.inc').write_text('\n'.join(x for x in raw.decode().splitlines() if not x.startswith('#include')))
manifest.append(dict(path=str(p),source_sha256=hashlib.sha256(raw).hexdigest()))
(o/'zone-native.inc').write_text(function('core/libs/Audio/play_memory.cpp','std::tuple<int16_t*, int16_t*, int16_t*> AudioPlayMemory::calculatePositionZoneAddresses('))
(o/'extraction.json').write_text(json.dumps(manifest,indent=2))
