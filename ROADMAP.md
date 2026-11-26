# Feuille de route — EmuCPC

Réécriture de zéro en C++20, d'après `docs/architecture_proposition.md`
(validée par MaaaX le 2026-11-21). Plan validé le 2026-11-21 : une étape par
jalon du § 8 de la proposition, peu de points par étape, détaillés à
l'arrivée sur l'étape. Le plan de l'archive (EmuCPC sur CPCEC) est dans
`docs/archive/ROADMAP_cpcec_2026-11-20.md`.

Statut : ⬜ à faire · 🔶 en cours · ✅ fait · ❌ annulé

Curseur : P7

| # | id | Étape | Contenu | État |
|---|---|---|---|---|
| 1 | E1 | Squelette et Z80 | Dépôt public, CMake, CI, Z80 exact contre SingleStepTests | ✅ |
| 2 | E2 | Z80 au tick | Cycles machine au 1/16 µs, z80test, SSM, variantes NEC et ST | 🔶 |
| 3 | E3 | Phases et vitesse | Gate array 40010 de référence, modèle rapide, banc ≥ 800 M ticks/s | ⬜ |
| 4 | E4 | Banc de la vraie machine | M4, identification, cartouche système, capture RVB, Hantek (dès la M4) | ⬜ |
| 5 | E5 | Machine 6128 et lot | Mémoire, ROM, PPI, clavier, CRTC, rendu, langage, BASIC | ⬜ |
| 6 | E6 | Parité du lot | Trace au tick, dump, state, sheet, stats, différentiel avec l'archive | ⬜ |
| 7 | E7 | Plus | ASIC d'après Arnold V, cartouches, DMA, GX4000 | ⬜ |
| 8 | E8 | Supports et pilotage | Disquette, cassette, touches, CSL, SSM, run until/us | ⬜ |
| 9 | E9 | CRTC et gate array complets | Cinq types, 40007/40008, Compendium entier, Shaker | ⬜ |
| 10 | E10 | Son | AY d'après lvd, étage de sortie, WAV, traces AY et DMA | ⬜ |
| 11 | E11 | États et socket | Instantanés, points d'arrêt, protocole, façade ACE | ⬜ |
| 12 | E12 | Mode développeur | Dear ImGui sur SDL3, OpenGL 3.3 sous Wine, débogueur, hexa, supports, profileur, code | ⬜ |
| 13 | E13 | Mode mixte et MCP | Agent dans la session ouverte, `emucpc-mcp` | ⬜ |
| 14 | E14 | Conception visuelle | Mode joueur soigné, shaders (à côté, après E12) | ⬜ |
| 15 | E15 | Assembleur | Assembleur et désassembleur, mêmes octets que rasm | ⬜ |
| 16 | E16 | Justesse sur la machine | Campagne des questions Q par la M4, le DHO804, le Hantek | ⬜ |
| 17 | E17 | Création | Éditeur graphique, tracker, extensions | ⬜ |
| 18 | E18 | Cartes son d'extension | Carte maison émulée et paramétrable (nombre d'AY, horloges, côté de chaque canal, ports), PlayCity comme cas particulier ; matériel dans son propre projet ; sujet annexe | ⬜ |

## Étape 1 — Squelette et Z80  [E1]

