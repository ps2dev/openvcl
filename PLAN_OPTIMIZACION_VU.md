# Plan de optimización general de OpenVCL

Fecha: 22 de septiembre de 2026. Estado: propuesta para ejecutar posteriormente.
Esta revisión no implementa optimizaciones ni ejecuta compilaciones o benchmarks.

Actualización de ejecución (2026-09-22): la fase 0 se ha ejecutado y se ha
iniciado la auditoría de fases 1/2. Resultados, cambios y pasos pendientes en
[el informe de línea base](reports/2026-09-22-linea-base.md). Los números del
apartado 2 se conservan como referencia histórica, no como resultados actuales.

## 1. Objetivo y alcance

Conseguir que los programas generados por OpenVCL sean funcionalmente correctos
y alcancen un rendimiento similar o superior a las referencias de Sony, por
renderer y carga de trabajo, mediante transformaciones generales del compilador.
No se exige igualdad textual, de registros, etiquetas o histogramas.

El trabajo principal estará en `/Users/fjtrujy/Projects/openvcl` (la ruta existente;
`openvlc` en la petición parece una errata). Los otros repositorios son
`/Users/fjtrujy/Projects/masp` y `/Users/fjtrujy/Projects/ps2gl`.
MASP debe preservar el significado del preprocesado; ps2gl aporta el corpus y
la integración. No modificar sus shaders para facilitar artificialmente la comparación.

La primera meta es VU1, que es donde están las 13 referencias. El CMake actual
de ps2gl define `NO_VU0_VECTORS` y `NO_ASM`: estos resultados no demostrarán
rendimiento ni corrección de VU0. Mantener las abstracciones generales y añadir
validación específica de VU0 antes de afirmar soporte equivalente.

## 2. Documentación y estado inspeccionados

Se han leído todos los Markdown encontrados en los tres árboles, excluyendo
los metadatos internos de Git:

- OpenVCL: `README.md`, `CHANGELOG.md`, `test/repro/README.md`.
- MASP: `CLAUDE.md`.
- ps2gl: `README.md`, `CMAKE_BUILD.md`.

No se encontraron archivos `AGENTS.md` en estos repositorios ni en los
ancestros comprobados. También se revisaron los CMake, el preprocesado de
ps2gl, su comparador VSM y los puntos de entrada del planificador/emisor.
Los tres árboles estaban limpios al iniciar la revisión.

| Repositorio | Commit inspeccionado |
|---|---|
| openvcl | `a5867c3daf03828806ee966aca4116622da3f671` |
| masp | `ddb4fa5fb1546e74662979a4e417217c27201c3f` |
| ps2gl | `881fbae441c69ef1c8b40316e4054b670d364abc` |

### Hallazgos que afectan al plan

1. **Ya existe una base considerable.** `VuInstructionInfo`,
   `VuTokenResourceAccess`, `VuSchedulingRules`, `VuLatencyTracker` y
   `VuSchedulerAnalysis` modelan instrucciones, recursos, dependencias y
   planificación. Hay software pipelining genérico activado por defecto.
2. **El código está más avanzado que parte de la documentación.** Existen
   planificación modular, tabla de reservas, layouts de kernel, renombrado y
   replanificación con operaciones añadidas en `VuModuloReservationTable.*`,
   `VuKernelLayout.*` y `VuSchedulerAnalysis.*`. Las rutas experimentales se
   controlan mediante `OPENVCL_USE_GENERIC_KERNEL_REWRITE` y
   `OPENVCL_USE_EXPANDED_DDG_PLACER`. Hay comentarios históricos que aún dicen
   «diagnostic-only» pese a existir consumidores: verificar las llamadas reales.
   La ruta genérica actual también selecciona emisión tipada sin necesitar
   explícitamente `--strict-schedule-slots`.
3. **Existen atajos específicos incompatibles con este objetivo.**
   `--enable-known-loop-optimizations` activa emisores para formas de ps2gl,
   incluyendo reconocimiento de `xform_loop_lid`. Están desactivados por defecto.
   Deben permanecer excluidos de toda medición y solución aceptada.
