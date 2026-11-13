from pathlib import Path
import json,re,hashlib,subprocess,sys
r=Path(__file__).resolve().parents[3]
o=Path(sys.argv[1]) if len(sys.argv)>1 else r/'build/wr2-a1-tests/fx-diff'
def read_rows(name):
    rows={}
    for line in (o/name).read_text(encoding='utf-8-sig').splitlines():
        if line.startswith('DIFFERENTIAL '):
            mode=line.split()[1]
            data={k:int(v) if v.isdigit() else v for k,v in re.findall(r'([\w-]+)=([^ ]+)',line)}
            rows[(mode,data['edit'])]=data
    return rows
native=read_rows('baseline.log');candidate=read_rows('candidate.log')
for edited in (0,1):
    b=native['native',edited];old=candidate['WR2-before',edited];new=candidate['WR2-hoisted',edited]
    for key in ['restarts','order','start','end']:
        assert b[key]==old[key]==new[key],key
    assert b['snap']==b['output-masked']==2
    assert old['snap']==old['output-masked']==(6 if edited else 2)
    assert new['snap']==(2 if edited else 0)
    assert new['output-masked']==new['BASEPRI-masked']==0
    assert new['PCM-visits']==(b['PCM-visits'] if edited else 0)
    assert old['PCM-visits']==b['PCM-visits']*(3 if edited else 1)
for name in ['baseline.log','baseline-asan.log','candidate.log','candidate-asan.log']:
    assert 'PASS differential fixture;' in (o/name).read_text(encoding='utf-8-sig')
native_loop=next(l for l in (o/'baseline.log').read_text(encoding='utf-8-sig').splitlines() if l.startswith('LOOP-DIFFERENTIAL'))
candidate_loop=next(l for l in (o/'candidate.log').read_text(encoding='utf-8-sig').splitlines() if l.startswith('LOOP-DIFFERENTIAL'))
assert 'accepted=1 starts=4' in native_loop
assert 'accepted=0 starts=0 RNG-advanced=1 notReady=1' in candidate_loop
tool=r/'agt/10.3/bin/arm-none-eabi-nm.exe'
elf=r/'platforms/imx/build-PLUS/TrackerPlus.axf';ref=r/'build/phase4/robustness-reference.axf'
symbols=subprocess.run([str(tool),'-C','--defined-only',str(elf)],check=True,capture_output=True).stdout
reference_symbols=subprocess.run([str(tool),'-C','--defined-only',str(ref)],check=True,capture_output=True).stdout
assert symbols==reference_symbols
assert not re.search(rb'wr2::|WeightedRandom|wr2Native',symbols)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert elf.read_bytes()==ref.read_bytes()
ptf=r/'build/TrackerPlus_EXP_WR2_A1_OFF.ptf';ptf_ref=r/'build/phase4/robustness-reference.ptf'
assert ptf.read_bytes()==ptf_ref.read_bytes()
result=dict(original_commit='998b06d40bd0d99de9ce92874bf2732298ca3fa7',native=[native[k] for k in sorted(native)],candidate_before=[candidate[k] for k in sorted(candidate) if k[0]=='WR2-before'],candidate_fixed=[candidate[k] for k in sorted(candidate) if k[0]=='WR2-hoisted'],native_loop=native_loop,candidate_loop=candidate_loop,off_elf_sha256=sha(elf),off_ptf_sha256=sha(ptf),off_ptf_bytes=ptf.stat().st_size,off_elf_bytes=elf.stat().st_size,off_symbols_identical=True,off_symbols_sha256=hashlib.sha256(symbols).hexdigest(),firmware_on_built=False)
(o/'comparison.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('PASS automated FX differential, Loop counterexample and OFF binary/symbol comparison')