1.1 ✅ [P1] Mise en place — ancien dossier déplacé vers `~/dev/emucpc-cpcec` (archive), nouveau `~/dev/emucpc` à historique neuf avec `AGENTS.md`, `CLAUDE.md` et les seuls documents publiables ; `LICENSE` GPL-3.0-or-later, `CREDITS.md` (texte du crédit du Compendium), `README.md` ; CMake et préréglages Linux et MinGW ; `third_party/` (doctest, nlohmann/json) avec `VERSIONS.md` ; SDL3 pour MinGW par `ci/deps.sh` ; variante `-posix` de MinGW et `libwinpthread` réglées
    - Fait le 2026-11-21. Décidé : nouveau dépôt construit à côté puis échangé avec l'ancien (`~/dev/emucpc-cpcec`) ; préréglages `linux-release`, `linux-debug`, `linux-san`, `windows-mingw`, `macos`, arbres dans `build/<préréglage>/`, Unix Makefiles (Ninja absent) ; MinGW `-posix` avec `-static` (bibliothèque C++ et `libwinpthread` dans l'exécutable) ; sortie standard binaire sous Windows ; doctest 2.5.3 et nlohmann/json 3.12.0 copiés en en-têtes `SYSTEM`, sommes par fichier dans `third_party/VERSIONS.md` ; SDL3 3.4.20 pour MinGW par `ci/deps.sh`, somme épinglée ; `materiel.md` et `docs/lectures/` exclus de Git (privés, § 9.3) ; `README.md` en anglais, crédit du Compendium en français avec traduction.
    - Écarté : variante `-win32` (elle compile aussi `std::thread`, mais s'écarte du modèle pthreads de Linux) ; `libwinpthread-1.dll` livrée à côté ; `doctest_discover_tests` (un `add_test` par exécutable suffit) ; SDL3 pour Linux (attend l'interface).
    - Atteste : trois essais verts (`toolchain` en doctest : version, `ordered_json`, fils et verrous ; `emucpc --version` ; option inconnue → code 2) sur `linux-release`, `linux-debug`, `linux-san` et `windows-mingw` sous Wine ; `objdump -p` : seuls `KERNEL32.dll` et `msvcrt.dll` importés ; `emucpc_core_library` refuse `Threads::Threads` et `emucpc::session` ; `ci/deps.sh` refuse une archive altérée ; `LICENSE` de SHA-256 `3972dc97…`, identique au `gpl.txt` de CPCEC.
1.2 ✅ [P2] CI locale — `ci/run.sh` : Linux, Windows par MinGW sous Wine, construction ASan + UBSan, clang-tidy, `ci/check_core.sh`, `ci/check_third_party.sh`, mêmes octets sous Linux et Windows ; hook `pre-commit` ; arbres de la CI séparés de ceux de `build/`
    - Décidé le 2026-11-21 (MaaaX, « Ok ») : `check_core.sh` éprouvé sur des fichiers fautifs plantés sous `tests/`, le cœur n'existant pas encore ; « mêmes octets » = comparaison de deux dossiers de sorties, appliquée à `--help` seul jusqu'au lot (E5) ; clang-tidy sur tout notre code hors `tests/` et `third_party/`, contrôles du § 0.1 de la proposition, sous Linux seulement ; contrôle des symboles du cœur (`nm`) sous Linux seulement.
    - Fait le 2026-11-22. Décidé en plus : préréglages `ci-linux` (avec `compile_commands.json`), `ci-san` (tests `fast`), `ci-windows`, arbres dans `build-ci/` ; `check_core.sh` attend l'archive en `<build>/libs/<lib>/libemucpc_<lib>.a` et compte une archive absente comme faute ; liste de symboles interdits étendue (E/S de stdio, fils, horloges, `std::chrono::*::now`, `random_device`, flux standard, maths) ; `ci/same_bytes.sh` produit lui-même les deux dossiers de sorties, liste `cases` ; témoins des contrôles déclarés à ctest (`tests/ci/`).
    - Écarté : chercher les archives du cœur n'importe où dans l'arbre (aurait trouvé les témoins) ; `nm` dont les erreurs sont tues ; préréglages de la CI dans `build/` ; JSON des préréglages réécrit en entier (forme compacte gardée).
    - Atteste : `ci/run.sh` vert, 13 étapes, 33 s à froid, 8 s sans changement, 42 s depuis une copie propre ; 8 tests Linux, 8 sous ASan + UBSan, 3 sous Wine ; `--help` identique à l'octet (114 octets) ; défauts plantés puis retirés, chacun refusé par sa propre étape : variable inutilisée (construction), `new` nu (clang-tidy), dépassement signé (UBSan), `<thread>` dans `libs/z80` (`check_core`), octet de `doctest.h` (`check_third_party`), sortie propre à Windows (`same-bytes`) ; hook `pre-commit` posé, lancé à la main avec un défaut : statut 1.
