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
| general | 9050 | 25017 | 29599 | 25293 | 25293 |
| general_nospec | 6494 | 16923 | 19749 | 17199 | 17199 |
| general_nospec_quad | 9004 | 21900 | 23395 | 21900 | 21900 |
| general_nospec_tri | 7813 | 19300 | 20795 | 19300 | 19300 |
| general_pv_diff | 9035 | 25018 | 29600 | 25294 | 25294 |
| general_pv_diff_quad | 11865 | 30395 | 33646 | 30395 | 30395 |
| general_pv_diff_tri | 10353 | 27395 | 30646 | 27395 | 27395 |
| general_quad | 11560 | 29994 | 33245 | 29994 | 29994 |
| general_tri | 10367 | 27394 | 30645 | 27394 | 27394 |
| indexed | 10118 | 32340 | 32340 | 32340 | 32340 |
| scei | 2085 | 5168 | 6697 | 5142 | 5142 |

| Métrica agregada | Sony | Genérico actual |
|---|---:|---:|
| Ciclos estáticos | 6308 | 5133 |
| Ciclos estimados | 6820 | 5575 |
| Estáticos ponderados | 100358 | 245208 |
| Estimados ponderados | 100870 | 265450 |

El resultado actual sustituye las cifras históricas del README como línea base
para esta rama, sin modificar todavía ese documento. El modo experimental
empeora cinco renderers, mantiene siete e introduce una mejora estimada de 26
ciclos ponderados en `scei`. La variante expandida da los mismos VSM que el
kernel en esta ejecución; **no se debe activar por defecto**.

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
| general_quad | xform_loop_lid | 47 | 94 |
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
   restantes son bucles no simples (`adcLoop_lid` y bucles exteriores de luces).
   No basta con mejorar únicamente DIV/RSQRT.
2. **Coste del renombrado.** Los logs de `fast` muestran 23 operaciones de
   kernel originales frente a 37 tras división/materialización; II 27 → 29.
   En `general`, 31 → 39 operaciones e II 36 → 38. Los gates rechazan esos
   refits, coherente con la ausencia de mejora de la variante expandida.
3. **Quads:** el diagnóstico de entrada de `xform_loop_lid` informa
   `multi_store_loaded_suffix_stream` y `read_write_memory_stream_conflict`.
   No quitar estas barreras sin una prueba de aliasing interiteración.
4. **Indexed/autoincremento:** el rechazo global de cualquier pre/post-incremento
   era demasiado conservador para `indexed/dir_light_vert_loop_lid`. El `SQI`
   postincrementado permanece en el sufijo original y se ejecuta una sola vez;
   el prefijo clonado se inserta después de esa actualización. Un harness VU1
   diferencial en PCSX2 compara memoria bit a bit contra la ruta sin pipeline y
   comprueba además el puntero final: `VU1_DIFFERENTIAL_PASS autoincrement
   rows=4 pointer=36`. Los pre/post-incrementos dentro del propio prefijo clonado
   siguen rechazados. Esto amplía cobertura semántica; el selector solo pasa de
   63 a 62 ciclos aislados y el steady-state emitido mide 65 filas, por lo que
   no se reclama una mejora de rendimiento runtime.
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
   `fallthrough_tail_static_cycles`. En los 19 casos la partición reconstruye
   exactamente el bloque estático de `VsmCostAnalyzer` (error máximo **0**); la
   cola de salida máxima es **6 ciclos** y explica 50 -> 56 y 11 -> 17. El preset
   histórico sigue ponderando el bloque textual completo, incluida esa cola de
   una sola ejecución, de modo que 56/17 y los agregados ponderados de arriba son
   útiles como línea base histórica pero **no son coste steady-state por
   iteración**. Ninguna de estas cifras demuestra ausencia de hazards ni equivale
   a tiempo real de PS2.
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
   productores/consumidores Q, memoria, autoincrementos y XGKICK. La suite queda
   en **428 casos**. Tras habilitar el caso seguro de autoincremento se recompilan
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
- `build-plan/Testing/Temporary/LastTest.log` y el equivalente en
  `../masp/build-plan/`: pruebas de las herramientas.

Los directorios `build*` no se versionan. Este informe conserva los resultados
resumidos; para reproducirlos en otra máquina ejecutar de nuevo las herramientas.

## Siguiente hito

Completar fase 1 antes de promover cambios de scheduling: el harness diferencial
ya tiene ejecución runtime automatizada en PCSX2, el ciclo residual y la cola de
salida ya están caracterizados y `general_quad` dispone ahora de un oráculo
determinista que coincide con Sony durante 12 frames. No queda reproducida la
discrepancia semántica que sugerían las capturas animadas. El siguiente trabajo
puede volver a cobertura general: aliasing/autoincremento, loops aún no cubiertos
y, después, corregir la ponderación del cost-model para que una cola fallthrough
de una ejecución no se multiplique por el número de iteraciones.
`bestCyclesInContext` queda por ahora estrictamente diagnóstico: no usarlo como
gate hasta cerrar ese oráculo runtime. No usar nombres de shaders para tomar
decisiones.

No alterar latencias de hardware ni habilitar un pase experimental para conseguir
una tabla favorable. No considerar ensamblado, invariancia de nombres o capturas
visuales aisladas como prueba suficiente de equivalencia semántica.
