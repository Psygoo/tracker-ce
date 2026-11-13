from pathlib import Path
import subprocess,json,struct
r=Path(__file__).resolve().parents[3];o=r/'build/wr2-minimal-tests';o.mkdir(parents=True,exist_ok=True);tools=r/'agt/10.3/bin'
unit='''#include "../../tests/wr2/minimal/ArmRegisters.h"
#include "../../tests/wr2/minimal/LocalSelection.h"
using namespace wr2minimal;
extern "C" const unsigned minimal_budget[]={sizeof(Prepared),sizeof(ChannelState),sizeof(Budget),sizeof(Guard<ArmRegisters>),sizeof(StereoFence<ArmRegisters>)};
Prepared prepareArm(const PublishedBank&bank,const Request&r){return prepare(bank,r);}
Selection chooseArm(const Prepared&p,Random&r){return choose<ArmRegisters>(p,r);}
void fenceProof(unsigned*value){StereoFence<ArmRegisters>guard;*value=123;}
'''
(o/'arm.cpp').write_text(unit)
base=['-std=c++17','-O2','-mcpu=cortex-m7','-mthumb','-mfpu=fpv5-sp-d16','-mfloat-abi=hard','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections','-fstack-usage','-Wall','-Wextra','-Werror','-I'+str(r/'platforms/imx/device'),'-I'+str(r/'platforms/imx/CMSIS')]
commands=[]
for source,target in [(o/'arm.cpp',o/'arm.o'),(r/'modules/wr2/WeightedRandom.cpp',o/'WeightedRandom.o'),(r/'modules/wr2/AudioAdapter.cpp',o/'AudioAdapter.o')]:
 cmd=[str(tools/'arm-none-eabi-g++.exe'),*base,'-c',str(source),'-o',str(target)];commands.append(cmd);subprocess.run(cmd,check=True)
subprocess.run([str(tools/'arm-none-eabi-objcopy.exe'),'-O','binary','-j','.rodata.minimal_budget',str(o/'arm.o'),str(o/'budget.bin')],check=True)
budget=dict(zip(['prepared_stack','channel','candidate_total','guard_stack_object','fence_stack_object'],struct.unpack('<5I',(o/'budget.bin').read_bytes())))
(o/'budget.json').write_text(json.dumps(budget,indent=2));(o/'arm-commands.json').write_text(json.dumps(commands,indent=2));print('ARM budget:',json.dumps(budget))
for tool,args,name in [('objdump',['-d','-C',str(o/'arm.o')],'arm-disassembly.txt'),('nm',['-u',str(o/'arm.o')],'arm-undefined.txt'),('size',[str(o/'arm.o'),str(o/'WeightedRandom.o'),str(o/'AudioAdapter.o')],'arm-size.txt')]:
 data=subprocess.check_output([str(tools/('arm-none-eabi-'+tool+'.exe')),*args]);(o/name).write_bytes(data)
 if name=='arm-undefined.txt':assert b'__atomic' not in data and b'__sync' not in data
print('PASS detached ARM objects/CMSIS intrinsics; no firmware link/PTF')
