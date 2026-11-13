from pathlib import Path
import subprocess, struct, json, hashlib
r=Path(__file__).resolve().parents[3]
o=r/'build/wr2-a1-tests'
tools=r/'agt/10.3/bin'
s=(Path(__file__).parent/'test.cpp').read_text(encoding='utf-8')
def declaration(name):
    a=s.index('struct '+name+'{');b=s.index('{',a);e=b+1;depth=1
    while depth:
        depth+=(s[e]=='{')-(s[e]=='}');e+=1
    assert s[e]==';'
    return s[a:e+1]
definitions='\n'.join(declaration(n) for n in ['GeometryKey','Bounds','Context'])
unit='''#include "../../tests/wr2/minimal/LocalSelection.h"
using namespace wr2minimal; using std::uint16_t; using std::uint32_t;
'''+definitions+'''
struct CandidateChannel {ChannelState selection;GeometryKey committedKey;bool committedValid;};
struct CandidateBudget {PublishedBank bank;CandidateChannel channels[8][2];unsigned revision[48];Random rng;};
extern "C" const unsigned a1_fix_budget[]={sizeof(GeometryKey),sizeof(Bounds),sizeof(Context),sizeof(ChannelState),sizeof(CandidateChannel),sizeof(CandidateBudget),sizeof(PublishedBank)};
'''
o.mkdir(parents=True,exist_ok=True)
(o/'budget-fix.cpp').write_text(unit,encoding='utf-8')
cmd=[str(tools/'arm-none-eabi-g++.exe'),'-std=c++17','-O2','-mcpu=cortex-m7','-mthumb','-mfpu=fpv5-sp-d16','-mfloat-abi=hard','-fno-exceptions','-fno-rtti','-fdata-sections','-c',str(o/'budget-fix.cpp'),'-o',str(o/'budget-fix.o')]
subprocess.run(cmd,check=True)
subprocess.run([str(tools/'arm-none-eabi-objcopy.exe'),'-O','binary','-j','.rodata.a1_fix_budget',str(o/'budget-fix.o'),str(o/'budget-fix.bin')],check=True)
values=struct.unpack('<7I',(o/'budget-fix.bin').read_bytes())
result=dict(zip(['geometry_key','transient_bounds','transient_context','previous_channel','candidate_channel','candidate_static_budget','published_bank'],values))
result['additional_key_bytes_for_16_channels']=16*(values[4]-values[3])
result['previous_duplicate_geometry_budget']=6852
result['note']='Measured detached ARM type layout only. No linked firmware storage. Native points reused; no duplicate persistent geometry. Context excludes call-stack overhead and nested calls.'
result['definitions_sha256']=hashlib.sha256(definitions.encode()).hexdigest()
(o/'budget-fix.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
