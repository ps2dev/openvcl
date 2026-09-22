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
pipeline genérico con la ruta conservadora. **No se ha modificado el optimizador,
no hay aún una nueva mejora de rendimiento ni equivalencia funcional con Sony
demostrada.**

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
- PCSX2 v2.9.74 está instalado. El control de escritorio devuelve
  `Computer Use permissions are not granted`: **no se ha observado el renderizado**
  ni confirmado que el fallo histórico de `general_quad` esté resuelto.
- El nuevo harness diferencial compila hasta ELF con el toolchain PS2 y verifica
  durante el build que el candidato contiene prólogo de software pipeline y el
  baseline no. En esta sesión `fsclient devlist` y `ps2client execee` no obtienen
  respuesta de PS2Link, por lo que su comparación runtime sigue pendiente.

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
   ruta cyclic-prefix acepta actualmente **18 rewrites**: nueve
   `dir_light_vert_loop_lid` y nueve `final_loop_lid`. El resto incluye bucles
   no simples y los casos indexed con pre/post-incremento. No basta con mejorar
   únicamente DIV/RSQRT.
2. **Coste del renombrado.** Los logs de `fast` muestran 23 operaciones de
   kernel originales frente a 37 tras división/materialización; II 27 → 29.
   En `general`, 31 → 39 operaciones e II 36 → 38. Los gates rechazan esos
   refits, coherente con la ausencia de mejora de la variante expandida.
3. **Quads:** el diagnóstico de entrada de `xform_loop_lid` informa
   `multi_store_loaded_suffix_stream` y `read_write_memory_stream_conflict`.
   No quitar estas barreras sin una prueba de aliasing interiteración.
4. **Indexed:** los rechazos incluyen `pre_or_post_increment_memory` y
   `cyclic_prefix_side_effect`. Hace falta modelar explícitamente los efectos
   de actualización de direcciones para ampliar cobertura con seguridad.
5. **Límites del estimador y discrepancia planner/emisor cuantificada.** No
   recorre dinámicamente el CFG: pondera bloques de un recorrido textual y
   modela esperas Q/P. `tools/analyze_ps2gl_no_q_rewrites.py` muestra que los
   18 rewrites no-Q aceptados tienen una diferencia de hasta **4 ciclos** entre
   el `bestCycles` usado por el selector aislado y las filas del bucle realmente
   emitido hasta su delay slot. En `general`, `dir_light_vert_loop_lid` pasa de
   63 ciclos baseline a 46 previstos, pero ocupa 50 ciclos steady-state; para
   `final_loop_lid`, 13 -> 7 previstos termina en 11. Además, el bloque textual
   de `VsmCostAnalyzer` añade **6 ciclos de cola de salida** después del backedge
   en estos casos (50 -> 56 y 11 -> 17), y el preset los pondera junto con el
   loop. Por tanto, 56/17 siguen siendo útiles como coste estructural histórico,
   pero no son el II steady-state exacto. Ninguna de estas cifras demuestra
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
   productores/consumidores Q, memoria, autoincrementos y XGKICK. La suite queda
   en **424 casos**, y una recompilación de los **13 shaders** produce VSM
   bit a bit idénticos a la baseline `default`.

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
- `build/no-q-rewrites.json`: caracterización reproducible de los 18 rewrites
  cyclic-prefix no-Q aceptados y de las diferencias selector/emisión/bloque.
- `build-plan/Testing/Temporary/LastTest.log` y el equivalente en
  `../masp/build-plan/`: pruebas de las herramientas.

Los directorios `build*` no se versionan. Este informe conserva los resultados
resumidos; para reproducirlos en otra máquina ejecutar de nuevo las herramientas.

## Siguiente hito

Completar fase 1 antes de promover cambios de scheduling: ejecutar en PS2/PCSX2
el harness diferencial VU1 ya construido y extender el oráculo a memoria/paquetes
GIF de `general_quad`. Mientras ese runtime no esté disponible, el siguiente
trabajo no invasivo es hacer que la rentabilidad del planner mida de forma
coherente el contexto que verá el emisor y separar la cola de salida del coste
steady-state de loops, sin usar nombres de shaders para tomar decisiones.

No alterar latencias de hardware ni habilitar un pase experimental para conseguir
una tabla favorable. No considerar ensamblado, invariancia de nombres o capturas
visuales aisladas como prueba suficiente de equivalencia semántica.
