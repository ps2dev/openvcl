# Prompt breve para GPT-5.6 Sol, esfuerzo High

Continúa el trabajo en `/Users/fjtrujy/Projects/openvcl`, rama `optimizations`.
Lee `PLAN_OPTIMIZACION_VU.md`, `reports/2026-09-22-linea-base.md` y
`tools/README.md`. Conserva cualquier cambio existente; en particular había
cambios del usuario en `/Users/fjtrujy/Projects/ps2gl/CMakeLists.txt`. MASP está en
`/Users/fjtrujy/Projects/masp` y PS2DEV en
`/Users/fjtrujy/toolchains/ps2/ps2dev`.

Fase 0 completada: la línea base original tenía 424 tests OpenVCL y 5 grupos
MASP; la suite actual está en 430 tests. Los 13 pares Sony y
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
tienen runtime reproducible en PCSX2. El siguiente candidato general es el
bloqueo de aliasing de `xform_loop_lid` (quads/indexed), no los loops no-Q con
CFG complejo. No convertir aún la métrica nueva en gate. Prohibidos atajos por
nombres, hashes o shaders conocidos. Distingue estimaciones de tiempos reales.