4. **La compilación predeterminada de ps2gl mezcla procedencias.**
   `PS2GL_USE_SCE_VSM_FOR` contiene `general_quad` por defecto. Es obligatorio
   vaciarlo en la variante OpenVCL. Su comentario documenta un problema visual
   previo, cuya vigencia debe comprobarse, no darse por resuelta ni por actual.
   La comprobación runtime determinista del 2026-09-22 (`tools/validate_ps2gl_general_quad.py`)
   hace coincidir durante 12 frames OpenVCL actual, no-Q desactivado, pipeline
   genérico desactivado y la referencia Sony en el estado VU1 semántico; la
   discrepancia sugerida por capturas animadas no queda reproducida.
5. **Hay 13 referencias, con una excepción de nombre.** Doce son
   `vu1/sce_<renderer>_vcl.vsm`; la de `scei` es `vu1/scei_vcl.vsm`.
6. **El comparador no prueba equivalencia.** `cmake/vsm_diff.py` compara
   histogramas y etiquetas; sus CTest están marcados `WILL_FAIL TRUE` y omiten
   `scei`. Una suite verde con esos tests no prueba corrección del compilador.
7. **La regeneración incremental puede usar resultados antiguos.** El paso de
   compilación VCL declara como dependencia el `_pp4.vcl`, pero no el binario
   OpenVCL. Revisar también las dependencias de macros/includes y MASP.
8. **El entorno necesita una compilación inicial.** Se encontraron `dvp-as` y
   el toolchain CMake en PS2DEV; no estaban los binarios locales `openvcl` ni
   `masp/build/src/masp` en las rutas comprobadas. No se ha validado todavía
   la disponibilidad de PCSX2, hardware o dependencias para todos los ejemplos.

El README publica esta línea base histórica; **no es una medición de esta revisión**:

| Métrica agregada | Sony | OpenVCL |
|---|---:|---:|
| Ciclos estáticos | 6308 | 5268 |
| Ciclos estimados | 6820 | 5838 |
| Ciclos estáticos ponderados | 100358 | 334938 |
| Ciclos estimados ponderados | 100870 | 373227 |

La hipótesis principal es que el solapamiento entre iteraciones y la presión
de registros explican buena parte de la diferencia en los bucles frecuentes.
Debe confirmarse por renderer; menos instrucciones totales no implica menor
coste de ejecución.

## 3. Reglas de generalidad y criterios de éxito

- Ninguna transformación decidirá por nombre de archivo, etiqueta, hash,
  renderer o parecido con una referencia. Sony se usa como comparación externa,
  nunca como fuente de instrucciones durante la compilación.
- Las decisiones se justificarán por dependencias, recursos, flujo de control,
  aliasing, vidas de valores y rentabilidad. Si faltan pruebas de seguridad,
  conservar el camino genérico seguro.
- El preset de costes `ps2gl` puede usarse para informes, pero no para decidir
  optimizaciones. Renombrar etiquetas no debe cambiar las decisiones del compilador.
- Cada nueva regla debe superar casos sintéticos independientes, variaciones
  de nombres/registros/offsets y casos negativos donde aplicarla sería incorrecto.
- Corrección antes que rendimiento: mismos resultados observables en memoria,
  control de flujo, flags relevantes y paquetes GIF. No exigir igualdad de
  registros temporales muertos. No introducir reasociaciones de coma flotante
  o aproximaciones numéricas sin un contrato explícito; conservar inicialmente
  la semántica de las operaciones.
- Medir por cada uno de los 13 renderers. Una mejora agregada no compensa una
  regresión individual. Distinguir regresiones respecto al OpenVCL anterior del
  déficit todavía pendiente respecto a Sony.
- Meta final: coste por trabajo equivalente no superior a Sony en la matriz
  acordada, con corrección demostrada y sin exceder los límites de microprograma.
  Documentar cualquier caso pendiente; no declarar el objetivo cumplido solo
  por una media o una estimación favorable.

## 4. Fases de ejecución

### Fase 0 — Línea base reproducible y compilación totalmente abierta

1. Registrar commits, versiones, rutas absolutas de herramientas, opciones,
   variables experimentales y hashes de entradas, intermedios y salidas.
