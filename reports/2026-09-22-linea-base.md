# Ejecución inicial del plan VU

Fecha: 2026-09-22. Rama OpenVCL: `optimizations`.
Commit de partida: `fd653c4588b4bde97a753b4d269a84338f69603b`.
MASP: `ddb4fa5fb1546e74662979a4e417217c27201c3f`.
ps2gl: `881fbae441c69ef1c8b40316e4054b670d364abc` más el cambio de dependencias
CMake descrito abajo. Las modificaciones quedan sin commit.

## Estado

Fase 0 ejecutada: herramientas probadas, 13 referencias y 52 candidatos
ensamblados, procedencia abierta verificada, comparación de cuatro modos y
regeneración incremental comprobada. Inicio de fases 1/2: auditoría del modelo
y diagnóstico de los bucles. El primer fallo de la fase 1, que permitía omitir
instrucciones desconocidas del coste, ya está corregido y cubierto por pruebas.
También existe ya un primer harness diferencial VU1 ejecutable para comparar el
pipeline genérico con la ruta conservadora y se ha ejecutado correctamente en
PCSX2. **No se ha modificado el optimizador, no hay aún una nueva mejora de
rendimiento ni equivalencia funcional con Sony demostrada.**

## Cambios

- `tools/ps2gl_baseline.py`: ejecución reproducible, directorios nuevos, rutas
  explícitas, ambiente limpio de opciones experimentales heredadas, hashes,
  comandos, intermedios, costes por bloque y tamaños de `.vutext`.
- `tools/check_ps2gl_label_invariance.py`: renombra todas las etiquetas de
  entrada, deshace nombres en la salida y compara instrucciones; también
  ensambla los resultados. Comprueba integridad de los artefactos usados.
- `tools/README.md`: reproducción y límites de la validación.
- `.gitignore`: excluye builds `build-*` y caché Python de las herramientas.
- En **ps2gl**, `CMakeLists.txt` ahora depende de los ejecutables VCL/MASP/DVP,
  macros `.i`, cabeceras `.h` transitivas y compilador del preprocesado.
  La ruta abierta usa además `${DVP_AS_FOUND}` de manera consistente.
  La dependencia conservadora en todas las macros/cabeceras puede recompilar
  más shaders de los estrictamente necesarios, evitando resultados obsoletos.
- MASP no requiere cambios de código en esta fase.
- `test/ps2/vu1_differential/` añade un microprograma terminante y un ELF que
  ejecuta, desde el mismo estado de memoria, la ruta genérica y la misma entrada
  compilada con `--disable-generic-software-pipelining`. Compara los qwords de
  salida bit a bit y además comprueba valores esperados independientes.

## Validación ejecutada

- OpenVCL: **423 casos, 0 fallos, 0 casos conocidos pendientes, 4489 aserciones**.
- MASP: **5/5 grupos CTest**. Compilación Release sin ASAN; no se ha ejecutado
  todavía una nueva pasada con sanitizadores. Hay avisos existentes de C antiguo.
- Dos ejecuciones limpias de la matriz reproducen exactamente los **65 informes
  de coste** (13 referencias + 52 candidatos), descontando las rutas de archivos.
- Los 13 VSM de CMake abierto son idénticos a otra compilación con
  `--disable-known-loop-optimizations` explícito. `general_quad` procede de VCL.
- **52/52 pruebas de renombrado de etiquetas pasan**, en los cuatro modos.
- La regeneración incremental se probó en una copia aislada de ps2gl y las
  herramientas: sin cambios, 0 shaders recompilados; al tocar OpenVCL, MASP,
  una macro, `vu1_context.h` o el ensamblador, 13 etapas correspondientes
  regeneradas en cada caso. No se tocaron tiempos de archivos del usuario.
- Compilan `box.elf`, `nehe_lesson04.elf` y `nehe_lesson05.elf` tanto con
  OpenVCL puro como con Sony; existe `libps2stuff.a` en el SDK instalado.
- PCSX2 v2.9.74 ejecuta el harness diferencial de forma headless. El ELF emite
  `VU1_DIFFERENTIAL_PASS rows=4` por el `stdout` normal de PS2SDK (`tty0:`) y el
  runner devuelve 0 tras comparar bit a bit la memoria VU1 de ambas rutas y
  comprobar además los valores esperados. Esto valida esa fixture concreta; no
  establece equivalencia con Sony ni rendimiento.
