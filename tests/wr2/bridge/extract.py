from pathlib import Path
import hashlib,json,sys
repo=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
p=repo/'core/AudioEngine/mtAudioSequencer.cpp';raw=p.read_bytes();s=raw.decode().replace('\r\n','\n')
a=s.index('void playerEngine::syncFxAmpLFO()');b=s.index('{',a);e=b+1;depth=1
while depth:
    depth+=(s[e]=='{')-(s[e]=='}');e+=1
body=s[a:e];(out/'sync-native.inc').write_text(body,encoding='utf-8')
(out/'native-source.json').write_text(json.dumps(dict(path=str(p),line=s.count('\n',0,a)+1,source_sha256=hashlib.sha256(raw).hexdigest(),function_sha256=hashlib.sha256(body.encode()).hexdigest(),scope='Actual sync body; mocked envelope methods; no NVIC/DSP'),indent=2))
