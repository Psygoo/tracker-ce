from pathlib import Path
import re,json,sys
r=Path(__file__).resolve().parents[3];o=Path(sys.argv[1]) if len(sys.argv)>1 else r/'build/native-modes'
def fields(s):return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',s)}
def read(name):
 rows={};single={};current=None
 for line in (o/name).read_text(encoding='utf-8-sig').splitlines():
  if line.startswith('REAL-MODE '):
   v=fields(line);key=tuple(v[k] for k in ['old','lfo','fx','phase']);current=dict(summary=v,events=[]);rows[key]=current
  elif line.startswith('REAL-EVENT '):current['events'].append(fields(line))
  elif line.startswith('REAL-SINGLE '):
   v=fields(line);single[tuple(v[k] for k in ['slot','newLfo','phase'])]=v
 assert len(rows)==96 and len(single)==16,(name,len(rows),len(single))
 return rows,single
b,bs=read('real-original.log');n,ns=read('real-firmware.log')
for key in b:
 a=b[key];c=n[key]
 for field in ['accepted','immediate','starts','pending']:assert a['summary'][field]==c['summary'][field],(key,field,a,c)
 assert c['summary']['accepted']==1 and c['summary']['selected'] in (1,2)
 assert c['summary']['snap']<=a['summary']['snap'] and c['summary']['basepri']==0
 assert len(a['events'])==len(c['events'])==a['summary']['starts']
 for x,y in zip(a['events'],c['events']):
  for k in ['instrument','channel','tech','mode']:assert x[k]==y[k],(key,k,x,y)
  if y['instrument']==0:
   assert y['slice']==c['summary']['selected'] and y['start']==y['slice']*1000 and y['end']==(y['slice']+1)*1000
  else:
   for k in ['start','end','loop1','loop2','span']:assert x[k]==y[k],(key,k,x,y)
for key in bs:
 for field in ['accepted','immediate','starts','pending']:assert bs[key][field]==ns[key][field],(key,field)
 assert ns[key]['before']<=bs[key]['before'] and ns[key]['snap']<=bs[key]['snap'] and ns[key]['basepri']==0
for name in ['real-original-asan.log','real-firmware-asan.log']:
 t=(o/name).read_text(encoding='utf-8-sig');assert 'PASS real Amp LFO helper:' in t and 'AddressSanitizer: ' not in t
result=dict(real_amp_transitions=96,single_lfo_phase_cases=16,total_cases=112,acceptance_delay_resume_order_equal=True,native_geometry_equal=True,wr2_selection_and_rng_checked=True,no_additional_pcm_calls=True,no_new_basepri_pcm_calls=True,asan=True)
(o/'real-amp-comparison.json').write_text(json.dumps(result,indent=2))
print('PASS real native Amp helper differential: 112 cases; accepted/delayed/resumed, instruments/order/native bounds, WR2 choice and no extra Snap calls')
