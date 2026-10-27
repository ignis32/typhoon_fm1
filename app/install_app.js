/* SPDX-License-Identifier: MPL-2.0 (Copyright (C) 2026 ignis32). typhoon's step before an install (fm1.toml install_app,
   fm1-base's web/installer.html APP): the FM-1's user presets saved to a file first, when it runs typhoon
   (web/editor/install.mjs, beside the editor on the site) */
({ before: async (midi, ui) => (await import("../editor/install.mjs")).before(midi, ui) })