- `test/ps2/vu1_differential/run_pcsx2.py` crea un `datapath` temporal, habilita
  logging de consola system/EE/IOP en una copia de `PCSX2.ini`, reutiliza la ruta
  de BIOS existente y termina PCSX2 al observar el marcador de `stdout`, sin
  modificar la configuración del usuario. `EnableIOPConsole` es necesario para
  hacer visible `tty0:`; no hacen falta llamadas `sio_puts` en el ELF.
- La observación visual inicial de `general_quad` en `box.elf` sugería una imagen
  más oscura con OpenVCL que con `--disable-generic-software-pipelining` o el VSM
  Sony. Esa conclusión queda **superada por un oráculo runtime determinista**:
  `tools/validate_ps2gl_general_quad.py` construye OpenVCL actual, no-Q
  desactivado, todo el pipeline genérico desactivado y la referencia Sony, fuerza
  el renderer `linear, quads` y ejecuta 12 frames con cámara/luces fijas por
  secuencia. Los cuatro producen exactamente el mismo hash semántico de VU1 en
  los 12 frames. La única diferencia de memoria completa está en qwords 4..8,
  que contienen punteros EE/estado de transporte dependiente del layout del ELF;
  qwords 0..3 y 9..1023 coinciden. Por tanto las capturas animadas anteriores no
  constituyen evidencia de una regresión semántica actual de `general_quad`.

## Costes obtenidos

Los números siguientes son estimaciones estáticas con el preset de etiquetas
`ps2gl` (100 repeticiones). **No son ciclos medidos por draw call ni FPS**.
No normalizan por sí solos luces, tamaños de lote, caminos cortos o trabajo por
iteración. La comparación dinámica sigue pendiente.

| Renderer | Sony | Genérico actual | Sin pipeline | Kernel experimental | Kernel expandido |
|---|---:|---:|---:|---:|---:|
| fast | 1799 | 3064 | 3821 | 4162 | 4162 |
| fast_nolights | 1327 | 1542 | 3207 | 1932 | 1932 |
| general | 9050 | 25017 | 29599 | 26574 | 26574 |
| general_nospec | 6494 | 16923 | 19749 | 18486 | 18486 |
| general_nospec_quad | 9004 | 20804 | 23395 | 20804 | 20804 |
| general_nospec_tri | 7813 | 18175 | 20795 | 18175 | 18175 |
| general_pv_diff | 9035 | 25018 | 29600 | 26575 | 26575 |
| general_pv_diff_quad | 11865 | 29303 | 33646 | 29303 | 29303 |
| general_pv_diff_tri | 10353 | 26270 | 30646 | 26270 | 26270 |
| general_quad | 11560 | 28898 | 33245 | 28898 | 28898 |
| general_tri | 10367 | 26269 | 30645 | 26269 | 26269 |
| indexed | 10118 | 32345 | 32340 | 32345 | 32345 |
| scei | 2085 | 5168 | 6697 | 5142 | 5142 |

| Métrica agregada | Sony | Genérico actual |
|---|---:|---:|
| Ciclos estáticos | 6308 | 5593 |
| Ciclos estimados | 6820 | 6049 |
| Estáticos ponderados | 100358 | 238639 |
| Estimados ponderados | 100870 | 258796 |

El resultado actual sustituye las cifras históricas del README como línea base
para esta rama, sin modificar todavía ese documento. Habilitar los drains
multi-store seguros reduce la estimación ponderada del camino genérico de
265450 a 258796 ciclos (-6654) respecto a la medición anterior. Es una mejora
**solo del modelo estático**; no equivale a ciclos medidos ni FPS. La variante
kernel/expandida sigue siendo experimental y **no se debe activar por defecto**.

Los bytes `.vutext` medidos en los 13 objetos suman 41104 para el modo genérico
y 50528 para Sony. El objeto genérico mayor ocupa 4080 bytes; la alineación del
ensamblador explica diferencias frente a `static_cycles * 8`. Esto solo mide
secciones, no valida todo el protocolo MPG/carga de microprogramas.

