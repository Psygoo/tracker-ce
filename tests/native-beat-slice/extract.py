import hashlib,json,pathlib,subprocess,sys
repo=pathlib.Path(sys.argv[1]);out=pathlib.Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
functions=[('core/libs/Audio/play_memory_beat_slice.cpp','AUDIO_CORE_FLASH void AudioPlayMemory::playBeatSlice('),('core/libs/Audio/play_memory.cpp','AUDIO_CORE_FLASH uint8_t AudioPlayMemory::play('),('core/libs/Audio/play_memory.cpp','void AudioPlayMemory::setForcedSlice('),('core/AudioEngine/mtAudioSequencer.cpp','void playerEngine::fxSampleSlice('),('core/AudioEngine/mtAudioSequencer.cpp','void playerEngine::endFxSlice(')]
manifest=[]
for variant in ['baseline','current']:
    chunks=[]
    for path,sig in functions:
        raw=subprocess.check_output(['git','show','998b06d40bd0d99de9ce92874bf2732298ca3fa7:'+path],cwd=repo) if variant=='baseline' else (repo/path).read_bytes()
        s=raw.decode().replace('\r\n','\n');start=s.index(sig);b=s.index('{',start);end=b+1;depth=1
        while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
        body=s[start:end];chunks.append(body)
        manifest.append(dict(variant=variant,path=path,line=s.count('\n',0,start)+1,source_sha256=hashlib.sha256(raw).hexdigest(),function_sha256=hashlib.sha256(body.encode()).hexdigest()))
    # These two guards are tested only up to the first IRQ call, not the full
    # envelope/FX implementation. Main methods above are extracted in full.
    path='core/AudioEngine/mtAudioNoteOnHandle.cpp'
    raw=subprocess.check_output(['git','show','998b06d40bd0d99de9ce92874bf2732298ca3fa7:'+path],cwd=repo) if variant=='baseline' else (repo/path).read_bytes()
    s=raw.decode().replace('\r\n','\n');position=0
    for i in range(2):
        start=s.index('playerEngine ::noteOn(',position);brace=s.index('{',start);end=s.index('    AudioNoInterrupts();',brace);prefix=s[brace+1:end];position=end+1
        chunks.append(f'uint8_t playerEngine::noteBoundary{i}(uint8_t instr_idx) {{'+prefix+'\nreturn 1;\n}\n')
        manifest.append(dict(variant=variant,path=path,role=f'note-entry-prefix-{i}',prefix_sha256=hashlib.sha256(prefix.encode()).hexdigest()))
    (out/f'{variant}.inc').write_text('\n'.join(chunks),encoding='utf-8')
(out/'extraction.json').write_text(json.dumps(manifest,indent=2))