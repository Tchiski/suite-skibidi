#pragma once

// Shared UI widgets for the Tchiskibidi suite: knobs, meters, tags, panels,
// the Tchiski mascot and a resizable editor base.

#include <juce_audio_processors/juce_audio_processors.h>
#include "TchiskibidiLookAndFeel.h"
#include "TchiskibidiDSP.h"
#include "TchiskibidiUI.h"

namespace tchk
{
using APVTS = juce::AudioProcessorValueTreeState;
using LNF = TchiskibidiLookAndFeel;

//==============================================================================
// Rotary knob with its label above and value below, attached to a parameter
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& name, juce::Colour accent, bool small = false)
        : compact (small)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, compact ? 14 : 16);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);

        label.setText (name.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (LNF::font (compact ? 10.5f : 12.0f));
        label.setColour (juce::Label::textColourId, LNF::textDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        setBufferedToImage (true); // drawn once, then just copied while the sky animates behind it
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (compact ? 13 : 15));
        slider.setBounds (r);
    }

    // planet with a sun orbiting around it (dry/wet, mix)
    Knob& asOrbit() { slider.getProperties().set ("tk", "orbit"); return *this; }
    // rocket fader (volumes): vertical slider, the rocket climbs with the value
    Knob& asRocket()
    {
        slider.getProperties().set ("tk", "rocket");
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, compact ? 14 : 16);
        return *this;
    }

    juce::Slider slider;
    juce::Label label;

private:
    bool compact = false;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
// Toggle button attached to a bool parameter
class Toggle : public juce::ToggleButton
{
public:
    Toggle (APVTS& state, const juce::String& paramId, const juce::String& buttonText, juce::Colour accent)
        : juce::ToggleButton (buttonText)
    {
        setColour (juce::ToggleButton::tickColourId, accent);
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
    }

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Combo box attached to a choice parameter
class Choice : public juce::ComboBox
{
public:
    Choice (APVTS& state, const juce::String& paramId)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
            addItemList (p->choices, 1);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, *this);
    }

private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

//==============================================================================
// Bar meter. getValue returns a dB value. fromTop = gain reduction style.
class Meter : public juce::Component
{
public:
    Meter (std::function<float()> valueFn, float minDb, float maxDb, juce::Colour c, bool reduction = false)
        : getValue (std::move (valueFn)), lo (minDb), hi (maxDb), colour (c), fromTop (reduction) {}

    void update()
    {
        const float v = getValue ? getValue() : lo;
        const float target = juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
        // fast attack, slower fall for readability
        level = target > level ? target : level + (target - level) * 0.25f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r, 3.0f);

        const bool vertical = r.getHeight() >= r.getWidth();
        auto fill = r.reduced (2.0f);
        const float amount = fromTop ? 1.0f - level : level;

        if (vertical)
            fill = fromTop ? fill.removeFromTop (fill.getHeight() * amount) : fill.removeFromBottom (fill.getHeight() * amount);
        else
            fill = fill.removeFromLeft (fill.getWidth() * amount);

        g.setGradientFill (juce::ColourGradient (colour, fill.getX(), fill.getBottom(),
                                                 colour.brighter (0.6f), fill.getX(), fill.getY(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    }

private:
    std::function<float()> getValue;
    float lo, hi;
    juce::Colour colour;
    bool fromTop;
    float level = 0.0f;
};

//==============================================================================
// Drawing helpers
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, juce::Colour base = juce::Colour (0xff160b28).withAlpha (0.92f))
{
    g.setColour (base);
    g.fillRoundedRectangle (r, 12.0f);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.10f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (accent.withAlpha (0.30f));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.2f);
}

// Slanted neon "sticker" tag with a section name
inline void drawTag (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour accent)
{
    juce::Path p;
    const float skew = r.getHeight() * 0.35f;
    p.startNewSubPath (r.getX() + skew, r.getY());
    p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight() - skew, r.getBottom());
    p.lineTo (r.getX(), r.getBottom());
    p.closeSubPath();

    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (5.0f));
    g.setColour (accent);
    g.fillPath (p);
    g.setColour (LNF::background);
    g.setFont (LNF::font (r.getHeight() * 0.62f));
    g.drawText (text.toUpperCase(), r, juce::Justification::centred);
}

