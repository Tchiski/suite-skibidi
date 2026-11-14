# Tchiskibidi Suite

Suite de plugins audio maison, en VST3 et AU, qui marche dans Ableton, FL Studio, Logic, Reaper, Bitwig…

| Plugin | Inspiré de | Ce qu'il fait |
|---|---|---|
| **Tchiskibidi Master** | LeMaster, Ozone | Mastering complet en un plugin, avec bouton AUTO |
| **Tchiskibidi Mix** | Neutron | Tout le traitement d'une piste : EQ, compresseur, transitoires, exciteur, clipper |
| **Tchiskibidi Vocal** | Nectar | Chaîne voix complète : autotune, auto-level, de-esser, EQ, comp, saturation, doubleur, delay, reverb |
| **Tchiskibidi Clean** | RX | Nettoyage : débruiteur, anti-ronflette, anti-clics |
| **Tchiskibidi Space** | – | Écho façon bande + reverb |

---|---|---|
| **Tchiskibidi Space** | v0.1 | Écho stéréo façon bande + reverb |
| Distortion | à venir | |
| Filter | à venir | |
| EQ | à venir | |
| Synth | à venir | |

---

## Tchiskibidi Master : mode d'emploi

À mettre **en dernier** sur ta piste Master.

1. Choisis un **preset** (Rap / Trap, Pop / Voix, Club…) ou garde « Défaut ».
2. Choisis ta **CIBLE** sous le bouton AUTO : *Streaming -14* (Spotify, Deezer…), *Fort -10* (rap actuel), *Club -8*…
3. Appuie sur **AUTO** et lance la partie la plus forte de ton son pendant 10 à 20 secondes : le Master règle tout seul le
   gain du limiteur pour atteindre la cible. Le compteur **LUFS S** affiche « OK » quand c'est bon. Coupe AUTO ensuite.
4. Ajuste à l'oreille et compare avec **BYPASS**. Avec **MATCH** activé, l'original est joué au même volume, ce qui permet
   de comparer honnêtement (plus fort paraît toujours meilleur).

| Module | Réglages |
|---|---|
| **Spectral** | Équilibre tonal automatique : compare ton son à l'équilibre d'un master moderne et corrige doucement. *Amount* règle la force. La courbe montre ce qui est corrigé. |
| **Comp** | *Glue* : compresseur 3 bandes (L/M/H) qui « colle » le mix. *Detail* : remonte les petits détails (compression vers le haut). |
| **EQ** | *Bass* (80 Hz) et *Air* (12 kHz). |
| **Stereo** | *Width* : largeur stéréo. *Bass mono* : met les basses (sous 120 Hz) en mono, indispensable en club. Le point PHASE doit rester à droite. |
| **Analog** | Saturation *Tape*, *Tube* ou *Crunch*. *Color* règle la chaleur. |
| **Clip** | Coupe doucement les pics avant le limiteur, pour gagner du volume sans écraser. |
| **Limiter** | *Gain* : volume. *Ceiling* : plafond (-1 dB conseillé pour le streaming). True peak, oversampling 4x. |

Compteurs du bas : **LUFS S** (volume des 3 dernières secondes), **LUFS I** (volume moyen du morceau, RESET pour
recommencer), **TRUE PEAK**, **DYNAMICS** (PUNCHY = bien, ÉCRASÉ = trop compressé) et **STEREO** (PHASE ! = problème).
Il y a aussi **UNDO/REDO** et deux mémoires **A/B** pour comparer deux réglages.

## Tchiskibidi Mix
À mettre sur n'importe quelle piste (kick, 808, snare, guitare, bus…). Choisis un preset puis ajuste :
**EQ** 4 bandes + low cut avec courbe · **Comp** (threshold, ratio, attack, release, mix parallèle, gain automatique) ·
**Transient** (*Punch* = plus d'attaque, *Sustain* = plus ou moins de queue) · **Exciter** (ajoute des harmoniques
brillantes) · **Clip** · **Stereo** · **In/Out**.

## Tchiskibidi Vocal
À mettre sur ta piste voix. Presets : Rap lead, Rap douce / mélo, Chant pop, Backs, Adlibs, Podcast, Lo-fi, puis des presets
**façon artistes** (inspirés de leur son de voix habituel) : Travis Scott, Drake, Kanye West (808s), Future, The Weeknd, PNL,
Jul, Ninho, Damso. Les presets ne changent pas la tonalité de l'autotune : règle-la selon ton morceau.

