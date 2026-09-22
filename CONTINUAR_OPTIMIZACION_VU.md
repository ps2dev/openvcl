# Prompt breve para GPT-5.6 Sol, esfuerzo High

Continúa el trabajo en `/Users/fjtrujy/Projects/openvcl`, rama `optimizations`.
Lee `PLAN_OPTIMIZACION_VU.md`, `reports/2026-09-22-linea-base.md` y
`tools/README.md`. Conserva cualquier cambio existente; en particular había
cambios del usuario en `/Users/fjtrujy/Projects/ps2gl/CMakeLists.txt`. MASP está en
`/Users/fjtrujy/Projects/masp` y PS2DEV en
`/Users/fjtrujy/toolchains/ps2/ps2dev`.

Fase 0 completada: la línea base original tenía 424 tests OpenVCL y 5 grupos
MASP; la suite actual está en 436 tests. Los 13 pares Sony y
52 candidatos ensamblan; 52 pruebas de renombrado de etiquetas pasan. Línea
base reproducida en `build/baselines/verified`. Genérico: 265450 ciclos
ponderados estimados frente a 100870 Sony. Kernel experimental: 267740;
no activarlo.

Fase 1 en curso: el parser de costes estricto está corregido en `5516c71`;
los 65 informes guardados conservan exactamente sus métricas. Existe
`test/ps2/vu1_differential/`, cuyo ELF se ejecuta automáticamente en PCSX2 y
compara memoria VU1 entre la ruta pipeline y
`--disable-generic-software-pipelining`. `general_quad` dispone además de
`tools/validate_ps2gl_general_quad.py`: 12 frames deterministas del renderer
`linear, quads` coinciden en estado VU1 semántico entre OpenVCL actual, no-Q
desactivado, pipeline genérico desactivado y Sony.
El dump de loops publica 81 candidatos: **35 sin Q y 46 con Q**. La ruta
cyclic-prefix ya acepta **19** de los no-Q, incluido ahora
`indexed/dir_light_vert_loop_lid`: su `SQI` postincrementado permanece fuera del
prefijo clonado y un harness diferencial en PCSX2 valida salida bit a bit y
puntero final (`pointer=36`). Los 16 no-Q restantes son no simples y ya están
separados por CFG: 6 `adcLoop_lid` tienen cuatro salidas tempranas; 10
`dir_light_loop_lid` son bucles exteriores con un backedge anidado. No quitar
`simpleCountedLoop` para cubrirlos: requieren soporte explícito multi-exit o
nested-loop.
`tools/analyze_ps2gl_no_q_rewrites.py`
ha revelado dos sesgos estáticos: hasta 4 ciclos entre el `bestCycles` aislado
del selector y el loop realmente emitido, y 6 ciclos de cola de salida que el
coste por label atribuye al loop. El diagnóstico `bestCyclesInContext` conserva
el prólogo y reduce el primer error a como máximo 1 ciclo sin afectar decisiones:
en `general`, dir-light 63 -> 46 aislado -> 50 contexto -> 50 emitido -> 56
bloque; final 13 -> 7 -> 10 -> 11 -> 17. `general_quad` y el harness base ya
tienen runtime reproducible en PCSX2. El bloqueo global
`multi_store_loaded_suffix_stream` ya no es necesario para streams separados:
la primera iteración pelada hace ahora el handoff de las rotaciones cyclic-prefix
antes de consumirlas. `general_quad` pasa de nuevo 12/12 frames contra Sony con
el rewrite activo. `read_write_memory_stream_conflict` sigue protegiendo streams
load/store con la misma base. Prohibidos atajos por
nombres, hashes o shaders conocidos. Distingue estimaciones de tiempos reales.

La comprobación de alias de los suffix-store drains entiende ahora inducciones
self-`iaddiu`/`isubiu` con paso constante entre dos accesos a la misma base. El
offset del acceso posterior se traduce al espacio de direcciones que existía en
el store retrasado antes de decidir si pueden aliasar. Un fixture diferencial
dedicado (`test/ps2/vu1_alias_induction_differential`) compara en PCSX2 ocho
qwords y el puntero final: `VU1_DIFFERENTIAL_PASS alias-induction rows=8
pointer=40`. En ese milestone el corpus quedó en **258796** ciclos ponderados, los 19 rewrites
no-Q siguen aceptados y la invariancia de labels pasa en las cuatro variantes.
`indexed/xform_loop_lid` está ahora cubierto mediante una transformación
específica pero general por forma semántica: cuando existe exactamente un `LQI
(base++)` dentro del prefijo y el branch invertible lee ese mismo `base`, sin
ninguna otra escritura ni lectura posterior de ese registro antes del branch, el
prólogo conserva el `LQI`, la copia
steady-state usa `LQ 0(base)` y el `+1` se materializa en el delay slot del
branch original. Un guard invertido antes del prefijo especulativo evita leer o
incrementar más allá de la última iteración. El fixture
`test/ps2/vu1_postincrement_prefix_differential` compara memoria y puntero para
1, 2 y 4 iteraciones en PCSX2 y da
`VU1_DIFFERENTIAL_PASS postincrement-prefix cases=1,2,4`.
El dump JSON expone explícitamente
`cyclic_prefix_defers_postincrement`, token y registro para auditar esta ruta.
Los 13 renderers vuelven a compilar/ensamblar, la invariancia de labels default
pasa 13/13 y la estimación ponderada default baja a **258325** ciclos
(Sony: 100870). `indexed` queda en **31874** frente a **32340** sin pipeline.
Estas cifras siguen siendo estimaciones estáticas, no rendimiento medido.

El cost-model ya corrige la ponderación de las colas de salida. Un branch que
vuelve a la etiqueta de su propio bloque conserva sus delay slots dentro del
steady-state repetido y las filas unlabeled posteriores pasan a un bloque
interno `__FALLTHROUGH` con repeat=1. Los 19 rewrites no-Q coinciden exactamente
con la caracterización independiente tanto en steady-state como en tail
(errores máximos 0/0). Los 52 VSM de default/no-pipeline/kernel/expanded son bit
a bit idénticos al baseline anterior y los 13 default siguen pasando invariancia
de labels; solo cambia la métrica. El agregado default baja de 258325 a
**242881**, no-pipeline de 297385 a **279565**, Sony permanece en 100870.

La regresión de rentabilidad ya está cerrada con un gate **general** y
conservador: el selector multi-Q conserva su búsqueda por coste aislado, pero el
plan solo se promueve si el main emitido en contexto de su prólogo tiene un coste
estimado estrictamente menor que el loop original. Si el steady-state no mejora,
ningún coste one-shot de prólogo/drain puede amortizarse para ningún trip count,
por lo que no hace falta asumir 100 iteraciones ni conocer el nombre del shader.
El dump expone `original_main_estimated_cycles` y
`candidate_main_in_context_estimated_cycles`, y
`OPENVCL_DISABLE_PIPELINE_PROFITABILITY_GATE=1` queda limitado a diagnósticos y
fixtures semánticos. El corpus pasa de **242881** a **241331** ciclos ponderados
default; `indexed` vuelve a **30558**, igual que no-pipeline, y los tres
`general_nospec*` evitan también un rewrite cuyo main estimado era 77 frente a
75. Los 13 renderers compilan/ensamblan, invariancia default pasa 13/13,
`general_quad` sigue 12/12 contra Sony y el fixture postincrement sigue pasando
1/2/4 iteraciones en PCSX2. El siguiente trabajo puede retomar los 16 loops no-Q
con CFG no simple mediante soporte explícito multi-exit/nested-loop; no relajar
`simpleCountedLoop` globalmente.
