# Prompt breve para GPT-5.6 Sol, esfuerzo High

Continúa el trabajo en `/Users/fjtrujy/Projects/openvcl`, rama `optimizations`.
Lee `PLAN_OPTIMIZACION_VU.md`, `reports/2026-09-22-linea-base.md` y
`tools/README.md`. Conserva cualquier cambio existente; en particular había
cambios del usuario en `/Users/fjtrujy/Projects/ps2gl/CMakeLists.txt`. MASP está en
`/Users/fjtrujy/Projects/masp` y PS2DEV en
`/Users/fjtrujy/toolchains/ps2/ps2dev`.

Fase 0 completada: 424 tests OpenVCL y 5 grupos MASP pasan; 13 pares Sony y
52 candidatos ensamblan; 52 pruebas de renombrado de etiquetas pasan. Línea
base reproducida en `build/baselines/verified`. Genérico: 265450 ciclos
ponderados estimados frente a 100870 Sony. Kernel experimental: 267740;
no activarlo. No se ha optimizado todavía el compilador.

Fase 1 en curso: el parser de costes estricto está corregido en `5516c71`;
los 65 informes guardados conservan exactamente sus métricas. Existe
`test/ps2/vu1_differential/`, un primer harness que compara memoria VU1 entre
la ruta pipeline y `--disable-generic-software-pipelining`, y compila hasta ELF.
Falta ejecutarlo: PS2Link no respondió en la última sesión y PCSX2 sigue sin
control de escritorio. `general_quad` aún necesita comprobación funcional.
El dump de loops publica 81 candidatos: **35 sin Q y 46 con Q**. La ruta
cyclic-prefix ya acepta 18 de los no-Q. `tools/analyze_ps2gl_no_q_rewrites.py`
ha revelado dos sesgos estáticos: hasta 4 ciclos entre el `bestCycles` aislado
del selector y el loop realmente emitido, y 6 ciclos de cola de salida que el
coste por label atribuye al loop. El diagnóstico `bestCyclesInContext` conserva
el prólogo y reduce el primer error a como máximo 1 ciclo sin afectar decisiones:
en `general`, dir-light 63 -> 46 aislado -> 50 contexto -> 50 emitido -> 56
bloque; final 13 -> 7 -> 10 -> 11 -> 17. Cuando haya runtime, valida primero el
harness y `general_quad`; mientras tanto explica el ciclo residual y mejora la
contabilidad steady-state, sin convertir aún la métrica nueva en gate. Prohibidos atajos por
nombres, hashes o shaders conocidos. Distingue estimaciones de tiempos reales.
