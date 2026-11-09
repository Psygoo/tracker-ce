# Indicador SD — revisão de layout antes da correção

Referência: SDMODE2. Data: 2026-11-09. Nenhum código alterado nesta revisão; nenhum PTF novo gerado.

## Causas confirmadas no código

O controlo nativo do título usa y=13 e altura=0. Em cLabel::update, CenterY calcula a posição do texto como y + altura/2. SDMODE2 criou o indicador com y=13 e altura=26, deslocando o centro para y=26. Logo o indicador foi efetivamente desenhado 13 pixels abaixo do título. O teste anterior verificava apenas y=13 da propriedade, ignorando a transformação interna de cLabel; isso explica a falha de cobertura.

O nome desapareceu por decisão da implementação anterior: showTitleBar chamava titleBar->show(false) em SD e não chamava showCurrentInstrumentOnRightLabel. A correção deve manter os dois controlos nativos ativos.

## Métricas reais das fontes CE

Barra útil x=9..790: 781 pixels. Font4 título/nome: Roboto Mono Bold 17. Indicador em Font5: Roboto Mono Regular 12, largura 199 pixels, altura 16. Título Sample Editor 1/2 ou 2/2: 170 pixels. Maior nome nativo formatado em 30 caracteres: 326 pixels. Dois intervalos de 8 pixels.

- Página normal: 170 + 199 + 326 + 16 = 711 pixels. Cabe com 70 pixels de margem.
- Submenu Stereo to Mono: título limitado nativamente a 270 pixels; 270 + 199 + 326 + 16 = 811 pixels. Não cabe em 781. O título completo teria 280 pixels.
- Mesmo título de submenu em Font5: 184 + 199 + 326 + 16 = 725 pixels. Cabe sem truncar texto ou alterar o nome.

## Proposta segura pendente de decisão

Restaurar o nome nativo com o seu alinhamento à direita e fonte original. Desenhar o indicador em Font5, a vermelho, imediatamente após a largura efetiva do título, com centro vertical real y=13. Preservar a fonte normal do título quando tudo couber. Somente nos submenus em que não houver espaço, reduzir também o título para Font5, mantendo o texto completo. Não alterar nomes, IDs, navegação ou processamento.

É necessário aprovar esta adaptação adicional do título, pois o pedido permitia explicitamente reduzir o indicador, mas não definia a redução do título. Fontes personalizadas podem exigir ainda outra política caso nem a fonte menor permita os três textos completos.

## Próximos testes

Testar a posição final calculada por cLabel, e não apenas as propriedades. Exercitar nomes curtos e de largura máxima, títulos principais e todos os submenus, RAM/SD, clipping, nome nativo e ordem de desenho. Compilar novo PTF após definir a política de espaço insuficiente.

Branch codex/sample-editor-sd; sem commit, push ou PR. SDMODE2, SDUFIX3 e a working tree original preservados.