## Diagnóstico que guía el siguiente trabajo

| Caso representativo | Bloque | Sony estimado | Genérico estimado |
|---|---|---:|---:|
| general | dir_light_vert_loop_lid | 18 | 56 |
| general | final_loop_lid | 5 | 17 |
| general | pt_light_vert_loop_lid | 39 | 131 |
| general | xform_loop_lid | 22 | 43 |
| general_quad | xform_loop_lid | 47 | 82 |
| indexed | dir_light_vert_loop_lid | 18 | 71 |
| indexed | pt_light_vert_loop_lid | 39 | 151 |
| indexed | xform_loop_lid | 38 | 100 |

1. **Bucles sin Q parcialmente cubiertos, ahora medibles.** En
   `findVuLoopPipelineOpportunities`, `qProducerCount == 0` sigue descartando
   el bucle para el planner Q, pero `buildGenericCyclicPrefixRewritePlan` ya
   transforma un subconjunto de bucles simples. En los 13 VCL preprocesados de
   ps2gl aparecen **81 candidatos: 35 sin Q y 46 con Q**. De los 35 sin Q, la
   ruta cyclic-prefix acepta actualmente **19 rewrites**: nueve
   `dir_light_vert_loop_lid`, nueve `final_loop_lid` y el
   `indexed/dir_light_vert_loop_lid` con `SQI` postincrementado. Los **16** no-Q
   restantes son bucles no simples, pero ya no forman un grupo ambiguo: los
   **6 `adcLoop_lid`** tienen cuatro branches de salida temprana por iteración
   (`exit_branches=4`), mientras los **10 `dir_light_loop_lid`** exteriores
   contienen exactamente un label interno, un branch interno y un backedge
   anidado (`internal_labels=1`, `internal_branches=1`, `nested_backedges=1`).
   Por tanto todos los no-Q de una sola salida/backedge detectados en este corpus
   están cubiertos; los 16 restantes requieren razonamiento de CFG, no quitar el
   requisito `simpleCountedLoop`. No basta con mejorar únicamente DIV/RSQRT.
2. **Coste del renombrado.** Los logs de `fast` muestran 23 operaciones de
   kernel originales frente a 37 tras división/materialización; II 27 → 29.
   En `general`, 31 → 39 operaciones e II 36 → 38. Los gates rechazan esos
   refits, coherente con la ausencia de mejora de la variante expandida.
3. **Quads/multi-store:** `multi_store_loaded_suffix_stream` era una barrera
   global demasiado conservadora. Al retirarla apareció un fallo real en la
   primera iteración pelada: el prefijo cyclic-prefix escribía un valor carried
   en su scratch, pero el primer consumidor del cuerpo pelado leía el registro
   base antes del handoff que solo existía en la entrada steady-state. El
   codegen ahora materializa ese handoff también entre prólogo y cuerpo pelado.
   Con ello `general_quad` vuelve a coincidir con Sony en los 12 frames del
   oráculo determinista y los drains de 12 stores del stream de salida pueden
   emitirse. La protección específica `read_write_memory_stream_conflict`
   permanece para loads/stores con la misma base; no se ha relajado aliasing
   ambiguo.
