from pathlib import Path
import subprocess,struct,json,hashlib
r=Path(__file__).resolve().parents[3];o=r/'build/wr2-a1-tests';tools=r/'agt/10.3/bin'
s=(Path(__file__).parent/'test.cpp').read_text(encoding='utf-8');a=s.index('struct Bounds{');b=s.index('};',a)+2;bounds=s[a:b]
unit='''#include "../../tests/wr2/minimal/LocalSelection.h"
using namespace wr2minimal;using std::uint16_t;using std::uint32_t;
'''+bounds+'''
struct ProposedChannel {ChannelState selection;Bounds activeGeometry,pendingGeometry;};
struct ProposedBudget {PublishedBank bank;ProposedChannel channels[8][2];unsigned revision[48];Random rng;};
extern "C" const unsigned a1_budget[]={sizeof(Bounds),sizeof(ProposedChannel),sizeof(ProposedBudget)};
'''
o.mkdir(parents=True,exist_ok=True);(o/'budget.cpp').write_text(unit,encoding='utf-8')
cmd=[str(tools/'arm-none-eabi-g++.exe'),'-std=c++17','-O2','-mcpu=cortex-m7','-mthumb','-mfpu=fpv5-sp-d16','-mfloat-abi=hard','-fno-exceptions','-fno-rtti','-fdata-sections','-c',str(o/'budget.cpp'),'-o',str(o/'budget.o')];subprocess.run(cmd,check=True)
subprocess.run([str(tools/'arm-none-eabi-objcopy.exe'),'-O','binary','-j','.rodata.a1_budget',str(o/'budget.o'),str(o/'budget.bin')],check=True)
values=struct.unpack('<3I',(o/'budget.bin').read_bytes());result=dict(zip(['bounds','channel_with_two_geometry_records','candidate_ram'],values));result['note']='Prospective type layout, not implemented protocol or firmware allocation; PCM identity/revision details can add cost';result['bounds_definition_sha256']=hashlib.sha256(bounds.encode()).hexdigest();(o/'budget.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
