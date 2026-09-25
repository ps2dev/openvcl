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
1/2/4 iteraciones en PCSX2.

El primer subgrupo de los 16 loops no-Q con CFG no simple ya está caracterizado:
los 6 `adcLoop_lid` son loops contados con cuatro salidas tempranas hacia un
único destino común. Una extensión cyclic-prefix segura necesitaría un guard
terminal; en el estado final del milestone el mejor candidato queda en 39 ciclos
frente a 35 del
loop original, por lo que el gate lo rechaza. Sí había una pérdida independiente:
al reescribir cualquier otro loop del shader se desactivaba globalmente el
relleno de branch-delay slots. Ahora los loops estructuralmente clasificados como
`forward_exit_counted_loop` rellenan sus slots seguros antes de aplicar los
rewrites del resto del programa. El fixture
`test/ps2/vu1_forward_exit_differential` pasa en PCSX2 con
`VU1_DIFFERENTIAL_PASS forward-exit rows=3 pointer=3`; la baseline completa
queda en **232541** ciclos estimados frente a **232571** tras el milestone de
ACC, y solo cambian los seis renderers que contienen `adcLoop_lid`. Invariancia
default pasa 13/13 y `general_quad` continúa 12/12 frente a Sony.

El segundo subgrupo de CFG no simple también queda cubierto sin relajar
`simpleCountedLoop`: los 10 `dir_light_loop_lid` son bucles exteriores con
exactamente un backedge interno `LoopCS`. La clasificación
`single_nested_counted_loop` exige ese CFG explícito y una inducción exterior
conocida. Para mantener el alcance demostrado, el relleno se aplica solo a los
outer loops sin productores Q.

En esos 10 casos el load de la siguiente luz ya era incondicional:
`ILW light_ptr, 0(light_ptr_ptr)` estaba antes del `ISUBIU` del contador
exterior. Si el load y ese único IALU son independientes, el compilador retrasa
el load al delay slot del backedge exterior. Sigue ejecutándose tanto al tomar
como al no tomar la rama; no se añade, elimina ni especula ningún acceso de
memoria. `indexed` conserva su `SQI++` del loop interior: no se mueve ni se usa
como prueba de la inducción exterior.

El diferencial `test/ps2/vu1_nested_loop_differential` pasa en PCSX2 con
`VU1_DIFFERENTIAL_PASS nested-loop outputs=2 pointer=3 load=333`. La baseline
`build/baselines/nested-loop-delay-002` completa los 13 renderers y las cuatro
variantes; solo cambian los 10 default con `dir_light_loop_lid`, mientras los
`pt_light_loop_lid` estructuralmente parecidos pero con Q permanecen intactos.
El agregado default baja de **232541** a **232531** ciclos estimados; no-pipeline
no cambia. La invariancia default pasa 13/13, `general_quad` pasa 12/12 frames
en las cuatro variantes y la suite queda en **446/446 tests**.

Con esto los 16 loops no-Q que antes estaban bloqueados solo por CFG no simple
ya tienen tratamiento conservador de sus oportunidades locales de branch-delay.
No se ha demostrado todavía un cyclic-prefix rentable para esos outer loops; el
siguiente trabajo debe volver a priorizar candidatos por dependencia/coste del
corpus, no por nombre de shader.

La auditoría siguiente está centrada en multi-Q. Hay dos diagnósticos opt-in:
`OPENVCL_DUMP_MULTI_Q_LOOP_MII=1` y
`OPENVCL_DUMP_MULTI_Q_KERNEL_PROBE=1`. En `indexed/xform_loop_lid` el modelo
preciso/rename-aware da `MII=52` frente a `RecMII=83`, pero el placer aún acaba
en `II=83`; a II=52 los fallos son de dependencias, no de MRT. El cyclic-prefix
forzado no debe promoverse: el VSM emitido estima 95 ciclos por iteración frente
a 94 sin rewrite. Los probes de los 13 renderers no cambian ningún VSM del
baseline `nested-loop-delay-002`. El siguiente trabajo debe investigar esa
brecha del placer con reglas generales y validación diferencial antes de relajar
dependencias o autoincrementos.