4. **Indexed/autoincremento:** el rechazo global de cualquier pre/post-incremento
   era demasiado conservador para `indexed/dir_light_vert_loop_lid`. El `SQI`
   postincrementado permanece en el sufijo original y se ejecuta una sola vez;
   el prefijo clonado se inserta después de esa actualización. Un harness VU1
   diferencial en PCSX2 compara memoria bit a bit contra la ruta sin pipeline y
   comprueba además el puntero final: `VU1_DIFFERENTIAL_PASS autoincrement
   rows=4 pointer=36`. El caso distinto de un `LQI (...base++)` dentro del
   propio prefijo clonado también puede cubrirse ahora, pero solo bajo una
   condición estructural estricta: exactamente un acceso postincrementado, branch
   invertible de una ranura que lee el mismo `base`, ninguna otra escritura de
   ese VI y ninguna lectura posterior antes del branch. El prólogo mantiene el
   `LQI`; la copia steady-state usa
   `LQ 0(base)`; el `+1` se ejecuta en el delay slot del branch original; y un
   guard invertido evita especular más allá de la salida. Esto habilita
   `indexed/xform_loop_lid` sin reconocer su nombre. El fixture
   `vu1_postincrement_prefix_differential` compara ocho qwords y el puntero final
   para 1, 2 y 4 iteraciones y produce
   `VU1_DIFFERENTIAL_PASS postincrement-prefix cases=1,2,4`. El caso de una
   iteración prueba específicamente que el drain no aplica un incremento extra.
   Múltiples accesos incrementales, predecrementos, branches incompatibles,
   escrituras adicionales o lecturas posteriores de la base permanecen
   bloqueados. La corrección posterior del cost-model cambia las cifras sin
   cambiar el VSM: `indexed` queda en **30686** default frente a **30558** sin
   pipeline, y el agregado default en **242881** frente a 100870 Sony. El valor
   anterior 31874/32340 multiplicaba colas fallthrough de una sola ejecución por
   el repeat del loop. El soporte guardado sigue siendo semánticamente válido,
   pero bajo el estimador corregido no es rentable: su `xform_loop_lid` tiene
   87 ciclos estáticos steady-state frente a 88 sin pipeline, pero **95**
   estimados frente a **94** por stalls, además de 23 ciclos de prólogo y 6 de
   drain. Son estimaciones estáticas, no una medición de rendimiento PS2.
5. **Límites del estimador y discrepancia planner/emisor cuantificada.** No
   recorre dinámicamente el CFG: pondera bloques de un recorrido textual y
   modela esperas Q/P. `tools/analyze_ps2gl_no_q_rewrites.py` muestra que los
   19 rewrites no-Q aceptados tienen una diferencia de hasta **4 ciclos** entre
   el `bestCycles` usado por el selector aislado y las filas del bucle realmente
   emitido hasta su delay slot. El diagnóstico `bestCyclesInContext`, que conserva
   el estado del prólogo al estimar el main pero **no cambia la selección**, reduce
   ese error máximo a **1 ciclo** en el corpus.
   El ciclo residual ya está localizado con más precisión. Los **19/19** rewrites
   marcan el último token del prefijo como movible al delay slot y aplican una
   rebaja uniforme de un ciclo. Antes de esa rebaja, el coste in-context coincide
   con las filas emitidas en **11/19** casos; después de aplicarla coincide en los
   otros **8/19**. Por tanto `prefixInBranchDelay` no basta por sí solo para inferir
   un descuento de un ciclo en esta métrica. En `general`,
   `dir_light_vert_loop_lid` da 63 baseline -> 46 selector -> 51 context raw ->
   50 context descontado -> 50 emitido, mientras `final_loop_lid` da
   13 -> 7 -> 11 -> 10 -> 11. Se exponen ahora `bestCyclesInContextRaw` y
   `branchDelayDiscount` para separar ambos efectos; siguen siendo diagnóstico y
   no un gate de scheduling.
   Además, el analizador separa explícitamente `steady_state_static_cycles` de
   `fallthrough_tail_static_cycles`. `VsmCostAnalyzer` reconoce ahora el
   self-backedge del bloque, mantiene su delay slot dentro del cuerpo repetido y
   abre un bloque interno `__FALLTHROUGH` para las filas unlabeled posteriores,
   que se pondera una sola vez. En los 19 casos no-Q, el coste del steady-state y
   el de la cola coinciden por separado con la caracterización del VSM
   (errores máximos **0/0**); la cola máxima sigue siendo **6 ciclos**.
   Los 52 VSM de las cuatro variantes son bit a bit idénticos a
   `postincrement-prefix-002`: esta es una corrección de métrica, no de
   codegen. El agregado default pasa de 258325 a **242881**, no-pipeline de
   297385 a **279565**, y Sony permanece en **100870**. El resultado revela que
   optimizar solo filas estáticas puede ser contraproducente cuando aumentan
   stalls, por lo que el siguiente gate de rentabilidad debe incorporar el coste
   estimado del main y los costes one-shot. Ninguna de estas cifras demuestra
   ausencia de hazards ni equivale a tiempo real de PS2.