2. Compilar OpenVCL y MASP locales y ejecutar sus suites existentes.
3. Crear dos builds separados de ps2gl: OpenVCL puro y referencias Sony.
   Fallar si el primero usa herramientas propietarias, VSM Sony u objetos
   precompilados. Vaciar `PS2GL_USE_SCE_VSM_FOR` explícitamente.
4. Generar un manifiesto de 13 pares, incluyendo la excepción `scei`, y
   conservar `_pp1` a `_pp4`, VSM, objetos ensamblados y logs.
5. Corregir dependencias de regeneración de CMake; hasta entonces usar
   directorios nuevos o regeneración explícita de derivados tras cada cambio.
6. Comparar variantes: genérica predeterminada, sin software pipelining,
   kernel experimental y kernel con replanificación expandida. No activar
   todas simultáneamente sin aislar su efecto. Las variables leídas con
   `getenv(...) != NULL` deben eliminarse para desactivarlas: asignar `0`
   también las activa.

**Entrega:** script reproducible y manifiesto/informe por renderer, con
procedencia verificada. Es el primer hito ejecutable.

### Fase 1 — Métricas y oráculo de corrección

1. Auditar `VsmCostAnalyzer`: cobertura de instrucciones, latencias, waits,
   estado entre bloques y tratamiento de prólogo, bucle, epílogo y ramas.
   Contrastar las reglas de hardware con documentación primaria antes de
   cambiarlas. No relajar latencias únicamente porque mejora una tabla.
2. Presentar tamaño en bytes, ciclos del kernel, ocupación de ambas tuberías,
   esperas Q/P, coste de arranque/salida y presión de registros por separado.
3. Normalizar por trabajo real: vértices/primitivas procesados por iteración,
   factores de desenrollado, número de luces y caminos ejecutados. El preset
   de 100 repeticiones por etiqueta es diagnóstico, no un perfil de ejecución.
4. Usar `C(n) = B + K*n` solo cuando representa el camino y unidad de trabajo;
   incorporar restos y caminos cortos si hay desenrollado. Probar tamaños
   pequeños y grandes, límites de buffer y valores alrededor del número de
   etapas. Una entrada de cero elementos se prueba solo donde el contrato la
   admita; en otro caso se valida la guarda del llamador.
5. Diseñar un harness diferencial con el mismo estado inicial para ambos
   microprogramas: capturar memoria de salida, paquetes GIF, flags observables
   y terminación. Reutilizar instrumentación de PCSX2 si está disponible;
   evaluar un intérprete de un subconjunto VU para tests sintéticos solo si
   falta una vía automatizable. Un modelo propio requerirá validación externa.
6. Sustituir la interpretación de `vsm_diff.py` como prueba semántica por un
   informe estructural auxiliar. Añadir checks reales sin depender de
   `WILL_FAIL`. Incorporar el repro de quads/ADC y escenarios de `general_quad`.

El cost-model ya separa una frontera que el preset de loops trataba mal: cuando
un bloque contiene un branch de vuelta a su propia etiqueta, las ranuras de
delay siguen formando parte del cuerpo repetido, pero las filas unlabeled que
siguen al delay slot pertenecen al camino fallthrough de salida y se contabilizan
una sola vez mediante un bloque interno `__FALLTHROUGH`. La caracterización de
los 19 rewrites no-Q reproduce ahora exactamente tanto el steady-state como la
cola (error estático máximo 0 en ambos), y los 52 VSM de las cuatro variantes
permanecen bit a bit iguales al baseline anterior. El agregado de 100 repeticiones
cambia por la métrica, no por codegen: default 258325 -> **242881**, no-pipeline
297385 -> **279565**, con Sony en 100870.

Esta corrección también invalida una conclusión local anterior: `indexed`
queda en **30686** default frente a **30558** sin pipeline. En
`xform_loop_lid`, el rewrite guardado reduce filas steady-state 88 -> 87 pero
sube el coste estimado 94 -> 95, además de prólogo/drain. Es un resultado del
modelo estático, no una medición de PS2, pero demuestra que el número de filas
por sí solo no es un gate de rentabilidad suficiente. Antes de ampliar CFG debe
cerrarse una comparación general de coste estimado que incluya stalls y costes
one-shot, sin reglas por nombre de shader ni por el preset de 100 iteraciones.

