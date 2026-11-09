# SD Sample Editor — Cancel e Undo

Data: 2026-11-09
Branch: `codex/sample-editor-sd`
Base HEAD: `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`

## Resultado

Foi corrigido o redesenho após Cancel e implementado Undo de um nível para Apply SD, sem carregar o sample inteiro para RAM. O firmware experimental Tracker+ foi compilado sem WR2. Não foi instalado no equipamento; a funcionalidade continua por validar fisicamente.

## Alterações

- O cancelamento redesenha o Sample Editor antes de mostrar a mensagem, incluindo a coluna dos efeitos. Fechar a mensagem restaura os handlers sem exigir outro redesenho.
- O Apply SD guarda um descritor validado do estado anterior e conserva o sample anterior através de rotação transacional de ficheiros. O Undo valida identidade e CRC, restaura os dados por blocos, preserva metadados e reconstrói a cache.
- O Undo cobre Crop, Reverse, Fade In/Out, Normalize, Amplifier, Overdrive e Bitcrusher. Apply consecutivo substitui o estado Undo anterior, seguindo o modelo de um nível. Cancel não cria novo Undo e conserva o último Undo válido.
- Snapshots Undo são voláteis e são descartados ao sair do editor, mudar de instrumento ou recuperar no arranque. O ficheiro temporário de Undo é separado dos nomes `.mono.*` usados pelas transações.
- O Undo nativo RAM não foi alterado. Nenhuma dependência WR2/CT2 foi adicionada.

## Testes executados

- Host completo: passou, 469.749.618 verificações.
- Host ASan completo do Sample Editor SD: passou, 469.749.618 verificações.
- Host e ASan focados em Undo, após os últimos ajustes: passaram; cobrem os 11 efeitos SD, incluindo Equalizer, Limiter e Compressor, e falha de cabeçalho corrompido sem alteração do instrumento ativo.
- Host ASan de editor RAM: passou, 419.659 verificações.
- Host ASan de Preview SD: passou, 2.829.432 verificações.
- Fixture da interface de Cancel: passou; confirma desenho da lista antes do popup e ausência de redesenho obrigatório ao fechar.
- Undo exercitado nos 11 efeitos SD suportados, Apply→Apply→Undo, seleção parcial, mono/estéreo, metadados, cancelamento, troca de contexto, falhas injetadas de I/O e interrupções simuladas nas fronteiras transacionais.
- Recuperação transacional exercitada para os 8 efeitos, com falhas e cortes de energia simulados em todos os pontos de operação.
- `git diff --check`: passou.

Os testes de Undo dos efeitos DSP nativos usam kernels de fixture para verificar a transação e a restauração exata; os testes DSP separados continuam a validar os algoritmos nativos e o processamento SD.

## Build e memória

Build ARM Tracker+ concluído com SD Mono/Edits ligados e WR2 no valor predefinido desligado. Link concluído sem erro. O linker apresentou avisos de limites de arrays em `sampleEditor.h` e um aviso de inicializador; não bloquearam o build.

- PTF: `build/TrackerPlus_TrackerPlus_EXP_CE_SDUNDO_FINAL.ptf`
- Tamanho: 4.852.709 bytes (4,63 MiB)
- SHA-256: `71A31EE989014BD563BE83C4F339B4E8715F74DA498E6EB985A3560EB77978A8`
- SRAM DTC: 343.192 / 360.448 bytes (95,21%)
- SRAM ITC: 114.648 / 131.064 bytes (87,47%)
- SRAM OC: 23.212 / 32.768 bytes (70,84%)
- QSPI flash: 1.710.820 bytes (10,32% de 16 MiB)
- SDRAM: 67.096.184 / 67.108.864 bytes reportados (99,98% da região reservada)

O caminho de Undo reutiliza o espaço de trabalho externo do Sample Editor e processamento por blocos; não acrescenta um buffer PCM permanente. A ocupação reportada de SDRAM é uma reserva quase completa já presente na configuração do firmware, pelo que deve ser confirmada em runtime no hardware antes de concluir margem física.

## Riscos e limitações

- Testes host/ASan e compilação ARM não comprovam o comportamento na SD e no hardware real.
- A recuperação após falhas foi simulada no harness, não validada com remoção física do cartão durante operações.
- Undo é temporário e desaparece ao sair do Sample Editor, trocar instrumento, reiniciar ou abrir outro contexto; Save/Open não persiste o histórico Undo.
- A cópia anterior fica representada por rotação/renomeação de ficheiros no volume SD. Falhas de filesystem específicas do cartão ainda requerem validação física.
- PTF é experimental e não validado fisicamente.

## Estado Git

Branch `codex/sample-editor-sd`, HEAD `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`. Sem commit, push, merge ou Pull Request. Alterações locais em 8 ficheiros de implementação e harness; PTFs anteriores preservados. A working tree original `C:\Projects\tracker-ce` não foi alterada.

## Próximos passos

1. Validar no Tracker+ Apply→Undo por efeito, Apply consecutivo→Undo, cancelamento, seleção parcial e streaming SD.
2. Testar SD cheia, falha/remoção do cartão e reinício durante Apply e Undo.
3. Confirmar o ciclo de vida do Undo ao abrir/guardar projeto e trocar instrumento.
4. Depois da sessão física, rever avisos do linker e margem SDRAM em runtime.