6. **Detección de instrucciones desconocidas corregida.** El parser de costes
   consume los slots según el número de operandos descrito por
   `VuInstructionInfo`, acepta etiquetas inline y expresiones con espacios, y
   falla con archivo/línea ante mnemónicos desconocidos, operandos incompletos o
   texto extra. Los repros `nop        mystery VI01, VI02` y
   `mystery.xyz VF01, VF02, VF03        nop` dejan de producir un coste válido.
   Los **65 informes de coste guardados** (13 Sony + 52 candidatos) mantienen
   exactamente las mismas métricas; solo cambia el campo de ruta cuando se
   ejecutan desde una ruta relativa. El ensamblado independiente sigue siendo
   obligatorio y tampoco sustituye un validador semántico.
7. **Observabilidad de bucles sin Q añadida sin alterar codegen.** La nueva
   sección `loop_candidates` del dump JSON/texto publica forma del bucle,
   productores/consumidores Q, memoria, autoincrementos, XGKICK y ahora también
   labels/branches internos, salidas y backedges anidados. La suite queda en
   **438 casos**. Tras habilitar los casos seguros de autoincremento se recompilan
   y ensamblan los **13 shaders**: solo cambia `indexed_vcl.vsm`; los otros 12
   permanecen bit a bit idénticos a la baseline `default`.

## Artefactos locales y reproducción

Ver `tools/README.md`. Todo está bajo la raíz de OpenVCL:

- `build/baselines/initial/`: primera ejecución, compilador sin modificaciones.
- `build/baselines/verified/`: ejecución repetida con script ampliado, tamaños,
  hashes de fuentes y corrección CMake de ps2gl. `metadata.json`, `costs.json`,
  `vutext-bytes.json`, `summary.md` y subdirectorios por variante.
- `build/label-invariance-initial/results.json`: matriz completa de 52 checks.
- `build/label-invariance-verified/`: repetición del modo default con validación
  de hashes de los artefactos antes de usarlos.
- `build/incremental-check/results.json`: seis escenarios de dependencias;
  script de la comprobación en `build/verify_incremental.py`.
- `build/integration/{openvcl,sce}/examples/`: los seis ELF compilados;
  logs de compilación en `build/integration/{openvcl,sce}.log`.
- `build/cost-audit/`: tres repros del parser de costes y sus JSON actuales.
- `build/no-q-rewrites.json`: caracterización reproducible de los 19 rewrites
  cyclic-prefix no-Q aceptados y de las diferencias selector/emisión/bloque.
- `build/baselines/multistore-handoff/`: baseline posterior al soporte de drains
  multi-store; los 13 renderers OpenVCL compilan/ensamblan y las cuatro variantes
  de coste quedan resumidas en `summary.md`.
- `build/baselines/postincrement-prefix-002/`: baseline final con la transformación
  `LQI -> LQ + iaddiu` de `indexed/xform_loop_lid`; `metadata.json` marca
  `complete=true`, sin fallos, y los 13 renderers ensamblan.
- `build/baselines/cost-fallthrough-split-001/`: mismos 52 VSM bit a bit que el
  baseline anterior, reanalizados con las colas post-backedge como coste one-shot;
  `metadata.json` marca `complete=true` y sin fallos.
- `build/no-q-rewrites-cost-split.json`: los 19 rewrites no-Q tienen error 0
  tanto para steady-state como para la cola frente a `VsmCostAnalyzer`.
- `build/label-invariance-cost-fallthrough-001/results.json`: 13/13 default con
  instrucciones equivalentes tras renombrar labels y ensamblado correcto.
- `build/label-invariance-postincrement-002/results.json`: los 13 renderers
  default conservan instrucciones equivalentes tras renombrar labels y todos
  ensamblan.
- `build/baselines/profitability-gate-002/`: baseline completo tras activar el
  gate de rentabilidad multi-Q; `metadata.json` marca `complete=true`, los 13
  renderers ensamblan y el agregado default queda en **241331** ciclos estimados.
- `build/label-invariance-profitability-gate-001/`: 13/13 checks default pasan
  tras el gate.
- `build/runtime/general-quad-oracle-profitability-gate-001/`: el renderer
  `linear, quads` mantiene 12/12 frames semánticamente iguales a Sony en PCSX2.
