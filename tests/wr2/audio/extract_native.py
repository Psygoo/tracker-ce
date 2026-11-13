"""Extract exact native function bodies for the host fixture, without editing CE.
HAL/DSP/arrays are fixture replacements; this is NOT a native firmware emulator.
"""
import hashlib, json, pathlib, sys
repo=pathlib.Path(sys.argv[1]); out=pathlib.Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
functions=[
 ('core/libs/Audio/play_memory_beat_slice.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::playBeatSlice('),
 ('core/libs/Audio/play_memory.cpp','void AudioPlayMemory::setForcedSlice('),
 ('core/libs/Audio/effect_envelope.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::envelopeNoteOn('),
 ('core/AudioEngine/mtAudioSequencer.cpp','void playerEngine::fxSampleSlice('),
 ('core/AudioEngine/mtAudioSequencer.cpp','void playerEngine::endFxSlice('),
]
text=[];manifest=[]
for rel,signature in functions:
    raw=(repo/rel).read_bytes();src=raw.decode('utf-8');start=src.index(signature)
    brace=src.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(src[end]=='{')-(src[end]=='}');end+=1
    body=src[start:end];line=src.count('\n',0,start)+1
    text.append(f'// EXACT EXTRACT: {rel}:{line}\n{body}\n')
    manifest.append(dict(path=rel,line=line,signature=signature,source_sha256=hashlib.sha256(raw).hexdigest(),function_sha256=hashlib.sha256(body.encode()).hexdigest()))
# Preserve both native resume call sites, without DSP loop/timing surroundings.
env=(repo/'core/libs/Audio/effect_envelope.cpp').read_text(encoding='utf-8')
position=0
for variant in (1,2):
    start=env.index('if (stackedPlay.isPrev)',position)
    tail='stackedPlay.enable = false;'
    end=env.index(tail,start)+len(tail)
    snippet=env[start:end];position=end
    (out/f'native_resume_{variant}.inc').write_text(snippet,encoding='utf-8')
    manifest.append(dict(path='core/libs/Audio/effect_envelope.cpp',line=env.count('\n',0,start)+1,function_sha256=hashlib.sha256(snippet.encode()).hexdigest(),role=f'resume-{variant}'))
(out/'native_functions.inc').write_text('\n'.join(text),encoding='utf-8')
(out/'native-extraction.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')