1.3 ✅ [P3] Z80 exact en résultats — toutes les instructions, WZ, Q, IFF, IM, HALT, sur un bus plat ; tests doctest ; banc `emucpc-ssts` sur SingleStepTests, échantillon en CI, suite entière à la demande
    - Fait le 2026-11-24. Décidé (MaaaX) : concept `Bus` du § 2.2 posé dès P3, un appel par cycle machine, `Cycle` (avec `kind`) daté aux durées nominales, 4 ticks par T, sans attente, `ready` non échantillonné avant E2 ; décodeur en `switch` écrit à la main par préfixe, opérandes par les bits de l'opcode, DD/FD par le décodeur principal instancié sur `Index` (hl, ix, iy), DDCB en branche à part ; `FlatBus` dans `libs/z80`, instancié dans la bibliothèque (`extern template`) ; banc `devtools/emucpc-ssts` lu par SAX et arrêté au n-ième cas, fichiers sur les cœurs, codes 0/1/2/77 ; `fast` = 10 premiers cas de chaque fichier avec l'activité du bus (`--cycles`, ajouté à l'échantillon après qu'un défaut de timing planté y a échappé), `full` = tout ; dossier par la variable de cache `EMUCPC_SSTS_DIR`, test sauté sans données ; Q remis à 0 par un préfixe DD/FD ; `enable_testing()` avant les sous-dossiers ; préréglages `ci-linux` et `ci-windows` filtrés sur `fast` ; `bugprone-easily-swappable-parameters` à partir de 3 paramètres.
    - Écarté : table de pointeurs de fonction (appel indirect, bus non mis en ligne) ; code généré (outil de plus ; repli si `emucpc-bench` déçoit en E3) ; bus minimal sans `Cycle` (décodeur à reprendre en E2) ; un cas sur cent (57 s de lecture JSON à chaque commit) ; cache de l'échantillon dans `build-ci/` (inutile) ; échantillon versionné (≈ 13,7 Mo, à revoir en P4) ; variantes NEC et ST (E2).
    - Atteste : SingleStepTests `ebe1875` entier, 1 604 000 cas verts, état final, RAM, ports et activité du bus T par T (`ctest -L full`, 18 s sur 8 cœurs) ; échantillon de 16 040 cas vert sous Linux (0,25 s), ASan + UBSan (9,7 s) et Wine (1,6 s) ; 13 tests doctest (98 assertions) verts ; `check_core.sh` sur `libs/z80` (4 fichiers, 1 archive) ; clang-tidy propre ; CI locale verte en 31 s ; défauts plantés puis retirés, attrapés : T interne d'`INC (HL)` (par `--cycles`), WZ de `LD A,(BC)` ; sans données, ctest affiche « Skipped ». Limite connue : en IM 0, opérandes d'une instruction longue lus en mémoire.