- `build/baselines/acc-fields-001/`: baseline completo tras modelar `ACC` por
  componentes. Los 13 renderers y las cuatro variantes compilan/ensamblan,
  `metadata.json` marca `complete=true` y sin fallos; el agregado default queda
  en **232571** ciclos estimados.
- `build/label-invariance-acc-fields-001/`: 13/13 checks default pasan con el
  modelo por componentes de `ACC`.
- `build/runtime/general-quad-acc-fields-001/`: `general_quad` mantiene 12/12
  frames semánticamente iguales a Sony en PCSX2 tras el cambio de dependencias.
- `build-plan/Testing/Temporary/LastTest.log` y el equivalente en
  `../masp/build-plan/`: pruebas de las herramientas.

Los directorios `build*` no se versionan. Este informe conserva los resultados
resumidos; para reproducirlos en otra máquina ejecutar de nuevo las herramientas.

## Siguiente hito

Completar fase 1 antes de promover cambios de scheduling: el harness diferencial
ya tiene ejecución runtime automatizada en PCSX2, el ciclo residual y la cola de
salida están caracterizados, `general_quad` dispone de un oráculo determinista
que coincide con Sony durante 12 frames y `indexed/xform_loop_lid` tiene un
diferencial específico para el postincremento del prefijo. La ponderación de las
colas fallthrough está corregida y el selector multi-Q ya aplica un gate general
de rentabilidad: tras elegir el mejor candidato por coste aislado, compara el
main estimado en contexto del prólogo con el loop original y rechaza
`candidate >= original`. Es una condición necesaria independiente del trip
count; un main que no mejora nunca puede amortizar prólogo/drain. No usa nombres,
hashes ni el preset de 100 iteraciones.

Con el gate, cuatro rewrites se rechazan en el corpus: `indexed/xform_loop_lid`
(93 frente a 93 en el modelo interno actual) y los `pt_light_vert_loop_lid` de
los tres `general_nospec*` (77 frente a 75). El agregado default baja de
**242881** a **241331**; `indexed` queda en **30558**, igual que no-pipeline.
Los 13 renderers compilan y ensamblan, invariancia default pasa 13/13,
`general_quad` sigue 12/12 frente a Sony y el fixture postincrement pasa
1/2/4 iteraciones. El escape
`OPENVCL_DISABLE_PIPELINE_PROFITABILITY_GATE=1` existe solo para diagnóstico y
fixtures que necesiten forzar la transformación semántica.

El siguiente hito es abordar los 16 loops no-Q con CFG no simple mediante
soporte explícito multi-exit/nested-loop. No eliminar `simpleCountedLoop` como
atajo y no usar nombres de shaders para tomar decisiones.

Antes de ese hito se corrigió una dependencia artificial importante: `ACC`
deja de tratarse como un único recurso monolítico y ahora sus dependencias
RAW/WAR/WAW respetan las máscaras X/Y/Z/W. Esto permite reordenar operaciones
sobre componentes disjuntos sin debilitar dependencias reales. El cambio afecta
7 de los 13 VSM default y reduce el agregado estático de **241331** a
**232571** ciclos estimados; sigue siendo una estimación, no una medida de
hardware. El detector del emisor especializado de luz direccional también se
hizo independiente del orden de dos productos sobre la normal: identifica las
rutas diffuse/specular por sus consumidores en vez de por el orden textual.
La suite queda en **442 casos** y `general_quad` continúa validando 12/12
frames frente a Sony.

El primer caso de CFG no simple se ha separado explícitamente. Los seis
`adcLoop_lid` del corpus tienen cuatro salidas tempranas al mismo destino,
ninguna label/branch interna y un único backedge contado. El dump los marca como
`forward_exit_counted_loop`; los loops exteriores `dir_light_loop_lid` con
backedge anidado permanecen fuera de esa clasificación.

Una copia cyclic-prefix segura del siguiente ciclo en `adcLoop_lid` necesita
guardarse antes del backedge para no ejecutar un load especulativo al salir. El
mejor candidato guardado observado cuesta 39 ciclos frente a 35 del loop
original, así que el gate de rentabilidad lo rechaza. La mejora útil estaba en
otro sitio: cuando cualquier loop del shader se reescribía, CodeGenerator
saltaba globalmente el relleno normal de branch-delay slots. El nuevo pase
acotado rellena únicamente branches pertenecientes a un
`forward_exit_counted_loop` probado, antes de reescribir otros loops.