Corrección posterior del propio probe: su prioridad `ASAP/ALAP/height` usaba el
DDG grueso aunque la factibilidad ya usaba aristas precisas. Al derivar ambas del
mismo DDG, `indexed/xform_loop_lid` consigue **II=53** (límite preciso 52) y
`general/pt_light_vert_loop_lid` consigue exactamente II=114. Los 13 VSM siguen
idénticos al baseline. No convertir esta mejora diagnóstica en codegen sin una
prueba diferencial de la ruta de kernel/renombrado que se pretenda promover.

Durante esa auditoría se detectó además un bug de corrección previo: el
scheduler trataba bases VI distintas como no-alias. El nuevo diferencial
`test/ps2/vu1_memory_alias_differential` demuestra en PCSX2 que dos VI distintos
pueden apuntar al mismo QW: antes del fix el candidate devolvía `1.0` y el
baseline `42.0`; después ambos devuelven `42.0` (`42280000`). La suite host queda
verde con 447 tests y los diferenciales de inducción conocida/postincrement
siguen pasando. Esta corrección eleva el agregado estático default de 232531 a
311872 (`indexed` 29757→34457, `general_quad` 25823→36488). Prioridad siguiente:
recuperar scheduling mediante provenance/aliasing afín demostrable, no mediante
nombres de bases distintos ni special-casing de shaders.

El primer recovery afín ya está implementado de forma conservadora. El
scheduler rastrea únicamente asignaciones `iaddiu/isubiu` con inmediato desde una
misma raíz VI conocida dentro del bloque; cualquier escritura desconocida crea
una raíz nueva. Dos accesos plain solo se consideran distintos cuando comparten
raíz y su delta es no cero incluso módulo 256 QW, condición suficiente tanto
para VU0 como para VU1. La misma prueba permite un loaded multi-Q suffix drain
solo si los dos streams conservan además el mismo paso de loop demostrado.

El fixture `vu1_affine_alias_differential` fuerza el reordenamiento seguro y pasa
en PCSX2 con `VU1_DIFFERENTIAL_PASS affine-alias value=40e00000`. También siguen
pasando `memory-alias`, `alias-induction` y `postincrement-prefix`. La suite host
queda en **452/452 tests**. La baseline
`build/baselines/affine-provenance-001` completa los 13 renderers y las cuatro
variantes, pero sus **52 VSM son bit a bit idénticos** a
`alias-correctness-003`; `indexed` sigue en 34457/34458. La invariancia default
pasa 13/13. Por tanto este milestone recupera una capacidad genérica demostrada,
pero todavía no recupera scheduling en `indexed/xform_loop_lid`; el siguiente
trabajo debe identificar qué relaciones de provenance reales de ese loop quedan
sin representar.

La auditoría posterior eliminó dos fuentes artificiales adicionales antes de
interpretar esa brecha. `3c12f37` evita reutilizar VIs literales físicamente a
través de iteraciones multi-Q solapadas, y `819e59b` fusiona actualizaciones
locales de una base VI solo cuando todos los usos intermedios son accesos plain
con offsets ajustables. El antiguo bloqueo `IOR -> ILW` ya no aparece en
`xform_loop_lid`: el límite preciso/rename-aware actual es **MII=35** y el
primer bloqueo del placer es la recurrencia real de `next_output`/`VI04` entre
los `SQ` y su `IADDIU`.

`5179f67` añadió un retry failed-first estrictamente diagnóstico que conserva
todas las aristas DDG y restricciones MRT. En el corpus actual ese retry coloca
`xform_loop_lid` en **II=46** desde MII=35, frente al greedy normal que necesita
un II mayor. Esto demuestra una limitación de orden del placer, no permiso para
relajar dependencias. La baseline `build/baselines/kernel-rewrite-safety-001`
completa los 13 renderers y las cuatro variantes sin fallos; `indexed` queda en
34257/34258/34257/34257 para default/no-pipeline/kernel/expanded.