**Entrega:** métricas con límites explícitos y pruebas diferenciales
reproducibles. Sin esto, las variantes experimentales no pasan a predeterminadas.

### Fase 2 — Diagnóstico por bucle y coherencia del planificador

1. Clasificar pérdidas por latencias Q/P, dependencias ACC/flags, aliasing,
   presión de registros, ramas/delay slots o huecos entre upper/lower.
2. Reutilizar los diagnósticos de DDG, MII, reservas y kernel existentes.
   Para cada bucle registrar límite por recursos, límite por recurrencias,
   intervalo de inicio conseguido y motivo concreto de rechazo o aumento.
3. Verificar que el VSM emitido respeta el plan elegido: ciclos, pares,
   latencias, delay slots y estado en las entradas/salidas de bloques.
   Evitar que un pase posterior destruya el solapamiento previsto.
4. Auditar liveness en el grafo de control, incluidos backedges y flags;
   no justificar eliminación de dependencias solo por el orden textual.
5. Mantener un único conjunto de hechos sobre instrucciones. Corregir
   discrepancias entre analizador, planificador y emisor con pruebas específicas.

**Entrega:** tabla priorizada de bloqueos y repro mínimo de cada categoría.
Elegir las primeras optimizaciones por evidencia, no por el nombre del shader.

### Fase 3 — Completar el software pipelining general existente

1. Auditar y reutilizar el planificador modular actual antes de reemplazarlo.
   Comenzar con bucles de una entrada y salida controlada, inducción conocida
   y aliasing demostrable. Rechazar inicialmente casos de control no soportados.
2. Modelar dependencias intra/interiteración, distancias y recursos implícitos:
   VF por campo, VI, ACC, I, Q/P, MAC/CLIP/STATUS, memoria y barreras pertinentes.
3. Integrar renombrado por versión/etapa con asignación de registros y valores
   vivos a entrada/salida. Contabilizar MOVEs, operaciones divididas y
   materializaciones antes de decidir la rentabilidad. Mantener registros
   explícitos y restricciones de interfaz.
4. Replanificar el grafo expandido con las operaciones añadidas. Buscar un
   intervalo de inicio legal dentro de un presupuesto determinista; aumentar
   el intervalo o abandonar si no caben recursos/registros.
5. Emitir prólogo, kernel y drenaje desde el plan validado. Resolver caminos
   de pocas iteraciones, restos, ramas, delay slots y límites de memoria.
   No adelantar cargas más allá de entradas válidas ni duplicar stores,
   actualizaciones de punteros o efectos de `xgkick`.
6. Evaluar rentabilidad con coste total, incluyendo arranque, salida, stalls y
   tamaño. El selector multi-Q aplica ya una condición necesaria conservadora:
   tras elegir el mejor candidato por coste aislado, su main estimado en contexto
   del prólogo debe mejorar estrictamente el loop original. Si el steady-state no
   mejora, ningún coste one-shot de prólogo/drain puede amortizarse para ningún
   trip count. Para tamaños desconocidos, mantener esta política general o añadir
   selección explícita por tamaño con coste de la guarda incluido. El preset de
   100 iteraciones sirve para análisis del corpus, no debe convertirse en una
   constante de codegen.
