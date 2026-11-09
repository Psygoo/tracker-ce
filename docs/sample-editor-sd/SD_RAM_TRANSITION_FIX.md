# Correção da transição dinâmica RAM / SD — SDSWAP

Data: 2026-11-09. Projeto: `C:\Projects\tracker-ce-sd`. Branch: `codex/sample-editor-sd`. Referência: SDIND.

## Resultado

Corrigida a reinicialização do widget da lista ao mudar de instrumento com o Sample Editor aberto. Host e AddressSanitizer passaram 300 ciclos completos / 600 mudanças, com ambos os sentidos, slots 1 e 48, nomes distintos, páginas distintas, seleções variadas e 0, 3 e 128 plugins Modular. O build ARM Tracker+ foi concluído sem WR2.

A validação física desta versão está pendente. Não foi capturado um dump/SWD do crash no equipamento; o percurso de falha foi reproduzido e diagnosticado no host com ASan.

## Causa raiz

A lista tem dois níveis de estado:

1. O descritor `effectListData`: tabela de nomes, length, linesCount e start.
2. O widget nativo `cList`: listPosition, barPos, textListPos e estado de animação.

`start()` já carregava o novo instrumento e chamava `configureEffectList()` antes de resolver currentEffectIdx/currentEffect e os parâmetros. usesSdEditing e a tabela completa RAM eram atualizados corretamente. Os nomes dos efeitos nativos, os nomes dos plugins e os arrays filtrados tinham duração válida.

O erro estava no contrato com o widget: configureEffectList substituía o descritor em memória e punha start=0, mas não chamava `cList::setData()`. O widget existente continuava a usar os offsets de scroll e animação da lista anterior. O posterior setValue alterava o destino da seleção, mas não reconstruía esses offsets para a nova dimensão.

Assim, o desenho/animador podia gerar `rowIdx >= length`. Na troca SD→RAM, a lista lógica já era RAM, mas a atualização gráfica passava a ler linhas inexistentes e não conseguia apresentar corretamente a nova lista. No regresso RAM→SD o mesmo problema aparecia ao encolher o número de linhas.

Os quadrados brancos são compatíveis com esse envio de texto inválido e com uma atualização gráfica interrompida/misturada com a anterior. Não foi encontrada uma transformação de encoding ou libertação das strings. A leitura inválida está comprovada no harness; o glifo branco exato do display físico não foi emulado.

## Reprodução e diagnóstico antes da correção

Foi criado um harness com o percurso de produção:

`functInstrument(press) → seleção do slot no popup → functInstrument(release) → cancelGlobalPopups() → start() completo → refreshEffectList() → cList::setValue/update/animação`.

O widget permanece a mesma instância: não se sai nem se recria o editor entre mudanças. O adaptador do popup fornece a alteração do slot; engine, FileManager e carregamento DSP são simulados. Os corpos completos dos handlers, cancelGlobalPopups, start, refreshEffectList e cList são extraídos dos fontes reais.

ASan reproduziu dois casos:

| Sentido | Estado inválido observado |
| --- | --- |
| SD→RAM | slot 48; length=18; linesCount=14; start=6; listPosition=17; textListPos=5; barPos=12 |
| RAM→SD | slot 1; length=12; linesCount=12; start=1; listPosition=11; textListPos=4; barPos=14 |

No primeiro caso, 14 linhas a partir do offset 5 chegam à linha 18, mas o último índice válido é 17. No segundo caso, o viewport antigo também ultrapassa os 12 elementos SD. Em ambos houve ponteiro nulo de texto e READ na página zero em strlen, com stack de cList::update. A stack RAM→SD inclui refreshEffectList, showMainScreen, start, cancelGlobalPopups e functInstrument.

No firmware real, `API_CMD_TEXT → DisplayDriverImpl::sendString → API_SendString` também chama strlen sobre o ponteiro recebido. Não existe validação de rowIdx no caminho nativo de desenho da lista. Não foi alterado esse código genérico: foi corrigido o chamador que substituía o descritor sem reinicializar o widget.

Logs preservados:

- `build/sd-transition-asan-repro.log`: diagnóstico antes de corrigir produção, SD→RAM.
- `build/sd-transition-return-asan-repro.log`: diagnóstico RAM→SD com a variante histórica apenas no harness, sem reverter produção.

## Correção mínima

Em `cSampleEditor::configureEffectList()`:

- Constrói as mesmas listas e mantém os mesmos critérios/IDs já validados.
- Calcula `effectListData.start` através de `effectListRow()`, usando a seleção já validada e o modo do novo instrumento.
- Chama `display.setControlData(effectList, &effectListData)` antes do primeiro redesenho.

O setter nativo de cList reconstitui listPosition, barPos e textListPos. O primeiro update recebe start igual à seleção e limpa a animação pendente pelo percurso nativo. Não há animação a partir da geometria da lista do instrumento anterior.

Não foram acrescentados caches, cópias de nomes, novos índices internos ou estado permanente. O efeito nativo mantém o ID global; somente o controlo recebe a linha visível. Uma seleção RAM não suportada em SD continua a ser substituída pelo primeiro efeito SD válido, antes de resolver o objeto e parâmetros, como já definido na filtragem existente.

## Verificação da lista completa e dos índices

Após cada mudança e após navegação, os testes verificam:

- usesSdEditing corresponde ao sample do slot atual.
- O widget aponta para o descritor atual, sem recriação.
- A tabela RAM é exatamente a do registo PtfxManager; a SD é a tabela filtrada existente.
- Todos os nomes da tabela, não apenas as linhas atualmente visíveis, correspondem aos nomes originais e dos plugins.
- Cada texto enviado ao renderer pertence à lista atual.
- O viewport completo cabe em length.
- listPosition/start correspondem à linha visível da seleção nativa.
- currentEffectIdx resolve selectedEffectListIdx corretamente e currentEffect aponta ao objeto correspondente.
- Quando Modular está ativo, currentModularPluginIdx é válido para o registo atual. Esse campo não é consultado nos efeitos nativos.

Com 0 plugins, RAM contém os 18 efeitos nativos desta compilação; com 3, contém 21 entradas; com 128, contém 146. SD conserva os mesmos 12 efeitos suportados. Os plugins são globais no CE, não um atributo de cada instrumento; a matriz representa registos globais com 0/3/128 plugins.

## Matriz executada

Para cada registo de 0, 3 e 128 plugins:

- 50 ciclos SD→RAM→SD.
- 50 ciclos RAM→SD→RAM.
- Total: 300 ciclos / 600 mudanças por execução Host e por execução ASan.
- Instrumentos nos slots 1 e 48, nomes distintos, sample longo e sample curto.
- Seleções no início, meio e fim, incluindo plugins, com navegação/animação nativas.
- Origem no ecrã principal e Settings, páginas de parâmetros 0/1/2.

A mudança mantém o comportamento nativo de start: regressa ao ecrã principal e inicializa os parâmetros do instrumento selecionado. Não se criou uma nova política de retenção de páginas.

## Resultados

- Host final: PASS, 300 ciclos / 600 mudanças.
- AddressSanitizer final: PASS, mesma matriz, sem leitura inválida.
- Regressões existentes de navegação: PASS.
- Regressões de comandos gráficos do título/indicador independente: PASS.
- Verificação de TitleBar byte a byte e showTitleBar contra HEAD original: PASS.
- `git diff --check`: PASS.
- ARM Tracker+ / link final: PASS, GNU Arm 10.3-2021.10, SD Mono/Edits ON, sem WR2.

Logs finais:

- `build/sd-transition-final-host.log`.
- `build/sd-transition-final-asan.log`.
- `build/sd-transition-arm.log`.

Os testes anteriores falharam na cobertura porque chamavam configureEffectList e verificavam os arrays/mapeamentos com um widget simulado sem scroll interno. A nova fixture usa start e os handlers completos, a mesma instância real de cList e a sua animação/renderização. Remover a nova ligação volta a reproduzir a leitura inválida.

## Ficheiros desta tarefa

- `core/interface/sampleEditor/fSampleEdit.cpp`: única alteração de produção, dentro de configureEffectList.
- `tests/sd-editor-ui/extract.py`: extração do percurso real e variante histórica opcional de diagnóstico.
- `tests/sd-editor-ui/transitions.cpp`: transições integradas, desenho da lista, verificações e diagnóstico.
- `tests/sd-editor-ui/run.ps1`: execução da nova fixture em Host e ASan.
- `tests/sd-editor-ui/test.cpp`: adaptação do stub de ligação da fixture isolada à nova chamada.
- Este relatório.

Os restantes ficheiros que aparecem modificados no Git pertencem a trabalhos anteriores. Nesta tarefa não foram alterados Apply, Undo, Cancel, Preview, DSP, transações, predicados de filtragem, indicador SD ou barra nativa.

## Firmware e memória

Identificação: `SDSWAP`.
PTF: `C:\Projects\tracker-ce-sd\build\TrackerPlus_TrackerPlus_EXP_CE_SD_TRANSITION.ptf`.
Tamanho: 4.858.134 bytes.
SHA-256: `DA94B025258F6D1A19AE5A671E4C82ED77C2E270E66AE366EC6722C04DE39D58`.

| Região | Utilização | Diferença SDIND |
| --- | ---: | ---: |
| QSPI | 1.712.748 B | -8 B |
| SRAM DTC | 343.336 B | 0 |
| SRAM ITC | 114.648 B | 0 |
| SRAM OC | 23.212 B | 0 |
| SDRAM reservada | 67.096.184 B | 0 |

Sem nova reserva de buffers ou estado de produção. A medição do linker não constitui medição de margem dinâmica/stack no hardware.

## Git e preservação

Branch `codex/sample-editor-sd`, HEAD `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`, alterações locais não commitadas incluindo tarefas anteriores. Sem commit, push, merge, PR ou flash. `C:\Projects\tracker-ce` não foi alterado. SDIND, SDUFIX3 e as restantes referências foram preservados.

## Limitações e próximos testes físicos

Confirmar no Tracker+ a alternância repetida sem sair do editor, a recuperação de todos os efeitos RAM e plugins, nomes no fundo da lista e regresso ao SD. Repetir a partir dos Settings e com seleção Modular. Validar também um ciclo funcional de playback/Preview/Apply/Undo por amostragem.

O harness simula PCM, engine, filesystem e carregamento de plugins; não executa os binários dos plugins nem operações de gravação durante a troca de instrumento. Não emula o glifo físico dos quadrados brancos. A falha de acesso e a correção do estado da lista estão demonstradas em software; a confirmação física desta versão permanece pendente.
