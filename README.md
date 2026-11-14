# Tchiskibidi Suite

Suite de plugins audio maison, en VST3 et AU, qui marche dans Ableton, FL Studio, Logic, Reaper, Bitwig…

| Plugin | Statut | Description |
|---|---|---|
| **Tchiskibidi Space** | v0.1 | Écho stéréo façon bande + reverb |
| Distortion | à venir | |
| Filter | à venir | |
| EQ | à venir | |
| Synth | à venir | |

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

## Obtenir les plugins

### Option A : sans rien installer (recommandé)
GitHub compile les plugins pour toi, pour Windows et pour Mac.

1. Crée un compte sur github.com, puis un nouveau dépôt (par ex. `tchiskibidi-suite`).
2. Envoie tout ce dossier dans le dépôt (bouton « Add file → Upload files », ou GitHub Desktop).
   Le dossier caché `.github` doit bien être inclus.
3. Va dans l'onglet **Actions** et attends que le build devienne vert (environ 10 à 15 minutes la première fois).
4. Clique sur le build, puis télécharge `Tchiskibidi-Windows` ou `Tchiskibidi-macOS` en bas de la page.

À chaque modification envoyée sur GitHub, les plugins sont recompilés automatiquement.

### Option B : compiler sur ton ordi
Il faut **CMake** et **Git**, plus :
- **Windows** : Visual Studio 2022 Community (cocher « Développement Desktop en C++ »)
- **Mac** : Xcode (App Store), puis `xcode-select --install` dans le Terminal

Ensuite, dans un terminal ouvert dans ce dossier :
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Les plugins se trouvent dans `build/Plugins/Space/TchiskibidiSpace_artefacts/Release/`.
Il y a aussi une version **Standalone** pour tester sans DAW.

---

## Installer

| Système | Où copier |
|---|---|
| Windows (VST3) | `C:\Program Files\Common Files\VST3\` |
| Mac (VST3) | `~/Library/Audio/Plug-Ins/VST3/` |
| Mac (AU, pour Logic) | `~/Library/Audio/Plug-Ins/Components/` |

**Sur Mac**, les plugins ne sont pas signés par Apple. S'ils sont bloqués, lance cette commande dans le Terminal :
```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Tchiskibidi Space.vst3"
```
Fais la même chose pour le `.component` si besoin.

**Dans Ableton** : Préférences → Plug-ins → active « Use VST3 Plug-in System Folders », puis clique sur **Rescan**.
Le plugin apparaît dans Plug-ins → Tchiskibidi.

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