7. Ampliar cobertura incrementalmente: cadenas Q múltiples, P/EFU, aliasing
   afín demostrable y bucles más complejos, únicamente si los diagnósticos
   muestran que justifican el trabajo. El primer caso seguro de autoincremento
   (`indexed/dir_light_vert_loop_lid`) ya está cubierto: el `SQI (...++)`
   permanece en el sufijo original, fuera del prefijo clonado, y dispone de
   validación diferencial runtime. No generalizar esto a actualizaciones que
   formen parte del prefijo sin una prueba equivalente. Los no-Q aún no
   transformados tampoco son simples variantes de este caso: seis tienen
   múltiples salidas tempranas y diez son bucles exteriores con un backedge
   anidado. Requieren soporte de CFG explícito antes de cualquier rewrite.
	   Los drains multi-Q cargados no pueden considerar dos nombres VI distintos
	   como prueba de streams separados: ambos registros pueden contener la misma
	   dirección runtime. Un diferencial PCSX2 reproduce el fallo histórico
	   (`1.0` observado en vez del `42.0` escrito) y obliga ahora a mantener esos
	   accesos ordenados salvo que exista una prueba independiente de provenance o
	   relación afín. La validación de `general_quad` sobre 12 frames sigue siendo
	   útil como oráculo de esos inputs, pero no constituye una prueba genérica de
	   no-alias para todos los valores runtime.
   Para stores del mismo stream, una inducción self-`iaddiu`/`isubiu` de paso
	   constante entre accesos puede usarse para traducir ambos offsets al mismo
	   espacio de direcciones antes del análisis de alias. Este caso dispone de
	   diferencial PCSX2 (`vu1_alias_induction_differential`) sobre memoria y
	   puntero final.
	   Un `LQI (...base++)` dentro del propio prefijo solo puede transformarse
	   bajo una regla separada y estrecha: debe ser el único acceso con
	   pre/postincremento del loop, el branch de una ranura debe ser invertible y
	   leer el mismo `base`, ningún otro token puede escribir ese registro y
	   ninguna instrucción posterior al `LQI` puede leerlo antes del branch. El
	   prólogo conserva el `LQI`; la copia steady-state se convierte en
	   `LQ 0(base)`; el incremento se difiere al delay slot del branch original;
	   y un guard invertido delante del prefijo especulativo evita una lectura o
	   actualización extra al terminar. `indexed/xform_loop_lid` satisface esta
	   forma y queda cubierto por
	   `vu1_postincrement_prefix_differential`, que compara memoria y puntero en
	   PCSX2 para 1, 2 y 4 iteraciones. Múltiples accesos incrementales, predecremento,
	   branches no invertibles, escrituras adicionales o lecturas posteriores del
	   base siguen bloqueados.
	   La primera recuperación de aliasing afín ya está cubierta: cadenas locales
	   de `iaddiu/isubiu` permiten demostrar que dos bases comparten raíz y difieren
	   por un offset constante. El criterio exige delta no cero módulo 256 QW y
	   mantiene conservadoras las raíces desconocidas y los pre/postincrementos.
	   Para loaded multi-Q suffix drains exige además igual paso afín del loop.
	   `vu1_affine_alias_differential` valida el reordenamiento en PCSX2 y los
	   diferenciales de alias real/inducción/postincrement siguen pasando. La
	   baseline `affine-provenance-001` no cambia ninguno de los 52 VSM frente a
	   `alias-correctness-003`, así que `indexed/xform_loop_lid` aún requiere una
	   relación de provenance real adicional antes de recuperar scheduling.
	   Como afinado local posterior, dos actualizaciones inmediatas de una misma
	   base VI pueden fusionarse si entre ellas solo existen accesos de memoria
	   plain con offset constante sobre esa base. El primer delta se traslada a
	   los offsets y el segundo update acumula ambos pasos. Labels, barreras,
	   usos visibles no-memory, pre/postincrementos y offsets fuera de rango
	   bloquean la regla. `vu1_memory_base_update_coalesce_differential` valida la
	   equivalencia en PCSX2. En `indexed/xform_loop_lid` reduce el cuerpo 63 -> 62
	   y ResMII 36 -> 35, pero el placer sigue en II=47; por tanto el siguiente
	   trabajo debe estudiar la recurrencia restante de `next_output`, no asumir
	   una mejora de hardware por esta simplificación.

**Entrega:** mejoras por categoría de bucle, cada una con casos positivos,
negativos y diferenciales; fallback seguro cuando la transformación no es legal.

### Fase 3.5 — Viabilidad exacta y comparación con Sony VCL 1.4beta7

