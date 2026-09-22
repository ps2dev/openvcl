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
y diagnóstico de los bucles. **No se ha modificado el optimizador, no hay aún
una nueva mejora de rendimiento ni equivalencia funcional demostrada.**

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

## Validación ejecutada

- OpenVCL: **419 casos, 0 fallos, 0 casos conocidos pendientes, 4465 aserciones**.
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

1. **Bucles sin Q fuera de cobertura.** En
   `findVuLoopPipelineOpportunities`, `qProducerCount == 0` descarta el bucle.
   Esto deja fuera bucles de iluminación direccional y salida final con una
   diferencia importante. No basta con mejorar únicamente DIV/RSQRT.
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
5. **Límites del estimador.** No recorre dinámicamente el CFG: pondera bloques
   de un recorrido textual y modela esperas Q/P. No demuestra ausencia de
   hazards VF/VI/ACC/flags ni equivale a ejecución repetida real del bucle.
   Los dumps de oportunidades se obtienen del análisis de entrada y no deben
   confundirse con una verificación del VSM final ya asignado.
6. **Fallo confirmado en la detección de instrucciones desconocidas.**
   `--cost-json` sobre `nop        mystery VI01, VI02` o sobre
   `mystery.xyz VF01, VF02, VF03        nop` devuelve éxito,
   `unknown_instructions=0` e `ignored_lines=0`. El parser busca mnemónicos
   conocidos y omite el desconocido. Con dos desconocidos solo incrementa
   `ignored_lines`. Antes de tratar las métricas como un gate autónomo,
   corregir el reconocimiento de slots con pruebas de entradas desconocidas,
   operandos que coincidan con mnemónicos, espacios y etiquetas. No se ha
   modificado ese parser en esta fase. El ensamblado independiente sigue siendo
   obligatorio y tampoco sustituye un validador semántico.

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
- `build-plan/Testing/Temporary/LastTest.log` y el equivalente en
  `../masp/build-plan/`: pruebas de las herramientas.

Los directorios `build*` no se versionan. Este informe conserva los resultados
resumidos; para reproducirlos en otra máquina ejecutar de nuevo las herramientas.

## Siguiente hito

Completar fase 1 antes de promover cambios de scheduling: corregir la validación
de entradas del analizador de costes y construir el harness diferencial VU1
con estados de entrada idénticos y comparación de memoria/paquetes GIF. Confirmar
`general_quad` con ambas rutas. Después, priorizar soporte general de bucles
sin Q y renombrado menos costoso según pruebas, sin usar nombres de shaders.

No alterar latencias de hardware ni habilitar un pase experimental para conseguir
una tabla favorable. No considerar ensamblado, invariancia de nombres o capturas
visuales aisladas como prueba suficiente de equivalencia semántica.