`build/baselines/forward-exit-branch-fill-001/` completa los 13 renderers y las
cuatro variantes sin fallos. Solo cambian los seis VSM default que contienen
`adcLoop_lid`; el agregado baja de **232571** a **232541** ciclos estimados.
`build/label-invariance-forward-exit-001/` pasa 13/13 y
`build/runtime/general-quad-forward-exit-001/` mantiene 12/12 frames frente a
Sony. El diferencial específico en PCSX2 compara la ruta nueva con el antiguo
slot vacío mientras ambas mantienen otro loop software-pipelined y produce
`VU1_DIFFERENTIAL_PASS forward-exit rows=3 pointer=3`. Estas cifras siguen siendo
estimaciones estáticas; la suite queda en **444/444 tests** y el fixture prueba
equivalencia semántica del caso
ejercitado, no rendimiento de hardware.

El segundo caso de CFG no simple son los 10 `dir_light_loop_lid` exteriores.
Todos contienen exactamente un backedge interno que apunta a un `LoopCS`
contado, ninguna salida lateral y un backedge exterior con inducción conocida.
El dump los marca como `single_nested_counted_loop`; esta clasificación es
estructural y también puede describir otros outer loops equivalentes, pero la
optimización se limita actualmente a candidatos sin productores Q.

En los 10 loops no-Q la secuencia final ya ejecutaba incondicionalmente un
`ILW` del estado de la siguiente luz antes de decrementar el contador exterior.
Cuando ese load plain y el único IALU que cruza no tienen dependencias de datos,
memoria implícita ni recursos implícitos, el load se retrasa al delay slot del
backedge exterior. Por arquitectura el delay slot corre en ambos caminos, así que
no se introduce una lectura especulativa ni se elimina una lectura existente.
El `SQI++` que aparece dentro del inner loop de `indexed` queda intacto y no
forma parte de esta prueba.

`build/baselines/nested-loop-delay-002/` completa los 13 renderers y las cuatro
variantes sin fallos. Frente al milestone forward-exit cambian exactamente los
10 VSM default que contienen `dir_light_loop_lid`; los `pt_light_loop_lid`
con productores Q permanecen sin cambio. El agregado default baja de
**232541** a **232531** ciclos estimados y no-pipeline permanece igual.
`build/label-invariance-nested-loop-001/` pasa 13/13 y
`build/runtime/general-quad-nested-loop-001/` produce
`PS2GL_GENERAL_QUAD_PASS frames=12 variants=default,no-noq,no-pipeline,sce`.
El diferencial aislado en PCSX2 produce
`VU1_DIFFERENTIAL_PASS nested-loop outputs=2 pointer=3 load=333`.
La suite queda en **446/446 tests**. Estas cifras son del modelo estático; no son
una medición de rendimiento de PS2.

Los 16 loops no-Q anteriormente clasificados como CFG no simple ya disponen de
un tratamiento conservador para estas oportunidades locales, pero no se ha
demostrado un cyclic-prefix rentable para los outer loops. No alterar latencias
de hardware ni habilitar un pase experimental para conseguir una tabla
favorable. No considerar ensamblado, invariancia de nombres o capturas visuales
aisladas como prueba suficiente de equivalencia semántica.

La siguiente priorización añade observabilidad equivalente para loops multi-Q
simples sin cambiar codegen. El diagnóstico separa el `RecMII` histórico, el
`RecMII` con recursos por campo, el límite tras eliminar únicamente recurrencias
VF cubiertas por el modelo de renombrado existente y el II que consigue el
placer con esas dependencias precisas. En `indexed/xform_loop_lid` el cuerpo de
65 operaciones pasa de `RecMII=83` a `preciseRenamedMII=52` (`ResMII=38`), pero
el placer todavía termina en `II=83`; a II=52 los fallos observados son de
aristas de dependencia y no de ocupación MRT. La transformación cyclic-prefix
forzada tampoco debe promoverse: el VSM emitido estima 95 ciclos por iteración
frente a 94 sin rewrite. Los probes de los 13 renderers generan VSM bit a bit
idéntico al baseline `nested-loop-delay-002`. Estas cifras son límites y
estimaciones estáticas, no tiempos medidos en PS2.