Antes de seguir añadiendo heurísticas al placer modular, separar de forma
concluyente dos causas distintas del déficit: que exista un schedule mejor pero
el greedy no lo encuentre, o que el propio modelo DDG/latencias/recursos de
OpenVCL haga imposible ese schedule. Esta fase es diagnóstica; no cambia el
codegen predeterminado hasta que exista una mejora general validada.

1. Completar y validar el solver exacto de viabilidad modular. Para cada II entre
   el MII preciso renombrado y el II encontrado por el placer, producir un
   resultado `SAT`, `UNSAT` o `UNKNOWN` bajo un presupuesto explícito. Los
   dominios deben ser finitos y reproducibles; limitar también el número máximo
   de stages cuando sea necesario para evitar que una ventana artificialmente
   grande convierta la búsqueda en irrelevante.
2. Cuando el solver devuelva `SAT`, conservar un witness completo de slots y
   verificarlo de forma independiente contra todas las desigualdades DDG y la
   `ModuloReservationTable`. Comparar después ese witness nodo a nodo con el
   layout greedy para identificar la primera decisión que bloquea la solución.
3. Cuando el solver devuelva `UNSAT`, tratarlo como evidencia contra el modelo
   actual, no como permiso para eliminar dependencias. Auditar qué aristas,
   latencias, recursos o restricciones de renombrado explican la diferencia con
   la referencia externa antes de cambiar ninguna regla.
4. Usar Sony VCL 1.4beta7 como referencia de investigación. El paquete público
   contiene builds oficiales x86 y PS2/MIPS y las referencias VSM del corpus ya
   permiten comparar resultados. Hacer reverse engineering dirigido, no una
   decompilación indiscriminada: localizar y reconstruir las fases alrededor de
   `Opt::Loop`, generación de candidatos (`lpgen`), `cap_ii`, reducción modular
   (`sred`/modular reduction), dependencias/latencias y selección del mejor
   candidato.
5. Complementar la inspección estática con experimentos black-box mínimos:
   variar una sola dependencia, latencia, recurso o forma de loop en pequeños
   `.vcl` y observar cómo cambia la salida oficial. Usar estos casos para validar
   la interpretación del binario antes de trasladar ideas al scheduler.
6. Mantener una frontera clara: extraer comportamiento y algoritmos generales,
   no copiar código propietario ni introducir casos especiales por shader,
   etiqueta, hash o renderer. Toda regla nueva debe justificarse con el modelo
   VU y tener tests positivos/negativos y, cuando afecte a codegen, diferencial
   runtime.
	7. El selector global acotado sigue siendo un experimento diagnóstico. Parte del
	   MII preciso, recorre por defecto cuatro `source II`, exige witness exacto,
	   construye las decisiones de rename y compara los candidatos **después** del
	   refit expandido. La clasificación sigue siendo determinista: menor refit II,
	   menor stage count, menos nodos expandidos y source II como desempate.
	8. Se corrigió un error conceptual importante del refit expandido. La rejilla
	   `kernelRewriteMainTokens` está en orden de placement, pero el constructor del
	   DDG preciso determina `dist=0/1` a partir del orden de su secuencia de entrada.
	   Usar la rejilla como si fuera orden de programa podía convertir RAW de la misma
	   iteración en recurrencias sintéticas. La expansión se construye ahora en orden
	   semántico original, conservando la celda de placement sólo como metadata. Con
	   ese modelo corregido, el resultado anterior 37->38 queda **invalidado**: el
	   sweep de `indexed/xform_loop_lid` produce 35->49, 36->49, **37->44** y 38->48;
	   el seleccionado 37->44 tiene dos etapas, 74 nodos, PROLOG=54 nodos y
	   DRAIN=20. Por tanto ya no existe evidencia válida de que OpenVCL iguale las
	   38 filas de Sony con el modelo actual.
	9. La investigación del antiguo `UNKNOWN` branch-aware de II=38 también cambia
	   con el DDG correcto. El refit expandido del candidato source-II 37 tiene ahora
	   MII 44 antes de imponer la forma directa de una sola etapa, de modo que II=38
	   queda por debajo del propio límite del modelo corregido; ya no es un problema
	   de presupuesto del solver. El solver
	   incorpora además, de forma opt-in para este experimento, un matching bipartito
	   upper/lower que detecta conflictos globales de residuos que AC-3 por pares no
	   puede ver. Es una poda necesaria y conservadora, no una relajación del modelo.
	10. La ruta `OPENVCL_EXPERIMENT_SELF_CONTAINED_REFIT_EMISSION=1` se mantiene sólo
	    como banco de pruebas semántico. Con orden y materializaciones corregidos, el
	    mejor MAIN de una etapa para `indexed` pasa a **37->66** y el VSM completo
	    queda en 2976 bytes frente a 3440 del default; ya no se considera una ruta de
	    scheduling candidata a promoción. Al ampliar el diferencial PCSX2 apareció
	    además una lectura stale: un productor renombrado escribía scratch mientras
	    un consumidor posterior seguía leyendo el base arquitectónico. El modelo
	    expandido mantiene ahora estado `dirty` por decisión e inserta
	    `scratch -> base` sólo antes de la siguiente lectura que realmente lo necesita,
	    preservando correctamente los live-ins. El fixture real de `xform_loop_lid`
	    valida 6 casos, entre 1 y 8 iteraciones, distintos pares de índices, hasta 48
	    qwords de salida y los punteros finales, bit a bit contra el baseline.
	    La baseline fresca `corrected-refit-final-014` completa los 13 renderers y las
	    cuatro variantes sin fallos: sus 52 VSM, `vutext-bytes.json` y métricas de
	    coste son idénticos a `direct-refit-default-001`, confirmando que estas
	    correcciones siguen aisladas del path por defecto.
	11. El siguiente milestone de optimización vuelve a ser el envelope completo de
	    dos etapas: transportar y emitir correctamente el PROLOG/MAIN/DRAIN del
	    candidato 37->44 antes de discutir cualquier promoción. La emisión directa
	    de una etapa queda como prueba de infraestructura y semántica, no como objetivo
	    de rendimiento.

