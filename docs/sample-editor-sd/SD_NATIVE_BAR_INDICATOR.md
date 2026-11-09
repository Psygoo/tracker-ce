# Barra nativa e indicador independente — SDIND

Data: 2026-11-09. Projeto: `C:\Projects\tracker-ce-sd`; branch `codex/sample-editor-sd`. Referências preservadas: SDUFIX3, SDMODE2 e SDTITLE.

## Resultado

Restaurada a implementação original da barra. O indicador SD é um controlo independente, vermelho, sem modificar fontes, dimensões, posição, texto ou visibilidade dos controlos nativos. Host, ASan e compilação/link ARM Tracker+ passaram. Esta versão ainda não foi observada fisicamente no Tracker+.

## Causa raiz comprovada

SDTITLE introduziu `TitleBar::setTitleLayout()`. Esse método chamava `display.setControlStyle()` com uma máscara sem `controlStyleShow`. O percurso era:

1. `TitleBar::show(true)` ativava o bit Show do título.
2. `setTitle()` chamava `setTitleLayout()`, substituindo toda a máscara e apagando Show.
3. `cDisplay::step3()` só inclui controlos com Show na lista final de comandos; o título era omitido em RAM e SD.

A fixture anterior guardava texto e coordenadas, mas não representava a máscara de visibilidade nem executava o compositor. Por isso passou apesar de o título não ser desenhado. A nova fixture reproduz exatamente essa sequência defeituosa e verifica que o comando do título desaparece; com o código nativo restaurado, o comando volta a existir.

Foi identificada outra condição relevante: o display compõe os controlos por ordem inversa da criação. Criar um indicador na entrada do editor pode colocá-lo atrás do fundo opaco da barra nativa. A ordem de chamadas de refresh não altera esta ordem de composição.

## Restauração nativa

`core/interface/core/titleBar.cpp` e `.h` foram restaurados byte a byte do HEAD `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`. O corpo de `cSampleEditor::showTitleBar()` também é exatamente o original. Os testes verificam essas identidades automaticamente.

Foram removidos `setTitleLayout`, `getRightLabelWidth`, o estado `rightText` e a redução/ocultação do título nativo. O nome volta a ser exclusivamente desenhado pelo código nativo, com o seu formato, fonte e alinhamento originais. Títulos 1/2 e 2/2 são passados sem alteração.

A lista filtrada e os IDs originais continuam como em SDMODE2. Nenhum percurso DSP, Apply, Undo, Cancel, Preview, streaming ou transação foi alterado nesta tarefa.

## Indicador independente

Texto visual: `LONG SAMPLE · SD STREAMING MODE`, todo a vermelho. O espaço central é preenchido por uma pequena primitiva gráfica vermelha porque a fonte CE padrão não contém o glifo Unicode do ponto médio. O controlo usa Font5 nativa, 199 pixels de largura e 16 de altura com a fonte padrão.

- Quando cabe: começa 8 pixels depois do título nativo, centro y=13, com 8 pixels de segurança antes do nome à direita. O título e o nome não são deslocados nem reduzidos.
- Quando não cabe na barra: no ecrã principal, x=9, centro y=48, na zona preta acima da waveform. Nos Settings, onde a waveform está oculta, x=409 e centro y=48, na área vazia à direita das quatro colunas de parâmetros. Não ocupa barras, listas ou botões.
- RAM: o indicador fica oculto.
- Saída do editor: fica oculto no stop existente.
- Alteração de instrumento e regresso de operações/submenus: os pontos de atualização dos ecrãs e da lista recalculam a posição e a visibilidade.

O controlo é reservado no arranque imediatamente antes da criação da barra nativa, permanecendo propriedade do Sample Editor e oculto nos restantes menus. Assim é composto depois do fundo da barra e dos fundos locais do editor. Os popups globais, criados antes, continuam à frente. Não foi alterado o compositor, a ordem relativa dos controlos nativos ou a implementação de TitleBar. A reserva utiliza um slot de controlo persistente; não acrescenta buffers PCM.

A medição do nome apenas reproduz, num buffer local, o formato nativo já usado para o desenho, incluindo o prefixo do slot. Não substitui o nome nativo. Nomes com bytes fora do ASCII escolhem conservadoramente a segunda posição, evitando consultar a tabela de 128 glifos com bytes UTF-8. Fontes personalizadas que tornem o indicador maior que 380 pixels ou mais alto que 26 pixels fazem ocultar o indicador para evitar sobreposição/clipping; os elementos nativos mantêm o comportamento CE.

## Evidência gráfica executada

A fixture extrai e compila os corpos reais de:

- `cLabel::setStyle()` e `cLabel::update()`;
- `cDisplay::setControlStyle`, `setControlShow`, `setControlHide` e `step3`;
- inicialização, show, setTitle e formatação do nome em TitleBar;
- showTitleBar nativo e os dois métodos do indicador independente.

As chamadas gráficas são registadas; os buffers de comandos dos controlos são compostos pela ordem do código nativo. Os testes verificam que textos existem no frame final, têm Show ativo, estão dentro do ecrã, não se sobrepõem e não são cobertos por retângulos opacos desenhados depois. O driver e o armazenamento dos comandos são simulados: isto não é uma emulação pixel a pixel do EVE.

Exemplos capturados da lista final (coordenada y é o topo efetivo do glifo):