// Mix: thin constellation lines joining the knobs of a module, a little star on each knob
inline void drawConstellation (juce::Graphics& g, std::initializer_list<juce::Component*> knobs, juce::Colour c)
{
    std::vector<juce::Point<float>> pts;
    for (auto* k : knobs) if (k != nullptr && k->isVisible()) pts.push_back (k->getBounds().toFloat().getCentre().translated (0.0f, 6.0f));
    if (pts.size() < 2) return;
    g.setColour (c.withAlpha (0.35f));
    for (size_t i = 1; i < pts.size(); ++i)
    {
        juce::Path p; p.startNewSubPath (pts[i - 1]); p.lineTo (pts[i]);
        float dashes[] = { 2.0f, 5.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (p, p, dashes, 2);
        g.fillPath (p);
    }
    for (auto& p : pts)
    {
        g.setColour (c.withAlpha (0.18f)); g.fillEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f);
    }
}

// Panel + tag in one call
inline float tagWidth (const juce::String& name, bool narrow = false)
{
    return narrow ? juce::jmax (64.0f, 14.0f + 9.0f * (float) name.length()) : juce::jmax (84.0f, 14.0f + 11.0f * (float) name.length());
}

inline void drawModule (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& name, juce::Colour accent,
                        juce::Colour base = juce::Colour (0xff160b28).withAlpha (0.92f), bool narrow = false)
{
    drawPanel (g, r.toFloat(), accent, base);
    drawTag (g, { (float) r.getX() + 12.0f, (float) r.getY() + 10.0f, tagWidth (name, narrow), 22.0f }, name, accent);
}

// Standard module layout: ON toggle top-right, components spread evenly on one row below the tag
inline void layoutModule (juce::Rectangle<int> panel, juce::Component* onToggle, std::initializer_list<juce::Component*> items, int topOffset = 38)
{
    if (onToggle != nullptr)
        onToggle->setBounds (panel.getRight() - 60, panel.getY() + 10, 48, 22);
    auto row = panel.withTrimmedTop (topOffset).reduced (8, 6);
    const int w = row.getWidth() / juce::jmax (1, (int) items.size());
    for (auto* c : items)
    {
        const auto cell = row.removeFromLeft (w); // a nullptr leaves an empty cell
        if (c != nullptr) c->setBounds (cell.reduced (2, 0));
    }
}

