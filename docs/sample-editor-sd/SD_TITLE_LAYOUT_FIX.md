# Correção da barra SD — SDTITLE

Data: 2026-11-09. Projeto: `C:\Projects\tracker-ce-sd`. Branch: `codex/sample-editor-sd`. Referência: SDMODE2.

## Resultado

Implementado o layout aprovado: título à esquerda, indicador vermelho imediatamente depois, nome nativo do sample à direita. Os testes de interface Host/ASan e a compilação/link ARM Tracker+ passaram. A confirmação visual física continua pendente.

## Causas corrigidas

O indicador antigo tinha y=13 e altura=26. `cLabel::update`, com CenterY, calcula o centro como y + altura/2; o texto ficava efetivamente centrado em y=26. O título nativo tem altura zero e centro y=13. Corrigido para altura zero, com centro efetivo y=13, dentro da barra.

O nome desaparecia porque a implementação SD chamava `titleBar->show(false)` e não restaurava o label nativo do instrumento. Agora chama `show(true)` e `showCurrentInstrumentOnRightLabel()` em ambos os modos.

O teste anterior verificava somente as propriedades de posição e ignorava o cálculo interno de `cLabel`. A fixture agora extrai e executa esse cálculo real; inclui a confirmação de que a configuração antiga renderizava em y=26.

## Layout e medidas

- Título: posição nativa x=9, centro y=13. Font4 original sempre que houver espaço; Font5 apenas quando o conjunto completo não cabe com a fonte original. Texto preservado, incluindo 1/2 e 2/2.
- Indicador: Font5 (Roboto Mono Regular 12), vermelho `theme.red`, largura real de 199 pixels, altura de glifo 16. Começa 8 pixels após a largura real do título.
- Nome: controlo nativo à direita, Font4 original e alinhamento original em x=790. Não há truncagem adicional, abreviação ou reposicionamento do nome. O texto considerado é o nome nativo já formatado, incluindo o prefixo do slot.
- O separador central continua a ser desenhado por uma primitiva nativa, pois as fontes CE padrão só contêm 128 glifos.

| Caso com nome nativo máximo de 326 px | Título | Indicador | Intervalos | Total útil |
| --- | ---: | ---: | ---: | ---: |
| Sample Editor 1/2 ou 2/2, Font4 | 170 | 199 | 16 | 711 / 781 px |
| Sample Editor Stereo to Mono, Font5 | 184 | 199 | 16 | 725 / 781 px |

Com nomes curtos, mesmo o título completo Stereo to Mono mantém Font4. Quando o título cabe em Font4, não se reduz a fonte. Em SD a largura do controlo acompanha o título completo, em vez de o cortar no limite fixo de 270 pixels. Em RAM restaura-se exatamente o estilo e limite nativos: Font4, largura 270, altura zero.

## Fontes personalizadas e nomes não ASCII

Se, mesmo reduzindo o título, o indicador completo não couber, o indicador é ocultado nessa apresentação. Se nem o título reduzido couber antes do nome, também é ocultado, preservando o nome nativo. Não se apresentam fragmentos do indicador nem se sobrepõe texto ao nome. Este fallback é uma limitação explícita para fontes personalizadas demasiado largas, não uma nova disposição com fontes padrão.

Nomes com bytes fora do ASCII não são usados como índices da tabela de 128 larguras da fonte. Nesses casos, reserva-se conservadoramente toda a barra para o nome nativo e omitem-se os outros textos SD. O nome mantém a apresentação nativa; não se acrescenta suporte Unicode. Uma fonte personalizada que torne o próprio nome maior do que o ecrã continua sujeita às limitações da apresentação nativa, cuja fonte e alinhamento não foram alterados.

## Ficheiros e funções desta correção

- `core/interface/core/titleBar.h`: API de medição do label direito e configuração do título; referência ao texto já usado pelo label.
- `core/interface/core/titleBar.cpp`: `setTitle` restaura o layout nativo por defeito; `setTitleLayout` permite o ajuste explícito SD; `getRightLabelWidth` mede o texto nativo; `setRightLabel` acompanha a referência existente.
- `core/interface/sampleEditor/gSampleEdit.cpp`: propriedades do indicador e `showTitleBar`, escolha da fonte, reserva do nome e fallback seguro.
- `tests/sd-editor-ui/extract.py`, `test.cpp`: métricas descomprimidas das fontes reais, corpos nativos de TitleBar e cálculo efetivo de cLabel.
- Este relatório.

Não foram alteradas a filtragem, a navegação, Apply, Undo, Cancel, Preview, DSP, áudio ou transações. Não foram introduzidas dependências WR2/CT2.

## Testes executados

- Interface Host: PASS.
- Interface AddressSanitizer: PASS.
- Métricas reais Font4/Font5; nomes curtos e nome de largura máxima; páginas 1/2 e 2/2; submenus Equalizer, Compressor e Stereo to Mono; três páginas de parâmetros EQ; manutenção ou redução condicional da fonte: PASS.
- Posição efetiva calculada por código extraído de `cLabel::update`: centro dos dois textos em y=13; caso antigo reproduz y=26: PASS.
- Texto do nome nativo, apresentação à direita, manutenção integral dos títulos com fontes padrão: PASS.
- Falta de espaço com fontes simuladas, casos extremos e bytes UTF-8 sem leitura fora dos limites: PASS.
- Regressão da lista filtrada e navegação RAM/SD no mesmo harness: PASS; IDs preservados.
- Build ARM Tracker+ e link final: PASS, GNU ARM 10.3-2021.10, SD Mono/Edits ON, sem WR2.
- `git diff --check`: PASS.

Os testes de interface simulam os comandos gráficos. Não emulam o display EVE nem substituem a observação física. Não foram repetidas suites de DSP/I/O nesta tarefa, pois esses percursos não foram alterados.

## Firmware e memória

Identificação experimental: `SDTITLE`.
PTF: `C:\Projects\tracker-ce-sd\build\TrackerPlus_TrackerPlus_EXP_CE_SD_TITLE.ptf`
Tamanho: 4.858.134 bytes.
SHA-256: `764B73C98082FB7A3FFBC4BF1B60209DCBB8648BAFF9725D8D6D21DF5B6F3616`.
Log ARM: `build/sd-title-arm.log`.

| Região | SDTITLE | Diferença SDMODE2 |
| --- | ---: | ---: |
| QSPI | 1.712.748 B | +320 B |
| SRAM DTC | 343.336 B | 0 |
| SRAM ITC | 114.648 B | 0 |
| SRAM OC | 23.212 B | 0 |
| SDRAM reservada | 67.096.184 B | 0 |

Sem novo buffer PCM. A igualdade das regiões de memória inclui alinhamentos do linker; não constitui medição de stack ou RAM livre em runtime.

## Estado e próximos passos

HEAD permanece `0ce0fcd6f3c73989e5e397888acfd8fd44fdfed2`; branch `codex/sample-editor-sd`, alterações locais não commitadas incluindo trabalhos anteriores. Sem commit, push, PR ou flash. A working tree original não foi modificada; SDMODE2 e SDUFIX3 preservados.

Validar fisicamente no Tracker+: alinhamento vertical dentro da barra, separador vermelho, nome à direita, títulos principais e submenus, nomes longos, transição RAM/SD e eventuais temas/fontes personalizados. Nenhuma nova validação física foi realizada nesta tarefa.
