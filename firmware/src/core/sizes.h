/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The sizes the engine interface needs before engines/felucca_engine.h (core/layers.h has the rest) */
#define NLAYER 3                     /* layers on one note and breath stream (a preset stores up to 4: core/presets.c) */
#define NPART NLAYER                 /* (felucca_engine.h's names) */
#define NVOICE 1                     /* a layer is one mono voice: legato moves its pitch, the phases run on */
