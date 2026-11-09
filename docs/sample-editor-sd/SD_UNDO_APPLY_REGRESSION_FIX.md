# Regressão Apply SD / Undo — correção SDUFIX3

Data: 2026-11-09. Projeto: C:\Projects\tracker-ce-sd. Branch: codex/sample-editor-sd.
HEAD público/local de referência: `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`.

## Causa raiz e reprodução

A mensagem vinha de `Job::tickUndoScan`, acrescentada pela implementação local de Undo. Este código exigia que o PTI canónico do Workspace tivesse exatamente cabeçalho + todos os frames PCM do instrumento. O CE admite Workspace com apenas os parâmetros PTI, mantendo o PCM usado pelo streaming noutro caminho. A implementação anterior de Apply em HEAD não tinha esta exigência: lia PCM através da fonte real em `Job::input`.

A fixture anterior atribuía sempre um PTI completo a `Workspace/instruments/instrument_01.pti`; no caso externo, os dois nomes também podiam apontar ao mesmo objeto de ficheiro simulado. Por isso, os testes não representavam esta disposição legítima do filesystem.

Foi acrescentado um teste transacional com dois ficheiros independentes: cabeçalho no Workspace e PCM externo completo. Antes da correção produziu exatamente `Undo source size changed; original preserved.` e recusou Apply. Depois da correção produz `Converted.` e permite Undo. Isto reproduz a falha observada, mas não substitui uma análise dos ficheiros do cartão concreto utilizado no hardware.

## Correção

- `Job::tickUndoScan` materializa um PTI Undo completo em `.mono.undo.new`, lendo o PCM original através de `Job::input`: RAM quando aplicável, fonte real do streaming nos restantes casos. Escreve o cabeçalho a partir dos parâmetros atuais e copia por blocos, sem carregar o sample completo.
- Calcula CRC durante a cópia, sincroniza/fecha e relê integralmente o snapshot para verificar tamanho e CRC antes de permitir Promote.
- `finalizeApplyRecovery` promove esse snapshot separado em vez de usar o backup do Workspace como PCM Undo. A recuperação é idempotente nas interrupções antes/depois dos renames. Erro de leitura do descritor novo impede finalização; não há fallback para um descritor antigo de outro Apply.
- O backup do Workspace continua a proteger a transação e o rollback. O snapshot Undo só fica disponível após Apply e validação concluídos.
- A estimativa de espaço livre inclui simultaneamente o resultado temporário e o snapshot completo do original. SD insuficiente recusa antes de alterar o instrumento.
- `Job::fail` limpa o snapshot novo incompleto; Cancel preserva o original e o Undo anterior.
- `sampleedit::cancel` devolve texto vazio em cancelamento normal. `cSampleEditor::update` omite popup para esse resultado vazio. Erros de I/O, CRC e recuperação continuam visíveis; mantém-se o redesenho validado.

Não foram alterados DSP, Preview de 10 segundos, algoritmos RAM ou Undo RAM. Não há integração WR2/CT2.

## Ficheiros alterados nesta correção

- `core/FileManager/imx/SampleFileEdit.cpp`: snapshot, validação, espaço, recuperação e Cancel.
- `core/interface/sampleEditor/fSampleEdit.cpp`: Cancel silencioso no ciclo de conclusão.
- `tests/stereo-mono/test.cpp`: reprodução header-only, matriz dos 11 efeitos, PCM externo, Undo exato e corrupção real do snapshot.
- `tests/sd-effects/regions.inc`: expectativa de Cancel silencioso.
- `tests/sd-preview/dynamics.inc`: expectativa de Cancel silencioso.

Existem também alterações locais da implementação anterior de Cancel/Undo, ainda sem commit. Este relatório não as apresenta como alterações novas desta correção.

## Evidência de validação

- Reprodução anterior à correção: falha exata confirmada no fluxo completo de Apply.
- Matriz nova: os 11 efeitos regionais/DSP, mono e estéreo, fonte externa e Workspace header-only, seleção parcial, slices e verificação de todos os frames após Undo. Apply→Apply→Undo compara bytes do primeiro resultado e do resultado restaurado.
- Stereo to Mono: modos Mix/Left/Right, Apply e Undo, mudança de canais e streaming. Crop cobre mudança da duração.
- Falhas: injeção de I/O, SD cheia, escritas curtas, Cancel, corrupções e interrupção antes/depois das operações de filesystem; recuperação repetível.
- Host completo final: PASS, 473.383.777 verificações, exit code 0.
- ASan completo final: PASS, 473.383.777 verificações, exit code 0.
- RAM ASan: PASS, 419.659 verificações.
- Preview e DSP nativo ASan: PASS, 2.983.033 verificações. EQ/Limiter/Compressor bit-identical aos algoritmos CE; falhas/recuperação em 107/103/103 fronteiras nessa execução.
- Logs detalhados: `build/undo-fix-host-verified.log`, `build/undo-fix-asan-verified.log`, `build/undo-fix-preview.log`, `build/undo-fix-ram.log`, `build/undo-fix-arm-verified.log`.
- `git diff --check`: PASS.

Os kernels usados pela matriz adicional de Undo de EQ/Limiter/Compressor são fixtures de transação; a equivalência DSP real é verificada separadamente pela suite nativa de dinâmica. As falhas SD e cortes são simulados; não são testes físicos.

## Firmware e memória

Build ARM Tracker+ concluído, ARM GCC 10.3-2021.10, SD Mono/SD Edits ON e sem WR2. Identificação: `SDUFIX3`.

PTF para a próxima validação: `C:\Projects\tracker-ce-sd\build\TrackerPlus_TrackerPlus_EXP_CE_SDUNDO_FIX3.ptf`

Tamanho: 4.855.338 bytes.
SHA-256: `DBAD08EA4B6F152506DCB81EB982F9245DD3EB64D6119DDBB7938CA9C64737BD`.

| Região | Utilização | Diferença face ao SDUNDO anterior |
| --- | ---: | ---: |
| QSPI | 1.711.740 B | +920 B |
| SRAM DTC | 343.192 B | 0 |
| SRAM ITC | 114.648 B | 0 |
| SRAM OC | 23.212 B | 0 |
| SDRAM reservada | 67.096.184 B | 0 |

Reutiliza os buffers por blocos existentes. O snapshot requer espaço SD adicional e acrescenta leitura/escrita/verificação de todo o original antes do commit. A duração adicional não foi medida no hardware. Não foram acrescentadas regiões com IRQ mascarada. A ocupação do linker de SDRAM representa a reserva do firmware, não uma medição da margem dinâmica ou do stack.

## Limitações e validação física pendente

O firmware ainda não foi testado no Tracker+. Confirmar Apply efetivo, Undo, Apply consecutivo, Cancel silencioso, streaming e recuperação nos cartões reais. Dar prioridade ao mesmo projeto que reproduziu a falha. Testar Stereo to Mono e Crop, pois alteram canais/duração, além dos restantes efeitos. Undo continua temporário e de um nível; sair do editor/trocar contexto descarta-o como definido na implementação anterior.

Os PTF anteriores foram preservados. Os ficheiros intermédios SDUNDO_FIX e SDUNDO_FIX2 não são a entrega final; usar exclusivamente SDUNDO_FIX3 para esta validação.

## Estado Git

Branch `codex/sample-editor-sd`; HEAD permanece `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`. Alterações locais não commitadas. Sem push, merge, PR ou flash. A branch pública e a working tree original `C:\Projects\tracker-ce` não foram alteradas.