// EQ / filter curve display from a set of biquads
class CurveDisplay : public juce::Component
{
public:
    std::function<std::vector<BiquadCoeffs>()> getCoeffs;
    juce::Colour colour = LNF::cyan;
    float rangeDb = 15.0f;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r, 8.0f);
        auto xForFreq = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
        g.setColour (LNF::panelLight);
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            g.drawVerticalLine ((int) xForFreq (f), r.getY() + 3.0f, r.getBottom() - 3.0f);
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 3.0f, r.getRight() - 3.0f);
        g.setColour (LNF::textDim);
        g.setFont (LNF::font (9.0f, false));
        for (auto [f, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
            g.drawText (t, (int) xForFreq (f) + 2, (int) r.getBottom() - 12, 30, 10, juce::Justification::left);

        if (! getCoeffs) return;
        const auto coeffs = getCoeffs();
        juce::Path curve, fill;
        const int steps = 160;
        for (int i = 0; i <= steps; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / steps);
            float db = 0.0f;
            for (auto& c : coeffs) db += c.magnitudeDb (f, 48000.0);
            const float x = xForFreq (f);
            const float y = juce::jlimit (r.getY() + 2.0f, r.getBottom() - 2.0f, r.getCentreY() - db / rangeDb * r.getHeight() * 0.45f);
            if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, r.getCentreY()); }
            else curve.lineTo (x, y);
            fill.lineTo (x, y);
        }
        fill.lineTo (r.getRight(), r.getCentreY());
        fill.closeSubPath();
        g.setColour (colour.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (colour.withAlpha (0.25f));
        g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

// Big readout box used in the bottom meter strip
inline void drawReadout (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
                         const juce::String& value, const juce::String& status, juce::Colour statusColour)
{
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (LNF::panelLight);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    auto inner = r.reduced (4.0f, 3.0f);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText (title, inner.removeFromTop (12.0f), juce::Justification::centred);
    g.setColour (LNF::text);
    g.setFont (LNF::font (17.0f));
    g.drawText (value, inner.removeFromTop (inner.getHeight() * 0.62f), juce::Justification::centred);
    g.setColour (statusColour);
    g.setFont (LNF::font (10.0f));
    g.drawText (status, inner, juce::Justification::centred);
}

// The Tchiski mascot: an original neon blob creature with an antenna.
// blink: 0 = eyes open, 1 = closed. look: -1..1 pupil offset.
inline void drawMascot (juce::Graphics& g, juce::Rectangle<float> area, float blink, juce::Point<float> look, float glowPhase)
{
    const float w = area.getWidth(), h = area.getHeight();
    auto body = juce::Rectangle<float> (w * 0.70f, h * 0.70f).withCentre ({ area.getCentreX(), area.getY() + h * 0.60f });

    // antenna
    const juce::Point<float> base (body.getCentreX() + body.getWidth() * 0.12f, body.getY() + 4.0f);
    const juce::Point<float> tip (base.x + w * 0.10f, area.getY() + h * 0.07f);
    juce::Path antenna;
    antenna.startNewSubPath (base);
    antenna.quadraticTo (base.x - w * 0.02f, (base.y + tip.y) * 0.5f, tip.x, tip.y);
    g.setColour (LNF::pink.withAlpha (0.9f));
    g.strokePath (antenna, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float glow = 0.6f + 0.4f * std::sin (glowPhase);
    g.setColour (LNF::lime.withAlpha (0.25f * glow));
    g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (tip));
    g.setColour (LNF::lime);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (tip));

    // body glow + body
    g.setColour (LNF::pink.withAlpha (0.12f));
    g.fillEllipse (body.expanded (10.0f));
    juce::Path blob;
    blob.addRoundedRectangle (body, body.getHeight() * 0.48f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a3cff), body.getX(), body.getY(),
                                             LNF::pink, body.getRight(), body.getBottom(), false));
    g.fillPath (blob);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillEllipse (body.getX() + body.getWidth() * 0.15f, body.getY() + body.getHeight() * 0.08f,
                   body.getWidth() * 0.35f, body.getHeight() * 0.18f);

    // eyes
    const float eyeW = body.getWidth() * 0.26f, eyeH = body.getHeight() * 0.42f;
    for (int side = -1; side <= 1; side += 2)
    {
        auto eye = juce::Rectangle<float> (eyeW, eyeH).withCentre ({ body.getCentreX() + side * body.getWidth() * 0.18f,
                                                                    body.getY() + body.getHeight() * 0.42f });
        g.setColour (juce::Colours::white);
        g.fillEllipse (eye);
        const float pr = eyeW * 0.30f;
        const juce::Point<float> pc (eye.getCentreX() + look.x * eyeW * 0.22f, eye.getCentreY() + look.y * eyeH * 0.18f);
        g.setColour (LNF::background);
        g.fillEllipse (juce::Rectangle<float> (pr * 2.0f, pr * 2.0f).withCentre (pc));
        g.setColour (LNF::cyan);
        g.fillEllipse (juce::Rectangle<float> (pr * 0.9f, pr * 0.9f).withCentre (pc));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (pr * 0.5f, pr * 0.5f).withCentre (pc.translated (-pr * 0.35f, -pr * 0.4f)));

        if (blink > 0.01f) // eyelid
        {
            g.setColour (juce::Colour (0xff9a3cf0));
            g.fillRoundedRectangle (eye.withHeight (eye.getHeight() * blink).expanded (1.0f), eyeW * 0.5f);
        }
    }

    // smile
    juce::Path smile;
    const float sy = body.getY() + body.getHeight() * 0.74f;
    smile.startNewSubPath (body.getCentreX() - body.getWidth() * 0.12f, sy);
    smile.quadraticTo (body.getCentreX(), sy + body.getHeight() * 0.12f, body.getCentreX() + body.getWidth() * 0.12f, sy);
    g.setColour (LNF::background);
    g.strokePath (smile, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
// Editor base: the UI is laid out at a fixed "design" size inside `content`
// and scaled to whatever size the user drags the window to.
// Layers inside `content`, back to front:
//   animated cosmic sky (content.paint) -> static layer (paintFn, cached image)
//   -> dynamic layer (overFn: texts that change) -> controls -> "i" icons -> gear -> settings panel
class ScaledEditor : public juce::AudioProcessorEditor, private juce::ChangeListener
{
public:
    ScaledEditor (juce::AudioProcessor& p, int designWidth, int designHeight, float defaultScale, const juce::String& pluginIdIn = "space")
        : juce::AudioProcessorEditor (&p), baseW (designWidth), baseH (designHeight), pluginId (pluginIdIn)
    {
        setLookAndFeel (&lookAndFeel);
        theme = makeTheme (pluginId);
        content.owner = this;
        addAndMakeVisible (content);
        content.setBounds (0, 0, baseW, baseH);

        staticLayer.fn = [this] (juce::Graphics& g) { if (content.paintFn) content.paintFn (g); };
        dynamicLayer.fn = [this] (juce::Graphics& g) { if (content.overFn) content.overFn (g); };
        for (auto* l : { &staticLayer, &dynamicLayer })
        {
            l->setInterceptsMouseClicks (false, false);
            l->setBounds (0, 0, baseW, baseH);
            content.addAndMakeVisible (l);
        }
        staticLayer.setBufferedToImage (true);

        gearBounds = { baseW - 42, 20, 26, 26 };
        gear.onClick = [this] { showSettings(); };
        content.addAndMakeVisible (gear);

        setResizable (true, true);
        setResizeLimits ((int) (baseW * 0.5f), (int) (baseH * 0.5f), (int) (baseW * 1.6f), (int) (baseH * 1.6f));
        if (auto* c = getConstrainer())
            c->setFixedAspectRatio ((double) baseW / (double) baseH);
        const float scale = settings->windowScale (pluginId, defaultScale);
        setSize ((int) (baseW * scale), (int) (baseH * scale));

        settings->addChangeListener (this);
        anim.cb = [this] { tick(); };
    }

    ~ScaledEditor() override
    {
        anim.stopTimer();
        settings->removeChangeListener (this);
        setLookAndFeel (nullptr);
    }

    void resized() override
    {
        content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    }

    void paint (juce::Graphics& g) override { g.fillAll (theme.bot); }

protected:
    struct Layer : public juce::Component
    {
        std::function<void (juce::Graphics&)> fn;
        void paint (juce::Graphics& g) override { if (fn) fn (g); }
    };
    struct Content : public juce::Component
    {
        ScaledEditor* owner = nullptr;
        std::function<void (juce::Graphics&)> paintFn; // static drawing (panels, labels): cached
        std::function<void (juce::Graphics&)> overFn;  // changing texts, drawn above the static layer
        std::function<void()> layoutFn;
        void paint (juce::Graphics& g) override { if (owner) owner->paintSky (g); }
        void resized() override { if (layoutFn) layoutFn(); }
    };

    juce::SharedResourcePointer<UISettings> settings;
    LNF lookAndFeel; // declared before content so it outlives every child
    juce::TooltipWindow tooltipWindow { this, 350 };
    Content content;
    Layer staticLayer, dynamicLayer;
    const int baseW, baseH;
    juce::String pluginId;
    Theme theme;
    CosmicSky sky;
    GearButton gear;
    juce::Rectangle<int> gearBounds;
    std::vector<SettingsExtra> settingsExtras;      // plugin-specific settings shown under the wheel
    std::function<void()> extraReset;               // called after "reset the plugin"
    std::function<float()> levelDb;                 // output level, makes the sky react to the music
    juce::OwnedArray<InfoIcon> infoIcons;

    juce::Colour panelBase() const { return theme.panel.withAlpha (0.78f); }
    void drawModuleT (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& name, juce::Colour accent, bool narrow = false) const { drawModule (g, r, name, accent, panelBase(), narrow); }
    void drawPanelT (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour accent) const { drawPanel (g, r.toFloat(), accent, panelBase()); }

    // "i" next to a module tag (design coordinates)
    InfoIcon& addInfo (int x, int y, const juce::String& textUtf8, const juce::String& title = {})
    {
        auto* i = infoIcons.add (new InfoIcon (textUtf8, title));
        i->setBounds (x, y, 18, 18);
        content.addAndMakeVisible (i);
        return *i;
    }
    // i placed right after a module tag drawn by drawModule()
    InfoIcon& addModuleInfo (juce::Rectangle<int> panel, const juce::String& name, const juce::String& text, bool narrow = false)
    {
        return addInfo (panel.getX() + 12 + (int) tagWidth (name, narrow) + 6, panel.getY() + 12, text, name);
    }
    // compact ON button for narrow modules
    static juce::Rectangle<int> onBoundsCompact (juce::Rectangle<int> panel) { return { panel.getRight() - 42, panel.getY() + 10, 34, 22 }; }
    static juce::Rectangle<int> onBounds (juce::Rectangle<int> panel) { return { panel.getRight() - 60, panel.getY() + 10, 48, 22 }; }

    // call at the end of the plugin editor constructor
    void finishSetup (int seed, std::vector<CosmicSky::EyeSpot> eyes)
    {
        sky.setup (baseW, baseH, theme, seed, std::move (eyes));
        gear.setBounds (gearBounds);
        for (auto* i : infoIcons) i->toFront (false);
        gear.toFront (false);
        applySettings();
    }

    // Standard header used by every plugin of the suite (drawn in the static layer, over the sky)
    void paintBackgroundAndHeader (juce::Graphics& g, const juce::String& pluginName, const juce::String& subtitle)
    {
        sky.drawStaticStars (g);
        g.setColour (LNF::textDim);
        g.setFont (LNF::font (11.0f));
        g.drawText ("T C H I S K I B I D I", 18, 10, 300, 14, juce::Justification::centredLeft);
        g.setColour (LNF::text);
        g.setFont (LNF::font (28.0f));
        g.drawText (pluginName.toUpperCase(), 18, 22, 400, 32, juce::Justification::centredLeft);
        g.setColour (LNF::textDim);
        g.setFont (LNF::font (11.0f, false));
        g.drawText (subtitle, baseW - 360, 30, 300, 16, juce::Justification::centredRight);
    }

    void paintSky (juce::Graphics& g)
    {
        sky.drawSky (g);
        sky.drawOverlays (g, t, animating);
    }

private:
    struct AnimTimer : public juce::Timer
    {
        std::function<void()> cb;
        void timerCallback() override { if (cb) cb(); }
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override { applySettings(); }

    void applySettings()
    {
        animating = settings->animations();
        if (animating)
        {
            lastTick = juce::Time::getMillisecondCounterHiRes() * 0.001;
            anim.startTimerHz (settings->fps());
        }
        else
        {
            anim.stopTimer();
            sky.renderSky (7.0, 0.0f);
            t = 7.0;
        }
        for (auto* i : infoIcons) i->setVisible (settings->infoBubbles());
        content.repaint();
    }

    void tick()
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const double dt = juce::jlimit (0.0, 0.1, now - lastTick);
        lastTick = now;
        t += dt;

        float pulse = 0.0f;
        if (settings->reactive() && levelDb)
        {
            const float lv = juce::jlimit (0.0f, 1.0f, (levelDb() + 48.0f) / 48.0f);
            lvFast += (lv - lvFast) * 0.6f;
            lvSlow += (lv - lvSlow) * (float) (1.0 - std::exp (-dt / 0.35));
            lvLong += (lv - lvLong) * (float) (1.0 - std::exp (-dt / 6.0));
            pulse = juce::jlimit (0.0f, 1.0f, (lvFast - lvSlow) * 4.0f);
            // a loud passage after a calmer one: the eyes of the sky open
            if (lvSlow > lvLong + 0.12f && lvSlow > 0.45f && t - lastEyes > 8.0)
            {
                sky.openEyesNow (t);
                lastEyes = t;
            }
        }
        smoothPulse = juce::jmax (pulse, smoothPulse * (float) std::exp (-dt * 6.0));
        sky.renderSky (t, smoothPulse);
        content.repaint();
    }

    void showSettings()
    {
        if (settingsPanel == nullptr)
        {
            settingsPanel = std::make_unique<SettingsPanel> (
                pluginId, settingsExtras,
                [this] { return (float) getWidth() / (float) baseW; },
                [this] (float s) { settings->set ("scale_" + pluginId, s); setSize ((int) (baseW * s), (int) (baseH * s)); },
                [this] { resetAllParameters (processor); if (extraReset) extraReset(); });
            content.addChildComponent (*settingsPanel);
            settingsPanel->setBounds (0, 0, baseW, baseH);
        }
        settingsPanel->setVisible (true);
        settingsPanel->toFront (true);
    }

    std::unique_ptr<SettingsPanel> settingsPanel;
    AnimTimer anim;
    bool animating = true;
    double t = 7.0, lastTick = 0.0, lastEyes = -100.0;
    float lvFast = 0.0f, lvSlow = 0.0f, lvLong = 0.0f, smoothPulse = 0.0f;
};

} // namespace tchk