**Punto de decisión:** si existe `SAT` por debajo del II greedy, mejorar el
algoritmo de placement usando el witness y la comparación con Sony. Si todos
los II relevantes son `UNSAT` pero Sony genera un loop mejor, priorizar la
diferencia de modelo antes de seguir ajustando heurísticas.

**Entrega:** tabla `II -> SAT/UNSAT/UNKNOWN`, witnesses reproducibles para los
casos SAT, informe de diferencias greedy/exacto y un mapa documentado de las
rutinas relevantes de Sony VCL 1.4beta7 suficiente para explicar la siguiente
decisión de diseño de OpenVCL.

### Fase 4 — Afinado local y asignación de registros

Tras medir lo anterior, mejorar prioridades de camino crítico, emparejamiento,
movimiento de memoria con aliasing probado, eliminación de dependencias falsas
mediante renombrado y ocupación de delay slots. Reutilizar los mecanismos ya
existentes y limitar el crecimiento de código y el tiempo de compilación.
No dedicar semanas a reducir NOPs de bloques fríos mientras persista el déficit
en los bucles calientes. Las correcciones de seguridad locales sí son prioritarias.

**Entrega:** ganancias adicionales verificadas sin regresión del corpus completo.

### Fase 5 — Validación de integración y cierre

1. Ensamblar los 13 microprogramas con `dvp-as`; comprobar tamaño, alineación,
   límites de memoria y protocolo de carga/terminación.
2. Ejecutar ejemplos con entradas/cámara/semillas fijas: geometría visible y
   recortada, culling, quads/triángulos, iluminación/especular, texturas,
   variantes indexed/scei y tamaños frontera. Confirmar qué renderer se usó.
3. Comparar capturas y buffers; una captura correcta es un smoke test,
   no la totalidad de la prueba semántica.
4. Medir en PS2 real si se dispone de ella, separando coste VU de CPU, DMA y
   GS cuando sea posible. Repetir ejecuciones y publicar dispersión. El tiempo
   de pared de PCSX2/JIT no equivale automáticamente a ciclos de PS2.
5. Incorporar a CI tests de corrección, ensamblado, determinismo y límites de
   coste por shader respecto a una línea base versionada. Mantener separado
   el objetivo Sony de los checks de no regresión durante el desarrollo.
