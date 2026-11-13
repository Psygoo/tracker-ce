from pathlib import Path
import re,json,sys
r=Path(__file__).resolve().parents[3];o=Path(sys.argv[1]) if len(sys.argv)>1 else r/'build/hardware-prep'
def rows(name):
 text=(o/(name+'.log')).read_text(encoding='utf-8-sig')
 assert 'PASS native IO/export:' in text and 'ERROR: AddressSanitizer' not in text
 result=[dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',line)) for line in text.splitlines() if line.startswith('IO case=')]
 assert len(result)==96
 return result,[l for l in text.splitlines() if l.startswith('EXPORT ')],[l for l in text.splitlines() if l.startswith('NATIVE-DISTRIBUTION')]
original,export,_=rows('io-original');allresults={}
for name in ['io-original-asan','io-normal','io-normal-asan']:
 data,e,hist=rows(name);assert e==export
 for a,b in zip(original,data):
  for key in a:
   if key in ('snap','masked'):assert b[key]<=a[key],(name,key,a,b)
   else:assert b[key]==a[key],(name,key,a,b)
 allresults[name]=dict(cases=96,original_snap=sum(x['snap'] for x in original),candidate_snap=sum(x['snap'] for x in data),original_output_masked_snap=sum(x['masked'] for x in original),candidate_output_masked_snap=sum(x['masked'] for x in data),distribution=hist)
assert rows('io-normal')[2]==rows('io-normal-asan')[2]
(o/'io-comparison.json').write_text(json.dumps(allresults,indent=2))
print(json.dumps(allresults,indent=2))
print('PASS 96 CE-native differential streaming cases in all configurations, master/stems exporter loop, no additional Snap; UI-created 60/30/10 native distribution normal/ASan identical')
