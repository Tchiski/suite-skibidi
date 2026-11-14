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