Antes de usar el layout genérico experimental como codegen se cerraron además
dos errores de emisión: una rama de backedge copiada al prólogo tenía la
polaridad equivocada, y los grids del planner podían hacer que la rama original
se filtrase dentro de prolog/main/drain. El emitter ahora exige una rama
invertible, emite explícitamente la guarda invertida del prólogo y excluye el
branch original de los tres grids. También rechaza planes cuyo pipeline base
tenga un prolog/drain semántico separado que el layout genérico aún no
represente. La suite host pasa completa. Estas restricciones no promueven una
relajación de scheduling; mantienen el generic kernel rewrite experimental
cerrado cuando no puede representar toda la semántica del plan.

La siguiente exploración del orden del placer probó dos heurísticas generales
sin tocar ninguna arista DDG ni regla MRT: priorizar nodos por presión de aristas
carried y, separadamente, invertir el desempate entre nodos con igual
height/mobility para favorecer sinks. La primera no encontró un layout factible
dentro del cap del probe; la segunda conservó exactamente **II=46**. Se
descartaron ambas del código. Con failed-first, a II=44 y 45 el único nodo que
queda sin colocar es el `SQ` de la segunda salida (token 252): sus límites son
los productores `FTOI4`/`MFIR` y el `IADDIU next_output` por las aristas
dist=0/dist=1 demostradas. A II=46 el layout coloca `MFIR`, `SQ` e `IADDIU` en
slots 68, 73 y 74 respectivamente. Por tanto el siguiente experimento debe
trabajar sobre la **selección de slot legal** del placer (no solo el orden de
nodos), manteniendo esas aristas intactas; en particular conviene comparar
earliest-slot con una selección dirigida por slack/vecindad ya colocada.
### 2026-09-25: slot-selection follow-up

The next generic modulo-placement experiments did not improve the indexed
transform loop and were discarded from the implementation. Recomputing
ASAP/ALAP/priority after every II bump was substantially slower and did not
produce a useful result in practical time; the current placer intentionally
keeps the priority computed at the initial MII. Two deterministic slot-order
experiments were also negative: preferring candidates close to already-placed
DDG neighbours kept `xform_loop_lid` at II=46 while making
`pt_light_vert_loop_lid` much worse (II=114), and modulo-neighbour proximity
made `xform_loop_lid` infeasible within the probe cap. Tightening the candidate
range to the exact bounds implied by already-placed neighbours was neutral:
`xform_loop_lid` remained II=46 and `pt_light_vert_loop_lid` remained II=114.
No experimental code from these attempts is retained. The remaining useful
direction is therefore bounded local repair/backtracking around the failed
SQ/MFIR/IADDIU neighbourhood, rather than another global priority or slot-order
heuristic; any such repair must continue to validate the complete DDG and MRT.

The bounded local-repair follow-up was also tested diagnostically. For each
failed node the placer copied the current MRT, released every pair of placed
incident DDG neighbours, and tried all six orders of the failed node plus that
pair while rechecking every DDG edge and MRT reservation. No pair repaired the
indexed transform loop below II=46. The test was then widened to the complete
three-neighbour incident set (24 deterministic permutations); that also found
no valid repair at II=44 or II=45. The experimental implementation was removed.
This is useful negative evidence: the final II=46 gap is not just a greedy
placement accident confined to the immediate FTOI4/MFIR/IADDIU neighbourhood.
Further work should inspect constraints/resource occupancy beyond one DDG hop,
or use a bounded exact feasibility search, rather than adding more local greedy
reordering.

### 2026-09-25: exact finite-domain feasibility

A diagnostic finite-domain solver now consumes the precise placement problem
exported by `OPENVCL_DUMP_MODULO_FEASIBILITY_PROBLEM=1`. The problem is binary:
each node has the placer's finite `[lo, hi]` slot domain; DDG edges impose
`slot[to]-slot[from] >= latency-distance*II`; nodes sharing an MRT resource have
pairwise modulo-slot/throughput exclusions. `tools/solve_vu_modulo_feasibility.py`
enforces arc consistency (AC-3), selects the minimum remaining domain, orders
values by least-constraining pressure, and performs bounded backtracking.