1.4 ✅ [P4] Dépôt public — `MaaaX-EPOCH84/emucpc` sur GitHub et workflow Linux, Windows, macOS sans ROM ; création avec l'accord de MaaaX au moment de la faire
    - Fait le 2026-11-26. Décidé (MaaaX, 2026-11-25, une question à la fois) : `docs/` hors du dépôt public, nettoyage et publication dans un point à inscrire ; historique et chemins du poste gardés tels quels, sans réécriture ; SingleStepTests récupéré dans le workflow au commit `ebe1875` (`ci/fetch_ssts.sh`, dans `build/ssts`, mis en cache) et son échantillon lancé comme étape à part, pour qu'une absence de données échoue au lieu de sauter ; Windows natif sous MSYS2 UCRT64 ; macOS Intel (`macos-15-intel`, à retirer en 2027) et ARM (`macos-15`) ; dépôt public, description posée, issues ouvertes, wiki et Discussions fermés, pas d'artefacts. En plus : en-tête de fichier du projet sur toutes nos sources et nos scripts, contrôlé par `ci/check_headers.sh` (`d7d1704`) ; journaux de `FlatBus` sans allocation (trouvé par une construction clang : `__cxa_begin_catch` dans le cœur) ; `std::thread` joint à la main (pas de `std::jthread` dans la libc++ d'Apple) ; `actions/checkout@v7`, `actions/cache@v6`.
    - Écarté : publier `docs/` nettoyé ou ne jamais le publier ; nettoyer l'état courant ou réécrire l'historique ; laisser le test de SingleStepTests sauté sur GitHub ou versionner un échantillon compact ; compilation croisée testée sur Windows, MSVC ; Intel seul, exécutable universel ; issues fermées ; données de SingleStepTests dans l'arbre de travail (prises pour nos fichiers par `check_headers`).
    - Atteste : https://github.com/MaaaX-EPOCH84/emucpc créé le 2026-11-25 avec l'accord de MaaaX, réglages relus par `gh repo view` ; `main` = `origin/main` = `bbc7de8` (6 commits) ; aucun fichier de ROM, cartouche, disquette, note privée, `AGENTS.md`, `CLAUDE.md` ni `docs/` dans les arbres poussés ; workflow n° 45727026961 vert sur Linux (12 tests), Windows, macOS Intel et ARM (5 tests chacun), l'échantillon de 16 040 cas avec l'activité du bus vert sur les quatre (lu dans les journaux) ; premier passage n° 45688496177 en échec, causes lues et corrigées.

## Étape 2 — Z80 au tick  [E2]

2.1 ✅ [P5] WAIT, Tw et échantillonnage de INT — `ready` interrogé aux fronts d'échantillonnage de WAIT du data sheet (T2 des cycles mémoire et M1, attente automatique des E/S, acquittement d'interruption), Tw de 4 ticks insérés ; INT et NMI échantillonnés au front du dernier T de l'instruction ; tests doctest avec un bus qui retient READY
    - Fait le 2026-11-26. Décidé : `ready` interrogé par `Executor::begin()` aux fronts du data sheet Zilog (T2 descendant pour M1 et mémoire, T1 + 6 ticks ; attente automatique pour les E/S, T1 + 10 ; seconde attente automatique pour l'acquittement, T1 + 14), un Tw de 4 ticks par échantillon à READY bas, rééchantillonné ; seule la fin du `Cycle` bouge ; plafond de 65 535 Tw ; INT (niveau) et NMI (front mémorisé) échantillonnés au front montant du dernier T (`now − 4`), servis au `step()` suivant (`Cpu::int_sampled`, `Cpu::nmi_latched`) ; `FlatBus` : fenêtres `wait_from`/`wait_until` et `int_from`/`int_until`, journaux `ready_samples`, `int_samples`.
    - Écarté : INT et NMI lus au début de `step()` (forme du P3, fausse dès qu'INT change pendant l'instruction) ; un seul `ready_from` dans le bus de test (il bloquait aussi les cycles précédents) ; attente sans plafond.
    - Atteste : 18 tests doctest, 146 assertions (INT pris au tick 12 d'un NOP et manqué à partir du tick 13, échantillon au tick 20 d'`INC BC`, Tw sur M1, lecture, écriture, sortie, acquittement à T1 + 14, fronts actifs inchangés par les Tw) ; défauts plantés (INT à `now`, E/S échantillonnées sur T2) attrapés ; SingleStepTests entier vert, activité du bus comprise ; CI locale verte (67 s). Coût mesuré : ×128 le temps réel au lieu de ×171 (2 030 M ticks/s), Z80 ≈ 39 % du budget de 800 M ticks/s.
2.2 ✅ [P6] Activité du bus T par T, exacte contre SingleStepTests — chaque échantillon des 1 604 000 cas (adresse de rafraîchissement I·R au T3 de M1, cycles internes, durée) ; reconstitution des broches depuis les `Cycle`, réutilisable par le gate array de référence
    - Fait le 2026-11-26. Décidé : `Cycle` porte `address` et `refresh` (I·R avant l'incrément de R, pour `opcode` et `int_ack`), logés dans le remplissage de la structure ; `BusAccess` réduit à `{cycle, value}` ; `libs/z80/include/emucpc/z80/pins.hpp` : `Pins` (adresse, M1, MREQ, IORQ, RD, WR, RFSH), `pins_at()` au tick sur un cycle ou une suite de cycles, `held_address()` (adresse gardée pendant les T internes : I·R après un M1, sinon celle de l'accès), fronts du data sheet Zilog ; `emucpc-ssts --cycles` compare chaque échantillon : adresse lue au front descendant de chaque T, donnée, marqueur d'accès (convention de SingleStepTests, pas un niveau de broche).
    - Écarté : adresse passée à part dans un journal du bus plutôt que dans le `Cycle` (la reconstitution doit se suffire du `Cycle`) ; rappel `refresh()` au bus (un appel de plus par M1, sans usage sur le CPC) ; reconstitution du bus de données au tick (aucune référence sur l'instant où la donnée est valide, au-delà de la convention de SingleStepTests).
    - Atteste : 1 604 000 cas verts, chaque échantillon comparé (16,9 s) ; défauts plantés attrapés (adresse gardée = adresse de l'accès : 122 fichiers en échec ; R + 1 au rafraîchissement : tous) ; 4 cas doctest de broches (22 cas, 208 assertions), dont l'acquittement d'interruption, vérifié seulement contre le data sheet (SingleStepTests n'a pas d'interruption) ; IORQ d'acquittement planté attrapé ; CI locale verte (81 s) ; vitesse dans le bruit (×110 à ×126 contre ×114 à ×126 avant).
2.3 ✅ [P7] Variantes NEC et ST — `Z80Variant` (Zilog par défaut) pour X et Y de SCF/CCF selon Q, et ce qui distingue les CMOS si une source lue le confirme ; d'après z80test et ses images de référence
    - Fait le 2026-11-26. Décidé : `emucpc::z80::Variant` {`zilog`, `nec`, `st`} dans `cpu.hpp`, champ `Cpu::variant` (Zilog par défaut, gardé par `reset()`) ; seuls X et Y après SCF et CCF changent, par `scf_ccf_xy()` dans `step.hpp`, d'après David Banks (wiki `hoglet67/Z80Decoder`, « Undocumented Flags », § SCF/CCF, lu le 2026-11-26) : Zilog X,Y = (Q ⊕ F) | A ; NEC X,Y = A ; ST X = A, Y = (Q ⊕ F) | A ; DD/FD avant SCF/CCF = instruction qui n'a pas modifié les drapeaux. Nom `z80::Variant` au lieu de `Z80Variant` (proposition d'architecture alignée). Le 464 et le 6128 portent un Zilog Z8400APS (NMOS, revu sur photos) : Zilog reste le défaut ; Z80 du 6128 Plus non relevé.
    - Écarté : écarts propres aux CMOS hors SCF/CCF (OUT (C),0 = FFh : aucune source lue ; pages de résultats par puce de Banks non lues) ; variante en paramètre de gabarit (décodeur compilé trois fois pour un `switch` dans deux instructions rares) ; masques sans branchement (gain invisible) ; achat de puces NEC ou ST (confirmation sur puce réelle renvoyée à E16) ; CRC NEC et ST de z80test (vérifiés au P8, recommandation suivie par MaaaX).
    - Atteste : 2 cas doctest (24 cas, 234 assertions) : `CP 28h` puis SCF ou CCF directement, après NOP, après DD ou FD, sur les trois variantes, et X,Y = A quand A les porte ; variante gardée par `reset()` ; défauts plantés (X et Y de ST inversés, NEC calculé en Zilog) attrapés ; SingleStepTests entier vert (20 s) ; CI locale verte (84 s). Vitesse non mesurée (un `switch` dans SCF et CCF seulement).
2.4 ⬜ [P8] Banc z80test — harnais sur bus plat imitant `CHAN-OPEN`, `RST 10h` et `IN (FEh)` = BFh du Spectrum ; sources de Patrik Rak (MIT) assemblées par rasm, adaptées si besoin ; sept programmes, échecs lus dans B, CRC verts en Zilog, tests NEC/ST désignant la variante ; étiquettes `full` et `local`
2.5 ⬜ [P9] Détection SSM — R1 : deux `ED xx` neutres consécutifs émettent un code par un rappel du bus ; coût nul et durée émulée inchangée quand c'est coupé ; codes d'après la spécification SSM v1.2
2.6 ⬜ [P10] Nettoyage et publication de `docs/` — décidé au P4 (réponse A) : renvois aux notes privées réécrits en sources publiques ou retirés, chemins du poste enlevés, puis `architecture_proposition.md` et `sources_emulateurs.md` publiés
2.7 ⬜ [P11] Outillage de CI — construction clang dans `ci/run.sh` (elle a trouvé `__cxa_begin_catch` dans le cœur au P4) ; clé de cache de SingleStepTests par système (`runner.os`) dans le workflow

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

## Étape 18 — Cartes son d'extension  [E18]