| Cenário | Texto | x | y | Largura | Fonte |
| --- | --- | ---: | ---: | ---: | --- |
| RAM/SD | Sample Editor 1/2 | 9 | 2 | 170 | Font4 nativa |
| RAM/SD, nome curto | 48. sample | 690 | 2 | 100 | Font4 nativa |
| SD, cabe na barra | LONG SAMPLE · SD STREAMING MODE | 187 | 5 | 199 | Font5 vermelha |
| RAM/SD, nome máximo | prefixo 48 + nome de largura máxima | 464 | 2 | 326 | Font4 nativa |
| SD, submenu longo/nome máximo | indicador na zona preta | 9 | 40 | 199 | Font5 vermelha |

As duas fontes têm centros efetivos y=13 na barra. No fallback, o indicador tem centro y=48. Os testes incluem páginas 1/2 e 2/2, submenus, três páginas de parâmetros EQ, slots 1 e 48, nomes curtos e máximos, RAM/SD, entrada e saída. As chamadas de texto do frame RAM correspondem às chamadas exclusivamente nativas, sem comando do indicador.

O limite nativo de 270 pixels do título continua intacto. Por exemplo, o CE original já corta a última letra do título longo `Sample Editor Stereo to Mono` com a fonte padrão. Esse clipping nativo não foi corrigido nem agravado, porque a tarefa exige restaurar exatamente os controlos originais. O indicador novo não é cortado; títulos 1/2 e 2/2 e os nomes nativos máximos testados ficam visíveis.

## Testes e build

- Host de navegação e gráficos: PASS.
- AddressSanitizer de navegação e gráficos: PASS.
- Contraexemplo SDTITLE do bit Show: reproduzido; restauração nativa verificada.
- Escolha explícita da primeira posição e fallback: PASS.
- Comandos do texto e do separador a vermelho: PASS.
- Fundos da barra e da área direita dos parâmetros sem ocultação do indicador: PASS.
- Verificação byte a byte dos ficheiros TitleBar e comparação do corpo de showTitleBar com HEAD: PASS.
- `git diff --check`: PASS.
- Build ARM Tracker+, GNU Arm 10.3-2021.10, SD Mono/Edits ON, sem WR2: PASS, link concluído.

O primeiro build identificou falta de `<cstdio>` no novo helper; o include foi corrigido e a compilação foi repetida com sucesso. Mantêm-se avisos preexistentes, incluindo índices dos arrays label/bar no construtor do editor. Não foram alterados nesta correção visual.

Logs: `build/sd-indicator-ui-host.log`, `build/sd-indicator-ui-asan.log`, `build/sd-indicator-arm.log`.

Não foram repetidas suites DSP/I/O completas, pois os respetivos percursos não foram alterados nesta tarefa. A navegação filtrada foi reexecutada com os corpos reais das funções existentes.

## Ficheiros desta tarefa

- `core/interface/core/titleBar.cpp` e `.h`: restauração ao HEAD original; sem diff Git final.
- `core/interface/core/interface.cpp`: reserva do controlo independente antes da barra nativa.
- `core/interface/sampleEditor/sampleEditor.h`: declaração dos dois métodos independentes.
- `core/interface/sampleEditor/gSampleEdit.cpp`: showTitleBar restaurado; indicador independente e atualização nos ecrãs.
- `core/interface/sampleEditor/fSampleEdit.cpp`: atualização independente após o refresh da lista/título. Filtragem e navegação inalteradas.
- `tests/sd-editor-ui/extract.py`, `test.cpp`, `graphics.cpp`, `run.ps1`: comandos gráficos, visibilidade, composição e regressão de navegação.
- Este relatório.

## Memória e PTF

Identificação: `SDIND`.
PTF: `C:\Projects\tracker-ce-sd\build\TrackerPlus_TrackerPlus_EXP_CE_SD_INDICATOR.ptf`
Tamanho: 4.858.176 bytes.
SHA-256: `40BB1CCD98B17E4E3D3ABE8F5F0B7581339BCE9CEBB04292755C5F3D99935F16`.

| Região | Utilização | Diferença SDTITLE |
| --- | ---: | ---: |
| QSPI | 1.712.756 B | +8 B |
| SRAM DTC | 343.336 B | 0 |
| SRAM ITC | 114.648 B | 0 |
| SRAM OC | 23.212 B | 0 |
| SDRAM reservada | 67.096.184 B | 0 |

O linker não mede o heap ou a margem dinâmica em runtime. O controlo independente ocupa um slot da tabela de 65 controlos e permanece reservado, oculto nos outros menus; o número de indicadores durante o Sample Editor continua a ser um. Sem novo buffer de áudio.

## Preservação e Git

Branch `codex/sample-editor-sd`, HEAD `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`. Alterações locais não commitadas incluem tarefas anteriores. Sem commit, push, merge, PR ou flash. A working tree original `C:\Projects\tracker-ce` não foi alterada.

Hashes das referências, confirmados inalterados:

- SDUFIX3: `DBAD08EA4B6F152506DCB81EB982F9245DD3EB64D6119DDBB7938CA9C64737BD`.
- SDMODE2: `F135BBA860F76E940A9E81BAEFE209D1312C4BB2E54480694B4E0C67A0D06418`.
- SDTITLE: `764B73C98082FB7A3FFBC4BF1B60209DCBB8648BAFF9725D8D6D21DF5B6F3616`.

## Validação física pendente

Confirmar no Tracker+ título e nome em RAM e SD, centro vertical dentro da barra, primeira posição com nomes curtos, fallback com submenu/nome longo, Settings e respetivas páginas, regresso ao editor e saída para outros menus. Verificar a leitura do ponto médio e eventuais fontes personalizadas. Os testes agora verificam comandos e composição efetiva, mas não demonstram o resultado final no controlador/display físico.