Ese primer resultado de `II=83` contenía además una incoherencia del probe: las
aristas del placer eran precisas, pero la prioridad `ASAP/ALAP/height` seguía
derivándose del DDG antiguo por base de registro. Al calcular prioridad y
factibilidad desde el mismo DDG preciso, `indexed/xform_loop_lid` baja a
**II=53**, a un solo ciclo del límite 52; el único fallo a II=52 sigue siendo
edge-bound (`IOR`), no MRT-bound. `general/pt_light_vert_loop_lid` alcanza
exactamente su `preciseRenamedMII=114`. Esta corrección sigue siendo solo
diagnóstica y los 13 VSM default permanecen bit a bit idénticos al baseline.

## Corrección de aliasing entre bases VI distintas

La investigación posterior de `indexed/xform_loop_lid` encontró una condición
de corrección previa al trabajo de rendimiento: `plainMemoryAccessesAreDistinct`
trataba dos registros base VI distintos como prueba suficiente de no-alias. Un
diferencial mínimo en PCSX2 inicializa `VI01 == VI02 == 16`, escribe `42.0`
mediante `VI01` y lee mediante `VI02`. Con la regla antigua el scheduler adelanta
el load y devuelve `1.0` (`3f800000`) frente al baseline `42.0` (`42280000`).
Tras hacer conservadoras las bases distintas, el diferencial pasa con
`VU1_DIFFERENTIAL_PASS memory-alias value=42280000`; los diferenciales de
inducción conocida y postincrement siguen pasando.

La consecuencia estática es grande y deliberada hasta disponer de una prueba
de provenance: el agregado default del preset pasa de **232531** a **311872**.
Entre los casos calientes, `indexed` pasa de 29757 a 34457, `general` de 21833
a 26853 y `general_quad` de 25823 a 36488. Estas son estimaciones estáticas, no
tiempos medidos. El siguiente trabajo debe recuperar únicamente relaciones de
dirección demostrables (por ejemplo, raíces comunes más offsets afines), nunca
restaurar la heurística `different VI => no alias`.

## Primera recuperación por provenance afín

El scheduler puede volver a eliminar dependencias de memoria entre bases VI
distintas cuando existe una prueba independiente: ambas bases deben derivarse de
la misma raíz mediante una cadena local de `iaddiu/isubiu` con inmediatos
conocidos. Las escrituras VI no comprendidas invalidan la relación creando una
raíz nueva. Para accesos plain, un delta de dirección no cero módulo 256 QW es
una prueba conservadora de separación tanto en VU0 como en VU1. En los loaded
multi-Q suffix drains se exige además que ambos streams tengan el mismo paso de
loop demostrado, de modo que el delta permanezca constante entre iteraciones.

El diferencial `test/ps2/vu1_affine_alias_differential` elimina una barrera,
comprueba que el candidato adelanta el load sobre el store y valida en PCSX2
`VU1_DIFFERENTIAL_PASS affine-alias value=40e00000`. Los diferenciales
`memory-alias`, `alias-induction` y `postincrement-prefix` continúan pasando, por
lo que raíces desconocidas, alias real e incrementos implícitos siguen cubiertos
por oráculos negativos/independientes. La suite host queda en **452/452 tests**.

`build/baselines/affine-provenance-001/` completa los 13 renderers y las cuatro
variantes. Sus **52 VSM son bit a bit idénticos** a
`build/baselines/alias-correctness-003/`, y por ello las métricas tampoco cambian:
`indexed` permanece en 34457 default frente a 34458 no-pipeline. La comprobación
de invariancia de labels default pasa 13/13. Este resultado es deliberadamente
un milestone de corrección/capacidad: todavía no prueba una mejora de rendimiento
ni desbloquea `indexed/xform_loop_lid`. El siguiente paso es caracterizar las
raíces VI efectivas de ese loop y extender provenance solo donde pueda
demostrarse sin asumir que registros distintos son streams distintos.
