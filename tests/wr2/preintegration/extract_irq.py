from pathlib import Path
import hashlib,json,sys
root=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
p=root/'platforms/imx/source/interruptsControl.cpp';raw=p.read_bytes();src=raw.decode('utf-8')
body='\n'.join(line for line in src.splitlines() if not line.startswith('#include'))
unit=r'''#include <cstdio>
#define AUDIO_CORE_FLASH
static bool enabled=true;
enum class IRQn_Type { DMA0_DMA16_IRQn };
void NVIC_EnableIRQ(IRQn_Type){enabled=true;}
void NVIC_DisableIRQ(IRQn_Type){enabled=false;}
'''+body+r'''
int main(){
 enabled=true;disableAudioInterrupt();if(enabled)return 1;
 disableAudioInterrupt();enableAudioInterrupt();if(!enabled)return 2;
 std::puts("CONFIRMED: inner enable restores DMA IRQ before outer scope ends (not nesting-safe)");
 enabled=false;disableAudioInterrupt();enableAudioInterrupt();if(!enabled)return 3;
 std::puts("CONFIRMED: initially disabled DMA IRQ becomes enabled (previous state not restored)");
 std::puts("PASS: exact native helper bodies, mocked NVIC; not a hardware IRQ timing test");
}
'''
(out/'irq_contract.cpp').write_text(unit,encoding='utf-8')
(out/'irq-source.json').write_text(json.dumps({'source':str(p),'sha256':hashlib.sha256(raw).hexdigest(),'mock':'NVIC enable bit only; no timing or ISR emulation'},indent=2))