The solver first reproduced a valid II=46 witness, then found **SAT at every II
from 45 down through the precise renamed MII=35** for `xform_loop_lid`. A later
bounded-stage pass strengthened that result: with every node constrained below
`2*II`, **all II=35..46 remain SAT**, including II=35 in only 45 search states
and 2212 AC-3 revisions. The greedy II=46/47 results are therefore not lower
bounds imposed by the current DDG or MRT model. At II=35 the ordinary greedy
layout places 61/62 nodes and leaves node 60 (`IADDIU next_output`) unplaced,
but the exact two-stage witness changes the slots of 39 already-placed nodes.
This is a global coordinated-placement failure rather than a local
FTOI4/MFIR/SQ/IADDIU repair opportunity. A dynamic MRV-only greedy experiment
was also tried and discarded; without backtracking it reached dead ends and
found no layout within the probe cap.

The checked-in Sony VCL 1.4beta7 reference for the same loop reports
`optimal=38` for both `ldumb` and `vuta`, and emits a 38-row MAIN loop. That is
currently the strongest practical target: our simplified finite model can fit
II=35, but Sony choosing 38 is evidence that full emission/pipeline constraints
or optimization objectives not yet represented by the probe may matter. The
next scheduler experiment should therefore port the successful CSP ideas
(propagation + bounded backtracking) into a generic C++ placer path, initially
diagnostic, and compare II=38/35 candidates through the complete kernel rewrite
and runtime validation rather than assuming the lowest model-only II is safe.

That solver has now also been ported to C++ as `VuModuloFeasibilitySolver` and
is available behind `OPENVCL_EXPERIMENT_EXACT_FEASIBILITY_PLACER=1` in the
precise rename-aware multi-Q probe. With
`OPENVCL_EXACT_FEASIBILITY_MAX_STAGES=2`, `indexed/xform_loop_lid` independently
reproduces the bounded Python result: **II=35, stageCount=2, conflicts=0, 45
search states, 2212 AC-3 revisions**, with no II bumps and a 140-token kernel
image. The integration rebuilds an independent `ModuloReservationTable` from
the witness and rechecks every DDG inequality before accepting it. The
implementation has unit coverage for ordinary DDG ordering, MRT collisions,
and carried self edges. It remains diagnostic: the production generic rewrite
currently builds its normal opportunity through the non-precise path, while
this lower-II result belongs to the separate precise multi-Q probe. The next
integration question is therefore not whether II<46 exists (it does), but how
to use exact/global placement ideas in the real rewrite while preserving the
complete prolog/drain and rename semantics. Sony VCL 1.4beta7 remains useful as
a scheduler-design reference: its checked-in result is 38 rows, while the
simplified exact OpenVCL model can fit 35, so reverse engineering should focus
on candidate generation/search and the full emission constraints rather than
assuming Sony merely drops a dependency that OpenVCL keeps.

Directed static analysis of the public Sony VCL 1.4beta7 x86 binary now maps
the core loop driver around `Opt::Loop` and its candidate pipeline. The driver
starts from a minimum loop size, applies the LoopExtra/LoopAbs bounds, builds a
work table, tries several internal strategies (`ldumb`, `vuta`, `hDown`,
`normal*`, etc.), and runs loop generation followed by stats, reduction,
move/post-opt validation and register assignment. If no strategy produces a
complete candidate, it increases the candidate loop size; once a size has a
valid candidate it selects the best strategy for that size and finalizes it.
This makes Sony's 38-row result consistent with OpenVCL's model-only II=35:
35--37 can be DDG/MRT-feasible yet fail a later full-pipeline constraint. The
detailed address/evidence map is in
`reports/2026-09-25-sony-vcl-loop-re.md`. The next investigation should identify
which full-pipeline phase first separates 35--37 from 38, then use that result
to define the constraints and search policy of a generic global OpenVCL placer.
