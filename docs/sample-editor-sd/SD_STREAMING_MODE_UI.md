# Sample Editor — indicador e lista SD (SDMODE2)

Data: 2026-11-09. Projeto: `C:\Projects\tracker-ce-sd`, branch `codex/sample-editor-sd`.
Referência funcional: SDUFIX3, validada fisicamente pelo utilizador. HEAD Git: `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`.

## Resultado

Implementado indicador visual vermelho `LONG SAMPLE · SD STREAMING MODE` e filtragem exclusiva da apresentação/navegação SD. Os IDs internos dos efeitos não foram alterados. O firmware novo é experimental: compilado e testado em software, ainda sem validação física.

## Inventário dos índices, realizado antes de modificar código

| Estado / utilização | Local | Tratamento |
| --- | --- | --- |
| Enum `enSampleEditorEffect`, array `sampleEditorEffect`, parâmetros e nomes | `core/effector/mtSampleEditorEngine.*`, `sampleEditor.h` | IDs e tabelas originais preservados |
| `selectedEffectListIdx` | `start`, `modSelectedEffect`, `refreshEffectList` | Continua a guardar o índice original da lista global; nunca passa a guardar uma linha filtrada |
| `currentEffectIdx` / `currentEffect` | Resolução nativa ou Modular no start/navegação; parâmetros, canais, Apply, Preview e Undo | Continua a usar o ID original; sem remapeamento DSP |
| `currentModularPluginIdx` | Load, apresentação, parâmetros e resolução de plugin | Mantém o cálculo original `selectedEffectListIdx - kBuiltInEffectsCount` em RAM; plugins sem backend SD ficam ocultos |
| `effectListData.length/data/start/linesCount` | Init, start e controlo gráfico da lista | Em SD descreve somente as linhas visíveis; em RAM usa novamente os nomes globais do PtfxManager |
| Valor selecionado no controlo `effectList` | `refreshEffectList` | Converte o índice original para uma linha visível apenas no desenho |
| Páginas de parâmetros e More | `effectScreenPage`, `getParamsPagesCount`, `getCurrentPageParams`, `showEffectParamsScreen` | Sem alteração dos índices ou cálculo de páginas |

A pesquisa de todas as referências a `selectedEffectListIdx` encontrou-as exclusivamente no Sample Editor. Os consumidores de `currentEffectIdx` permanecem ligados às tabelas nativas.

## Implementação

### Modo e lista

`usesSdEditing` identifica um instrumento ativo cujo sample excede `EDIT_BUFF_SIZE/2`, o mesmo limite de encaminhamento RAM/SD existente. Não depende apenas de o ficheiro ter streaming ativo: samples longos residentes em RAM também são editados pelo backend SD.

`configureEffectList` percorre os IDs originais em ordem e consulta `isSdMonoEffect` e `isSdRegionEffect`, os predicados já usados pelo dispatch real de Apply e condicionados às opções compiladas. Não compara nomes nem duplica uma lista de compatibilidade por strings. A correspondência linha→ID e os nomes visíveis pertencem apenas à interface.

Ao entrar em SD com uma seleção oculta, seleciona o primeiro efeito suportado antes de resolver `currentEffect`. Seleções já válidas são preservadas, incluindo Stereo to Mono. A navegação passa pelas linhas visíveis e converte de volta para o índice original. Ao regressar a RAM, volta à lista completa, incluindo plugins, com os significados originais dos índices.

Efeitos visíveis nesta compilação SD, pela ordem CE:

1. Normalizer
2. Crop
3. Reverse
4. Amplifier
5. Overdrive
6. Bitcrusher
7. Equalizer
8. Limiter
9. Compressor
10. Fade In
11. Fade Out
12. Stereo to Mono

Delay, Chorus, Flanger, WT Smoother, Timestretch Beat, Timestretch Note e plugins Modular não aparecem no modo SD, pois não têm Apply SD implementado.

### Indicador

O indicador ocupa o canto superior direito da barra de título, substituindo o nome do instrumento somente em SD. A barra esquerda continua a usar o título original, incluindo 1/2. Os indicadores More de parâmetros não mudaram.

Usa Font4 nativa, cor `theme.red` em todo o texto e no separador. A fonte CE é ASCII e não contém o carácter `·`; a frase é composta pelos glifos nativos e um pequeno traço gráfico central, visualmente um ponto médio, sem introduzir fontes ou alterar o texto dos restantes controlos.

