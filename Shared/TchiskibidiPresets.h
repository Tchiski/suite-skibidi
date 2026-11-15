#pragma once

// "♥ Garder": user presets shared by the whole suite.
// Each preset is the plugin's full state saved in ~/Music/Tchiskibidi/Presets/<Plugin>/<name>.tkpreset

#include <juce_audio_processors/juce_audio_processors.h>
#include "TchiskibidiUI.h"

namespace tchk
{
class UserPresets
{
public:
    explicit UserPresets (const juce::String& pluginName)
        : dir (juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("Tchiskibidi").getChildFile ("Presets").getChildFile (pluginName)) {}

    juce::Array<juce::File> list() const
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.tkpreset");
        files.sort();
        return files;
    }
    bool save (const juce::String& name, juce::AudioProcessor& p) const
    {
        dir.createDirectory();
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        return dir.getChildFile (juce::File::createLegalFileName (name) + ".tkpreset").replaceWithData (mb.getData(), mb.getSize());
    }
    static bool load (const juce::File& f, juce::AudioProcessor& p)
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb) || mb.getSize() == 0) return false;
        p.setStateInformation (mb.getData(), (int) mb.getSize());
        return true;
    }
    juce::File dir;
};

// little heart button
class HeartButton : public juce::Button
{
public:
    HeartButton() : juce::Button ("keep") { setTooltip (TK_FR ("♥ Garder ce réglage dans Mes presets")); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::white.withAlpha (over ? 0.16f : 0.08f));
        g.fillEllipse (r);
        const auto c = r.getCentre();
        const float s = r.getWidth() * (down ? 0.26f : 0.3f);
        juce::Path h;
        h.startNewSubPath (c.x, c.y + s * 0.95f);
        h.cubicTo (c.x - s * 1.6f, c.y - s * 0.1f, c.x - s * 0.9f, c.y - s * 1.3f, c.x, c.y - s * 0.45f);
        h.cubicTo (c.x + s * 0.9f, c.y - s * 1.3f, c.x + s * 1.6f, c.y - s * 0.1f, c.x, c.y + s * 0.95f);
        h.closeSubPath();
        g.setColour (juce::Colour (0xffff8ab8));
        g.fillPath (h);
    }
};

// Preset combo: the plugin's own presets (ids 1..999), then "♥ MES PRESETS" (ids 1000+),
// and "Supprimer « … »" for the user preset currently loaded. The heart saves the current settings.
class PresetSystem
{
public:
    PresetSystem (juce::AudioProcessor& proc, const juce::String& pluginName, juce::ComboBox& comboBox, juce::Button& heartButton,
                  std::function<void (juce::ComboBox&)> addFactoryItems, std::function<void (int)> applyFactory,
                  const juce::String& mineTitle = TK_FR ("♥ MES PRESETS"), const juce::String& placeholder = "PRESETS")
        : processor (proc), store (pluginName), box (comboBox), heart (heartButton),
          addFactory (std::move (addFactoryItems)), applyFactoryPreset (std::move (applyFactory)), title (mineTitle), empty (placeholder)
    {
        refresh();
        box.onChange = [this] { onPick(); };
        heart.onClick = [this] { askName(); };
    }

    void refresh (const juce::String& showName = {})
    {
        box.clear (juce::dontSendNotification);
        box.setTextWhenNothingSelected (empty);
        if (addFactory) addFactory (box);
        mine = store.list();
        box.addSectionHeading (title);
        if (mine.isEmpty())
        { box.addItem (TK_FR ("(règle un son puis clique ♥)"), 999); box.setItemEnabled (999, false); }
        for (int i = 0; i < mine.size(); ++i)
            box.addItem (TK_FR ("♥ ") + mine[i].getFileNameWithoutExtension(), 1000 + i);
        if (current.existsAsFile())
        {
            box.addSeparator();
            box.addItem (TK_FR ("Supprimer « ") + current.getFileNameWithoutExtension() + TK_FR (" »"), 5000);
        }
        if (showName.isNotEmpty())
            for (int i = 0; i < mine.size(); ++i)
                if (mine[i].getFileNameWithoutExtension() == juce::File::createLegalFileName (showName))
                    box.setSelectedId (1000 + i, juce::dontSendNotification);
    }

private:
    void onPick()
    {
        const int id = box.getSelectedId();
        if (id <= 0) return;
        if (id < 999) { current = juce::File(); if (applyFactoryPreset) applyFactoryPreset (id - 1); return; }
        if (id >= 1000 && id < 1000 + mine.size())
        {
            current = mine[id - 1000];
            UserPresets::load (current, processor);
            const auto name = current.getFileNameWithoutExtension();
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::ComboBox> (&box), this, name] { if (safe != nullptr) refresh (name); });
            return;
        }
        if (id == 5000 && current.existsAsFile())
        {
            current.moveToTrash();
            current = juce::File();
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::ComboBox> (&box), this] { if (safe != nullptr) refresh(); });
        }
    }

    void askName()
    {
        auto* host = box.getTopLevelComponent();
        if (host == nullptr) return;
        auto* aw = new juce::AlertWindow (TK_FR ("♥ Garder ce réglage"), TK_FR ("Donne-lui un nom :"), juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", TK_FR ("Mon preset ") + juce::String (store.list().size() + 1));
        aw->addButton (TK_FR ("Enregistrer"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton (TK_FR ("Annuler"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        host->addAndMakeVisible (aw);
        aw->setCentrePosition (host->getLocalBounds().getCentre());
        juce::Component::SafePointer<juce::ComboBox> safe (&box);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, safe] (int r)
        {
            if (r != 1 || safe == nullptr) return;
            auto name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) name = TK_FR ("Mon preset");
            store.save (name, processor);
            current = store.dir.getChildFile (juce::File::createLegalFileName (name) + ".tkpreset");
            refresh (name);
        }), true);
    }

    juce::AudioProcessor& processor;
    UserPresets store;
    juce::ComboBox& box;
    juce::Button& heart;
    std::function<void (juce::ComboBox&)> addFactory;
    std::function<void (int)> applyFactoryPreset;
    juce::String title, empty;
    juce::Array<juce::File> mine;
    juce::File current;
};
} // namespace tchk
