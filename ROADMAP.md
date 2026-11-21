# Feuille de route — EmuCPC

Réécriture de zéro en C++20, d'après `docs/architecture_proposition.md`
(validée par MaaaX le 2026-11-21). Plan validé le 2026-11-21 : une étape par
jalon du § 8 de la proposition, peu de points par étape, détaillés à
l'arrivée sur l'étape. Le plan de l'archive (EmuCPC sur CPCEC) est dans
`docs/archive/ROADMAP_cpcec_2026-11-20.md`.

Statut : ⬜ à faire · 🔶 en cours · ✅ fait · ❌ annulé

| # | id | Étape | Contenu | État |
|---|---|---|---|---|
| 1 | E1 | Squelette et Z80 | Dépôt public, CMake, CI, Z80 exact contre SingleStepTests | ⬜ |
| 2 | E2 | Z80 au tick | Cycles machine au 1/16 µs, z80test, SSM, variantes NEC et ST | ⬜ |
| 3 | E3 | Phases et vitesse | Gate array 40010 de référence, modèle rapide, banc ≥ 800 M ticks/s | ⬜ |
| 4 | E4 | Banc de la vraie machine | M4, identification, cartouche système, capture RVB, Hantek (dès la M4) | ⬜ |
| 5 | E5 | Machine 6128 et lot | Mémoire, ROM, PPI, clavier, CRTC, rendu, langage, BASIC | ⬜ |
| 6 | E6 | Parité du lot | Trace au tick, dump, state, sheet, stats, différentiel avec l'archive | ⬜ |
| 7 | E7 | Plus | ASIC d'après Arnold V, cartouches, DMA, GX4000 | ⬜ |
| 8 | E8 | Supports et pilotage | Disquette, cassette, touches, CSL, SSM, run until/us | ⬜ |
| 9 | E9 | CRTC et gate array complets | Cinq types, 40007/40008, Compendium entier, Shaker | ⬜ |
| 10 | E10 | Son | AY d'après lvd, étage de sortie, WAV, traces AY et DMA | ⬜ |
| 11 | E11 | États et socket | Instantanés, points d'arrêt, protocole, façade ACE | ⬜ |
| 12 | E12 | Mode développeur | Dear ImGui sur SDL3, débogueur, hexa, supports, profileur, code | ⬜ |
| 13 | E13 | Mode mixte et MCP | Agent dans la session ouverte, `emucpc-mcp` | ⬜ |
| 14 | E14 | Conception visuelle | Mode joueur soigné, shaders (à côté, après E12) | ⬜ |
| 15 | E15 | Assembleur | Assembleur et désassembleur, mêmes octets que rasm | ⬜ |
| 16 | E16 | Justesse sur la machine | Campagne des questions Q par la M4, le DHO804, le Hantek | ⬜ |
| 17 | E17 | Création | Éditeur graphique, tracker, extensions | ⬜ |

## Étape 1 — Squelette et Z80  [E1]

1.1 ✅ [P1] Mise en place — ancien dossier déplacé vers `~/dev/emucpc-cpcec` (archive), nouveau `~/dev/emucpc` à historique neuf avec `AGENTS.md`, `CLAUDE.md` et les seuls documents publiables ; `LICENSE` GPL-3.0-or-later, `CREDITS.md` (texte du crédit du Compendium), `README.md` ; CMake et préréglages Linux et MinGW ; `third_party/` (doctest, nlohmann/json) avec `VERSIONS.md` ; SDL3 pour MinGW par `ci/deps.sh` ; variante `-posix` de MinGW et `libwinpthread` réglées
    - Fait le 2026-11-21. Décidé : nouveau dépôt construit à côté puis échangé avec l'ancien (`~/dev/emucpc-cpcec`) ; préréglages `linux-release`, `linux-debug`, `linux-san`, `windows-mingw`, `macos`, arbres dans `build/<préréglage>/`, Unix Makefiles (Ninja absent) ; MinGW `-posix` avec `-static` (bibliothèque C++ et `libwinpthread` dans l'exécutable) ; sortie standard binaire sous Windows ; doctest 2.5.3 et nlohmann/json 3.12.0 copiés en en-têtes `SYSTEM`, sommes par fichier dans `third_party/VERSIONS.md` ; SDL3 3.4.20 pour MinGW par `ci/deps.sh`, somme épinglée ; `materiel.md` et `docs/lectures/` exclus de Git (privés, § 9.3) ; `README.md` en anglais, crédit du Compendium en français avec traduction.
    - Écarté : variante `-win32` (elle compile aussi `std::thread`, mais s'écarte du modèle pthreads de Linux) ; `libwinpthread-1.dll` livrée à côté ; `doctest_discover_tests` (un `add_test` par exécutable suffit) ; SDL3 pour Linux (attend l'interface).
    - Atteste : trois essais verts (`toolchain` en doctest : version, `ordered_json`, fils et verrous ; `emucpc --version` ; option inconnue → code 2) sur `linux-release`, `linux-debug`, `linux-san` et `windows-mingw` sous Wine ; `objdump -p` : seuls `KERNEL32.dll` et `msvcrt.dll` importés ; `emucpc_core_library` refuse `Threads::Threads` et `emucpc::session` ; `ci/deps.sh` refuse une archive altérée ; `LICENSE` de SHA-256 `3972dc97…`, identique au `gpl.txt` de CPCEC.
1.2 ⬜ [P2] CI locale — `ci/run.sh` : Linux, Windows par MinGW sous Wine, construction ASan + UBSan, clang-tidy, `ci/check_core.sh`, `ci/check_third_party.sh`, mêmes octets sous Linux et Windows ; hook `pre-commit`
1.3 ⬜ [P3] Z80 exact en résultats — toutes les instructions, WZ, Q, IFF, IM, HALT, sur un bus plat ; tests doctest ; banc `emucpc-ssts` sur SingleStepTests, échantillon en CI, suite entière à la demande
1.4 ⬜ [P4] Dépôt public — `MaaaX-EPOCH84/emucpc` sur GitHub et workflow Linux, Windows, macOS sans ROM ; création avec l'accord de MaaaX au moment de la faire

## Étape 2 — Z80 au tick  [E2]

## Étape 3 — Phases et vitesse  [E3]

## Étape 4 — Banc de la vraie machine  [E4]

## Étape 5 — Machine 6128 et lot  [E5]

## Étape 6 — Parité du lot  [E6]

## Étape 7 — Plus  [E7]

## Étape 8 — Supports et pilotage  [E8]

## Étape 9 — CRTC et gate array complets  [E9]

## Étape 10 — Son  [E10]

## Étape 11 — États et socket  [E11]

## Étape 12 — Mode développeur  [E12]

## Étape 13 — Mode mixte et MCP  [E13]

## Étape 14 — Conception visuelle  [E14]

## Étape 15 — Assembleur  [E15]

## Étape 16 — Justesse sur la machine  [E16]

## Étape 17 — Création  [E17]
