/**
 * \file            external_boot.c
 * \brief           Link-only external boot context without a new SoC startup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
void SystemInit(void);
void ExitRun0Mode(void);
void _init(void);
void _fini(void);
/** \brief External owner must establish clocks, MPU and peer boot beforehand.
 * \note This fixture performs no runtime hardware initialization and is never
 *       an executable boot recipe. Its empty context cannot qualify real boot.
 */
void SystemInit(void) {
}
/** \brief The external boot owner already selected the supply/run mode. */
void ExitRun0Mode(void) {
}
/** \brief No C constructor/destructor runtime is selected by this fixture. */
void _init(void) {
}
/** \brief Match the fixture's empty constructor context. */
void _fini(void) {
}
