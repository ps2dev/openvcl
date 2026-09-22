# Prompt breve para GPT-5.6 Sol, esfuerzo High

Continúa el trabajo en `/Users/fjtrujy/Projects/openvcl`, rama `optimizations`.
Lee `PLAN_OPTIMIZACION_VU.md`, `reports/2026-09-22-linea-base.md` y
`tools/README.md`. Hay cambios sin commit en OpenVCL y en
`/Users/fjtrujy/Projects/ps2gl/CMakeLists.txt`; consérvalos. MASP está en
`/Users/fjtrujy/Projects/masp` y PS2DEV en
`/Users/fjtrujy/toolchains/ps2/ps2dev`.

Fase 0 completada: 419 tests OpenVCL y 5 grupos MASP pasan; 13 pares Sony y
52 candidatos ensamblan; 52 pruebas de renombrado de etiquetas pasan. Línea
base reproducida en `build/baselines/verified`. Genérico: 265450 ciclos
ponderados estimados frente a 100870 Sony. Kernel experimental: 267740;
no activarlo. No se ha optimizado todavía el compilador.

Sigue fase 1: corrige primero el parser de costes que omite instrucciones
 desconocidas (`nop mystery VI01, VI02` da unknown=0), con regresiones y sin
alterar costes del corpus válido. Construye validación diferencial VU1 antes
de modificar scheduling; `general_quad` aún necesita comprobación funcional.
PCSX2 está instalado pero faltan permisos para control visual del escritorio.
Después prioriza los bucles sin Q (hoy descartados por el planner), coste del
renombrado y aliasing/autoincremento. Prohibidos atajos por nombres, hashes o
shaders conocidos. Distingue estimaciones de tiempos reales. Actualiza el
informe con avances y deja otro prompt breve si necesitas relevo.
