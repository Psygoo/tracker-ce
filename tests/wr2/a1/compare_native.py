from pathlib import Path
import re,json,sys
r=Path(__file__).resolve().parents[3]
logs=Path(sys.argv[1]) if len(sys.argv)>1 else r/'build/native-modes'
def pairs(line):return {k:int(v) if v.isdigit() else v for k,v in re.findall(r'([\w-]+)=([^ ]+)',line)}
def read(name,head,event):
 cases={};current=None
 for line in (logs/name).read_text(encoding='utf-8-sig').splitlines():
  if line.startswith(head):
   row=pairs(line);key=tuple(row[k] for k in ['old','lfo','fx']);current=dict(summary=row,events=[]);cases[key]=current
  elif line.startswith(event) and current is not None:current['events'].append(pairs(line))
 assert len(cases)==32,(name,len(cases))
 return cases
b=read('baseline.log','MODE original ','EVENT ')
p=read('candidate.log','MODE candidate ','EVENT ')
n=read('firmware.log','NATIVE-MODE ','NATIVE-EVENT ')
checks=0
for key in sorted(b):
 original=b[key];prototype=p[key];native=n[key]
 for case in [original,prototype,native]:
  assert case['summary']['accepted']==1
  assert len(case['events'])==case['summary']['starts']==(4 if key[1] else 2)
  checks+=2
 assert prototype['summary']['draws']==native['summary']['draws']==1
 assert native['summary']['basepri']==0
 for idx,(a,c) in enumerate(zip(original['events'],native['events'])):
  for field in ['instrument','channel','tech','mode']:assert a[field]==c[field],(key,idx,field,a,c)
  if c['instrument']==0 and c['mode']==5:
   assert c['slice'] in (1,2)
   assert c['start']==c['slice']*1000 and c['end']==(c['slice']+1)*1000
   assert c['loop1']==c['loop2']==0 and c['span']==1000
  else:
   for field in ['start','end','loop1','loop2','span']:assert a[field]==c[field],(key,idx,field,a,c)
  checks+=9
 for events in [prototype['events'],native['events']]:
  for idx in range(0,len(events),2):
   assert events[idx]['channel']==0 and events[idx+1]['channel']==1
   for field in ['instrument','slice','start','end','loop1','loop2','span']:assert events[idx][field]==events[idx+1][field],(key,field)
  if key[1] and key[2]:assert events[0]['slice']==events[2]['slice']
text=(logs/'firmware.log').read_text(encoding='utf-8-sig')
for beat in [0,1]:assert f'FIRMWARE FX-only beat={beat} snap=2 output-masked=2 BASEPRI=0' in text
assert 'PASS actual bridge extra:' in text and 'PASS actual native bridge:' in text
for file in ['baseline-asan.log','candidate-asan.log','firmware-asan.log']:
 text=(logs/file).read_text(encoding='utf-8-sig');assert 'PASS' in text and 'AddressSanitizer: ' not in text
data=dict(cases=32,modes=8,checks=checks,original_accepts_all=True,prototype_accepts_all=True,actual_bridge_accepts_all=True,one_draw_per_master=True,stereo_pairs_match=True,native_geometry_preserved=True,new_beat_selection_valid=True,actual_fx_only_snap_calls=2,actual_fx_only_new_basepri_pcm_calls=0)
(logs/'mode-comparison.json').write_text(json.dumps(data,indent=2))
print('PASS differential: 32 transitions, 8 modes, original acceptance/restart order/instruments/native bounds, one WR2 selection, stereo, FX-only and ASan logs')