As métricas da fonte CE real Roboto Mono Bold 17 dão largura de 312 pixels: x=478..790, centro y=13, altura do glifo 22. O título esquerdo termina em x=279, deixando 199 pixels entre as regiões. Não ocupa a lista nem os parâmetros a partir de y=29. O indicador é ocultado ao sair do editor e no modo RAM.

## Alterações desta tarefa

- `core/interface/sampleEditor/sampleEditor.h`: estado da vista SD e controlo do indicador.
- `core/interface/sampleEditor/fSampleEdit.cpp`: seleção inicial, mapeamento de linhas, navegação e ocultação ao sair.
- `core/interface/sampleEditor/gSampleEdit.cpp`: criação e desenho do indicador.
- `tests/sd-editor-ui/extract.py`, `test.cpp`, `run.ps1`: testes das funções reais de configuração, navegação, título e páginas, com dependências gráficas simuladas.
- Este relatório.

Nenhuma alteração ao motor de áudio, transações, ficheiros, DSP, Preview ou Undo. Os ficheiros desses percursos já modificados em tarefas anteriores continuam pendentes na working tree; não representam alterações novas desta tarefa.

## Validação

- UI Host: PASS.
- UI AddressSanitizer: PASS. Testa entrada a partir de cada índice nativo/plugin, ordem, passos positivos/negativos e extremos, retorno de submenu, transição RAM/SD, limite exato de tamanho, todos os índices RAM e plugins, páginas EQ, título 1/2 e 2/2, indicador vermelho e ocultação.
- Verificação das métricas reais da fonte: PASS, limites acima.
- Host transacional completo: PASS, 473.383.777 verificações.
- ASan transacional completo: PASS, 473.383.777 verificações.
- Preview/DSP nativo com ASan: PASS, 2.983.327 verificações, incluindo 10 segundos, mono/estéreo, seleção, repetição, cancelamento e EQ/Limiter/Compressor SD equivalentes aos kernels CE.
- ARM Tracker+: compilação e link concluídos; SD Mono e SD Edits ON, sem WR2.
- `git diff --check`: PASS.

Logs: `build/sd-mode-host.log`, `build/sd-mode-asan.log`, `build/sd-mode-preview.log`, `build/sd-mode-arm-final.log`. A fixture UI extrai os corpos de produção; não é uma emulação do display EVE ou validação física.

## Memória e firmware

| Região | SDMODE2 | Diferença para SDUFIX3 |
| --- | ---: | ---: |
| QSPI | 1.712.428 B | +688 B |
| SRAM DTC | 343.336 B | +144 B |
| SRAM ITC | 114.648 B | 0 |
| SRAM OC | 23.212 B | 0 |
| SDRAM reservada | 67.096.184 B | 0 |

A adição é metadados de interface e um controlo gráfico. Sem buffers PCM adicionais; SDRAM do linker continua a representar a reserva existente, sem constituir uma medição da margem dinâmica.

PTF final: `C:\Projects\tracker-ce-sd\build\TrackerPlus_TrackerPlus_EXP_CE_SD_MODE2.ptf`
Identificação: `SDMODE2`. Tamanho: 4.857.247 bytes.
SHA-256: `F135BBA860F76E940A9E81BAEFE209D1312C4BB2E54480694B4E0C67A0D06418`.

SDUFIX3 preservado, SHA-256 confirmado: `DBAD08EA4B6F152506DCB81EB982F9245DD3EB64D6119DDBB7938CA9C64737BD`.

## Validação física pendente

Confirmar no Tracker+ a leitura do indicador e do separador central, cores/temas, ausência de sobreposição em todas as páginas, alternância entre instrumentos curtos/longos e retorno de submenus. As métricas verificadas são da fonte padrão; fontes personalizadas ainda precisam de avaliação visual. Confirmar seleção/aplicação de cada linha filtrada e um ciclo Apply→Undo/Cancel/Preview por amostragem. Não há hardware novo validado nesta tarefa.

## Git e preservação

Branch `codex/sample-editor-sd`, HEAD `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`; alterações locais não commitadas, incluindo trabalhos anteriores. Sem commit, push, merge, PR ou flash. Branch pública inalterada. `C:\Projects\tracker-ce` e firmwares anteriores preservados.