6. Promover las rutas experimentales solo después de superar estas pruebas.
   Actualizar README y changelog; retirar o aislar claramente los emisores
   específicos para impedir que se confundan con resultados generales.

**Entrega final:** informe reproducible antes/después, resultados por renderer
 y carga, comandos y artefactos, mejoras generales explicadas y limitaciones
pendientes. Si no hay hardware, identificar expresamente la validación de
rendimiento real como pendiente.

## 5. Comandos iniciales propuestos

Estos comandos son para la ejecución posterior, no se han ejecutado al redactar
el plan. Usar directorios nuevos si ya existen con otra configuración.

```sh
export PS2DEV=/Users/fjtrujy/toolchains/ps2/ps2dev
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
export PATH="$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin"

cmake -S /Users/fjtrujy/Projects/openvcl \
  -B /Users/fjtrujy/Projects/openvcl/build-plan \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_EXAMPLES=OFF
cmake --build /Users/fjtrujy/Projects/openvcl/build-plan -j8
ctest --test-dir /Users/fjtrujy/Projects/openvcl/build-plan --output-on-failure

cmake -S /Users/fjtrujy/Projects/masp \
  -B /Users/fjtrujy/Projects/masp/build-plan \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF
cmake --build /Users/fjtrujy/Projects/masp/build-plan -j8
ctest --test-dir /Users/fjtrujy/Projects/masp/build-plan --output-on-failure

cmake -S /Users/fjtrujy/Projects/ps2gl \
  -B /Users/fjtrujy/Projects/ps2gl/build-plan-openvcl \
  -DOPENVCL_FOUND=/Users/fjtrujy/Projects/openvcl/openvcl \
  -DMASP_FOUND=/Users/fjtrujy/Projects/masp/build-plan/src/masp \
  -DPS2GL_USE_SCE_VSM=OFF -DPS2GL_USE_SCE_VSM_FOR:STRING= \
  -DBUILD_EXAMPLES=OFF
cmake --build /Users/fjtrujy/Projects/ps2gl/build-plan-openvcl \
  --target vu1_objects -j8

cmake -S /Users/fjtrujy/Projects/ps2gl \
  -B /Users/fjtrujy/Projects/ps2gl/build-plan-sce \
  -DPS2GL_USE_SCE_VSM=ON -DPS2GL_USE_SCE_VSM_FOR:STRING= \
  -DBUILD_EXAMPLES=OFF
cmake --build /Users/fjtrujy/Projects/ps2gl/build-plan-sce \
  --target vu1_objects -j8
```

La compilación host de OpenVCL escribe el ejecutable en la raíz del repositorio
incluso con build separado: tenerlo en cuenta al comparar versiones.
Preparar después un manifiesto `pairs.txt` con rutas absolutas Sony/candidato
y ejecutar `--cost-compare-list-markdown --cost-loop-preset ps2gl`.
Los checks `--cost-compare-list-check` permiten comprobar métricas individuales;
registrar inicialmente sus fallos contra Sony como déficits, sin ocultarlos.

Para los ejemplos completos, comprobar primero `ps2stuff` y las bibliotecas
instaladas en `$PS2SDK/ports`. Es una dependencia de integración, no un cuarto
repositorio de optimización salvo que aparezca un bloqueo demostrado.

## 6. Orden y tamaño de los cambios

Orden recomendado: fase 0 → fase 1 → fase 2 → fase 3 → fase 3.5 → fase 4 → fase 5,
con validación funcional continua desde fase 1. Dividir cada mejora en cambios
revisables: repro y evidencia, regla general, pruebas y comparación completa.
Mantener las rutas experimentales separadas hasta validarlas.

La línea base pura de los 13 pares y el estado funcional determinista de
`general_quad` ya están cubiertos. El trabajo posterior debe continuar cerrando
la cobertura semántica general (aliasing/autoincremento y loops no cubiertos) y
ordenar los bloqueos por coste. Ese informe decidirá qué parte del planificador
modular completar primero; no se fija todavía una promesa de mejora ni un
calendario sin esas mediciones.