**Autotune** : *Tonalité* + *Gamme* (Chromatique = marche sur tous les morceaux, sinon choisis la tonalité de ton instru),
*Speed* (0 ms = effet robot à la T-Pain / Travis, 100 ms et plus = correction naturelle), *Amount* (dose de correction).
L'écran montre la note chantée et la note visée.
**Level** (égalise automatiquement le volume de la voix) · **De-ess** (adoucit les « s ») · **EQ voix** (low cut, boue,
présence, air) · **Comp** (*Speed* = rapidité) · **Warm** (saturation à lampe) · **Double** (doublage stéréo) ·
**Delay** calé sur le tempo · **Reverb**.

## Tchiskibidi Clean
Pour réparer un enregistrement (souffle, ronflette, clics) :
- **De-noise** : en mode *Adaptatif*, il suit le bruit tout seul. Pour un meilleur résultat, lance un passage où on
  n'entend **que le bruit** (sans voix), puis appuie sur **LEARN** : il écoute 3 secondes et apprend le bruit.
  *Reduction* = combien on enlève, *Sensibilité* = jusqu'où on considère que c'est du bruit.
- **De-hum** : enlève la ronflette électrique (50 Hz en Europe) et ses harmoniques.
- **De-click** : enlève les clics et craquements.
- **Écoute** : « Écouter le bruit retiré » fait entendre uniquement ce qui est enlevé. Si tu entends la voix, baisse les réglages.

Clean ajoute environ 45 ms de latence (compensée automatiquement par Ableton). C'est un outil de réparation, pas pour s'enregistrer en direct.

---

## Tchiskibidi Space : les réglages

**ECHO**
- **Time** : temps entre les répétitions (1 ms à 2 s). Il est grisé quand Sync est activé.
- **Sync** + **division** : l'écho se cale sur le tempo du projet (1/4, 1/8 pointée, triolets…).
- **Feedback** : nombre de répétitions. Au-dessus de 80 % ça part dans l'espace, et la saturation douce empêche que ça explose.
- **Wobble** : légère modulation du temps, comme une vieille bande. C'est l'effet « lo-fi ».
- **Low cut / High cut** : chaque répétition devient plus fine et plus sombre.
- **Ping-pong** : les répétitions rebondissent entre gauche et droite.
- **Level** : volume des échos.

**REVERB** (elle reçoit le son d'origine et les échos)
- **Size** : taille de la pièce. **Damping** : plus c'est haut, plus la reverb est sombre. **Width** : largeur stéréo. **Level** : volume.

**MASTER**
- **Mix** : dosage entre le son sec et l'effet. À 50 %, le son sec et l'effet sont tous les deux au maximum. À 100 %, il ne reste que l'effet, ce qui est pratique sur une piste de retour (send).
- **Output** : volume de sortie.

Astuce : double-clic sur un bouton pour revenir à sa valeur par défaut.

---

## Installer (le plus simple)

1. Sur la page GitHub du projet, clique à droite sur **Releases** → **latest**.
2. Télécharge **Tchiskibidi-Installer-macOS.pkg** (Mac) ou **Tchiskibidi-Installer-Windows.exe** (Windows).
3. Double-clique dessus et suis les étapes (« Continuer », « Installer », ton mot de passe).
4. Dans Ableton : Réglages → Plug-ins → **Examen**. Le plugin apparaît dans Plug-ins → Tchiskibidi.

**Mac, première fois seulement** : si le Mac dit qu'il ne peut pas vérifier l'installateur, clique sur « OK »,
puis ouvre **Réglages Système → Confidentialité et sécurité**, descends et clique sur **Ouvrir quand même**.

Chaque fois que le code change sur GitHub, un nouvel installateur est fabriqué automatiquement (10 à 15 min).

---

## Structure du projet
```
CMakeLists.txt              ← liste des plugins de la suite
Shared/                     ← look commun à tous les plugins Tchiskibidi
Plugins/Space/Source/
    PluginProcessor.*       ← le son (DSP)
    PluginEditor.*          ← l'interface
.github/workflows/build.yml ← compilation automatique Windows + Mac
```

Construit avec [JUCE](https://juce.com), téléchargé automatiquement au moment du build.
Licence JUCE : gratuite pour un usage perso et les petits revenus. Si tu veux vendre la suite un jour, vérifie les conditions de JUCE.
