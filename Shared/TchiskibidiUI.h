#pragma once

// Shared UI layer of the Tchiskibidi suite:
//  - Theme: the colours of each plugin's sky
//  - UISettings: display settings shared by every Tchiskibidi plugin (saved on disk)
//  - CosmicSky: animated cosmic background (drifting nebulae, twinkling stars, eyes that open)
//  - InfoIcon: small "i" that shows an explanation when hovered
//  - GearButton + SettingsPanel: the settings wheel (top right of every plugin)
//
// CPU notes: the sky is rendered at half resolution into one image (nebulae are soft, so it
// looks the same), static stars are drawn once into the cached static layer, and the whole
// animation stops when "Animations" is switched off.

#include <juce_audio_processors/juce_audio_processors.h>
#include "TchiskibidiLookAndFeel.h"

// French text with accents in UTF-8 source files
#define TK_FR(text) juce::String (juce::CharPointer_UTF8 (text))

namespace tchk
{
//==============================================================================
struct Theme
{
    juce::Colour top, bot, panel;
    std::array<juce::Colour, 4> neb;
    std::array<juce::Colour, 3> iris;
    juce::Colour glow, lid;
};

inline Theme makeTheme (const juce::String& name)
{
    using C = juce::Colour;
    if (name == "mix")
        return { C (0xff0f2e20), C (0xff04110b), C (0xff081a13), { C (60, 255, 160), C (198, 255, 61), C (40, 200, 180), C (120, 255, 220) },
                 { C (0xfff0ffb8), C (0xff5dffa8), C (0xff14a88e) }, C (90, 255, 170), C (0xffb8ffd9) };
    if (name == "vocal")
        return { C (0xff0e1c4a), C (0xff030716), C (0xff080e26), { C (60, 110, 255), C (30, 70, 220), C (61, 245, 255), C (140, 120, 255) },
                 { C (0xffe6f6ff), C (0xff5aa8ff), C (0xff2c3fd8) }, C (90, 150, 255), C (0xffbcd6ff) };
    if (name == "clean")
        return { C (0xff03504a), C (0xff011a18), C (0xff032221), { C (30, 240, 210), C (61, 245, 255), C (10, 190, 170), C (140, 255, 230) },
                 { C (0xffeafffb), C (0xff3df5e0), C (0xff0aa898) }, C (50, 240, 210), C (0xffc4fff4) };
    if (name == "master")
        return { C (0xff3e1408), C (0xff140504), C (0xff220b08), { C (255, 90, 60), C (255, 162, 61), C (255, 225, 77), C (255, 70, 140) },
                 { C (0xfffff2a8), C (0xffffa23d), C (0xffff4f3d) }, C (255, 140, 60), C (0xffffd0a0) };
    if (name == "accords")
        return { C (0xff3a0e3e), C (0xff10030f), C (0xff1c0820), { C (255, 211, 77), C (220, 90, 255), C (255, 150, 200), C (255, 243, 196) },
                 { C (0xfffff3c4), C (0xffffd34d), C (0xffd86bff) }, C (255, 211, 77), C (0xfffff0c8) };
    if (name == "synth")
        return { C (0xff1a1450), C (0xff05041a), C (0xff0e0b2c), { C (255, 138, 61), C (110, 90, 255), C (255, 90, 160), C (255, 226, 204) },
                 { C (0xffffe2cc), C (0xffff8a3d), C (0xff6e5aff) }, C (255, 138, 61), C (0xffffd8c0) };
    if (name == "basse")
        return { C (0xff3a0820), C (0xff12020a), C (0xff1e0612), { C (255, 77, 125), C (160, 40, 120), C (255, 140, 170), C (255, 208, 222) },
                 { C (0xffffd0de), C (0xffff4d7d), C (0xffa02878) }, C (255, 77, 125), C (0xffffc8d8) };
    // space (default): the Plaine des Songes sky
    return { C (0xff241245), C (0xff0e0820), C (0xff160b28), { C (242, 115, 255), C (90, 140, 255), C (190, 90, 255), C (61, 200, 255) },
             { C (0xff9ff6ff), C (0xffb48cff), C (0xffff7ae6) }, C (242, 115, 255), C (0xfff2b8ff) };
}

//==============================================================================
// Display settings shared by all Tchiskibidi plugins (one small file in Application Support)
class UISettings : public juce::ChangeBroadcaster
{
public:
    UISettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Tchiskibidi";
        o.filenameSuffix = ".settings";
        o.folderName = "Tchiskibidi";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);
    }

    bool animations() const  { return props->getBoolValue ("animations", true); }
    int fps() const          { return juce::jlimit (8, 60, props->getIntValue ("fps", 24)); }
    bool reactive() const    { return props->getBoolValue ("reactive", true); }
    bool infoBubbles() const { return props->getBoolValue ("info", true); }
    float windowScale (const juce::String& plugin, float def) const { return (float) props->getDoubleValue ("scale_" + plugin, def); }

    void set (const juce::String& key, const juce::var& v)
    {
        props->setValue (key, v);
        props->saveIfNeeded();
        sendChangeMessage();
    }

private:
    std::unique_ptr<juce::PropertiesFile> props;
};

//==============================================================================
class CosmicSky
{
public:
    struct EyeSpot { float x, y, w; };

    void setup (int width, int height, const Theme& th, int seed, std::vector<EyeSpot> spots)
    {
        w = width; h = height; theme = th;
        juce::Random rnd (seed);

        // soft nebula sprites, one per colour
        for (size_t i = 0; i < sprites.size(); ++i)
        {
            sprites[i] = juce::Image (juce::Image::ARGB, 96, 96, true);
            juce::Graphics g (sprites[i]);
            juce::ColourGradient cg (theme.neb[i].withAlpha (0.55f), 48.0f, 48.0f, theme.neb[i].withAlpha (0.0f), 48.0f, 2.0f, true);
            cg.addColour (0.45, theme.neb[i].withAlpha (0.22f));
            g.setGradientFill (cg);
            g.fillEllipse (0.0f, 0.0f, 96.0f, 96.0f);
        }

        // low-resolution base (gradient), refreshed into `sky` every frame
        base = juce::Image (juce::Image::ARGB, juce::jmax (1, w / 2), juce::jmax (1, h / 2), true);
        sky = juce::Image (juce::Image::ARGB, base.getWidth(), base.getHeight(), true);
        {
            juce::Graphics g (base);
            g.setGradientFill (juce::ColourGradient (theme.top, 0.0f, 0.0f, theme.bot, 0.0f, (float) base.getHeight() * 0.75f, false));
            g.fillAll();
            juce::ColourGradient glow (theme.top.brighter (0.25f).withAlpha (0.55f), (float) base.getWidth() * 0.5f, 0.0f,
                                       theme.top.withAlpha (0.0f), (float) base.getWidth() * 0.5f, (float) base.getHeight() * 0.7f, true);
            g.setGradientFill (glow);
            g.fillAll();
        }

        nebulae.clear();
        const float m = (float) juce::jmin (w, h);
        for (int i = 0; i < 5; ++i)
        {
            Nebula n;
            n.size = m * (0.55f + 0.4f * rnd.nextFloat());
            n.x = -n.size / 3.0f + rnd.nextFloat() * ((float) w - n.size / 6.0f);
            n.y = -n.size / 3.0f + rnd.nextFloat() * ((float) h - n.size / 6.0f);
            n.dur = 22.0f + 16.0f * rnd.nextFloat();
            n.delay = rnd.nextFloat() * n.dur;
            n.dx = (rnd.nextBool() ? 1.0f : -1.0f) * (40.0f + 50.0f * rnd.nextFloat());
            n.dy = -40.0f + 80.0f * rnd.nextFloat();
            n.col = i % 4;
            nebulae.push_back (n);
        }

        stars.clear();
        const int numStars = juce::jmax (40, w * h / 3000);
        const juce::Colour tints[] = { juce::Colours::white, juce::Colour (0xfffff2c8), juce::Colour (0xffbfefff), theme.lid };
        for (int i = 0; i < numStars; ++i)
            stars.push_back ({ rnd.nextFloat() * (float) w, rnd.nextFloat() * (float) h, rnd.nextFloat() < 0.15f ? 1.6f : 1.0f,
                               rnd.nextFloat() * 6.28f, 2.5f + rnd.nextFloat() * 4.0f, tints[rnd.nextInt (4)] });

        eyes.clear();
        for (auto& s : spots)
        {
            const float dur = 9.0f + 8.0f * rnd.nextFloat();
            eyes.push_back ({ s.x, s.y, s.w, dur, rnd.nextFloat() * dur, rnd.nextFloat() * 40.0f });
        }
        renderSky (7.0, 0.0f);
    }

    // half-resolution sky (base + nebulae); cheap, call once per animation frame
    void renderSky (double t, float pulse)
    {
        if (! sky.isValid()) return;
        juce::Graphics g (sky);
        g.drawImageAt (base, 0, 0);
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        for (auto& n : nebulae)
        {
            const float f = (float) std::fmod ((t + n.delay) / n.dur, 1.0);
            float op, sc, tx, ty;
            nebulaFrame (f, n, op, sc, tx, ty);
            op = juce::jlimit (0.0f, 1.0f, op * (1.0f + 0.9f * pulse));
            sc *= 1.0f + 0.06f * pulse;
            if (op < 0.01f) continue;
            const float size = n.size * sc * 0.5f;
            const float cx = (n.x + n.size * 0.5f + tx) * 0.5f, cy = (n.y + n.size * 0.5f + ty) * 0.5f;
            g.setOpacity (op);
            g.drawImageTransformed (sprites[(size_t) n.col],
                                    juce::AffineTransform::scale (size / 96.0f).translated (cx - size * 0.5f, cy - size * 0.5f));
        }
    }

    void drawSky (juce::Graphics& g) const
    {
        if (! sky.isValid()) return;
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImage (sky, juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h));
    }

    // crisp still stars: drawn once into the cached static layer
    void drawStaticStars (juce::Graphics& g) const
    {
        for (size_t i = 0; i < stars.size(); i += 2)
        {
            const auto& s = stars[i];
            g.setColour (s.col.withAlpha (0.55f));
            g.fillEllipse (s.x, s.y, s.size, s.size);
        }
    }

    // twinkling stars + eyes (per frame, small)
    void drawOverlays (juce::Graphics& g, double t, bool animated) const
    {
        for (size_t i = 1; i < stars.size(); i += 2)
        {
            const auto& s = stars[i];
            const float a = animated ? 0.25f + 0.75f * (0.5f + 0.5f * std::sin ((float) t * 6.2832f / s.period + s.phase)) : 0.6f;
            g.setColour (s.col.withAlpha (a));
            g.fillEllipse (s.x, s.y, s.size + 0.4f, s.size + 0.4f);
        }
        for (auto& e : eyes)
            drawEye (g, e, t, animated);
    }

    // make every eye start opening now (called on a loud passage)
    void openEyesNow (double t)
    {
        for (size_t i = 0; i < eyes.size(); ++i)
        {
            auto& e = eyes[i];
            const double wanted = 0.40 * e.dur - 0.15 * (double) i; // cycle position just before "open"
            e.offset = (float) std::fmod (wanted - t + 1000.0 * e.dur, (double) e.dur);
        }
    }

private:
    struct Nebula { float x, y, size, dur, delay, dx, dy; int col; };
    struct Star { float x, y, size, phase, period; juce::Colour col; };
    struct Eye { float x, y, w, dur, offset, lookSeed; };

    static float seg (float f, float a, float b, float va, float vb) { return va + (vb - va) * juce::jlimit (0.0f, 1.0f, (f - a) / (b - a)); }

    static void nebulaFrame (float f, const Nebula& n, float& op, float& sc, float& tx, float& ty)
    {
        if (f < 0.5f)
        {
            op = f < 0.18f ? seg (f, 0.0f, 0.18f, 0.0f, 0.85f) : seg (f, 0.18f, 0.5f, 0.85f, 0.6f);
            sc = seg (f, 0.0f, 0.5f, 0.85f, 1.2f);
            tx = seg (f, 0.0f, 0.5f, 0.0f, n.dx); ty = seg (f, 0.0f, 0.5f, 0.0f, n.dy);
        }
        else
        {
            op = f < 0.82f ? seg (f, 0.5f, 0.82f, 0.6f, 0.85f) : seg (f, 0.82f, 1.0f, 0.85f, 0.0f);
            sc = seg (f, 0.5f, 1.0f, 1.2f, 0.9f);
            tx = seg (f, 0.5f, 1.0f, n.dx, 2.0f * n.dx); ty = seg (f, 0.5f, 1.0f, n.dy, 0.0f);
        }
    }

    static float eyeOpen (float f)
    {
        // closed most of the time, opens, blinks once, closes
        if (f < 0.38f) return 0.04f;
        if (f < 0.46f) return seg (f, 0.38f, 0.46f, 0.04f, 1.0f);
        if (f < 0.62f) return 1.0f;
        if (f < 0.64f) return seg (f, 0.62f, 0.64f, 1.0f, 0.08f);
        if (f < 0.66f) return seg (f, 0.64f, 0.66f, 0.08f, 1.0f);
        if (f < 0.80f) return 1.0f;
        if (f < 0.88f) return seg (f, 0.80f, 0.88f, 1.0f, 0.04f);
        return 0.04f;
    }

    void drawEye (juce::Graphics& g, const Eye& e, double t, bool animated) const
    {
        const float f = animated ? (float) std::fmod ((t + e.offset) / e.dur, 1.0) : 0.5f;
        const float open = eyeOpen (f);
        const float ew = e.w, eh = e.w * 0.5f, cx = e.x + ew * 0.5f, cy = e.y + eh * 0.5f;
        if (open < 0.06f)
        {
            g.setColour (theme.lid.withAlpha (0.35f));
            g.drawLine (e.x + ew * 0.08f, cy, e.x + ew * 0.92f, cy, 1.5f);
            return;
        }
        juce::Path almond;
        const float hh = eh * 0.78f * open;
        almond.startNewSubPath (e.x + ew * 0.02f, cy);
        almond.quadraticTo (cx, cy - hh * 1.1f, e.x + ew * 0.98f, cy);
        almond.quadraticTo (cx, cy + hh * 1.1f, e.x + ew * 0.02f, cy);
        almond.closeSubPath();

        g.setColour (theme.glow.withAlpha (0.35f));
        g.strokePath (almond, juce::PathStrokeType (5.0f));
        g.setColour (juce::Colour (0xffe6dcef));
        g.fillPath (almond);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (almond);
            const float tt = (float) t;
            const float lx = std::sin (tt * 0.23f + e.lookSeed) * ew * 0.12f, ly = std::cos (tt * 0.17f + e.lookSeed) * eh * 0.08f;
            const float r = eh * 0.68f;
            const juce::Point<float> c (cx + lx, cy + ly);
            juce::ColourGradient ig (theme.iris[0], c.x - r * 0.3f, c.y - r * 0.4f, theme.iris[2], c.x + r, c.y + r, true);
            ig.addColour (0.6, theme.iris[1]);
            g.setGradientFill (ig);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
            g.setColour (juce::Colour (0xff09030f));
            g.fillEllipse (juce::Rectangle<float> (r * 0.88f, r * 0.88f).withCentre (c));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (r * 0.4f, r * 0.4f).withCentre (c.translated (r * 0.3f, -r * 0.35f)));
        }
        g.setColour (theme.lid.withAlpha (0.8f));
        g.strokePath (almond, juce::PathStrokeType (1.3f));
    }

    int w = 1, h = 1;
    Theme theme;
    std::array<juce::Image, 4> sprites;
    juce::Image base, sky;
    std::vector<Nebula> nebulae;
    std::vector<Star> stars;
    std::vector<Eye> eyes;
};

//==============================================================================
// Small "i": hover it to read an explanation (tooltip)
// Big explanation card opened by a click on an "i": title, short line, then the details.
// Text format: "short line||details" where details are paragraphs split by '|',
// and a paragraph "WORD : text" gets its WORD in bold.
class InfoCard : public juce::Component
{
public:
    InfoCard (const juce::String& titleIn, const juce::String& shortIn, const juce::String& longIn)
        : title (titleIn.toUpperCase()), shortText (shortIn), longText (longIn)
    {
        ok.setButtonText (TK_FR ("Compris"));
        ok.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffffd34d));
        ok.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a0f08));
        ok.onClick = [this] { dismiss(); };
        addAndMakeVisible (ok);
        setWantsKeyboardFocus (true);
    }
    void dismiss() { auto safe = juce::Component::SafePointer<InfoCard> (this); setVisible (false); juce::MessageManager::callAsync ([safe] { if (safe != nullptr) if (auto* p = safe->getParentComponent()) p->removeChildComponent (safe.getComponent()); }); }
    juce::Rectangle<float> cardBounds() const
    {
        const float w = juce::jmin (440.0f, (float) getWidth() - 40.0f);
        const float h = juce::jmin ((float) getHeight() - 40.0f, 130.0f + layout (w - 48.0f).getHeight());
        return juce::Rectangle<float> (w, h).withCentre (getLocalBounds().toFloat().getCentre());
    }
    juce::TextLayout layout (float width) const
    {
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        as.setLineSpacing (3.0f);
        const auto txt = juce::Colour (0xfff4ecff), gold = juce::Colour (0xffffd34d);
        auto paras = juce::StringArray::fromTokens (longText, "|", "");
        paras.trim(); paras.removeEmptyStrings();
        for (int i = 0; i < paras.size(); ++i)
        {
            auto para = paras[i];
            const int colon = para.indexOf (" : ");
            if (colon > 1 && colon < 34 && para.substring (0, colon) == para.substring (0, colon).toUpperCase())
            {
                as.append (para.substring (0, colon) + " : ", TchiskibidiLookAndFeel::font (14.0f, true), gold);
                para = para.substring (colon + 3);
            }
            as.append (para + (i + 1 < paras.size() ? "\n\n" : ""), TchiskibidiLookAndFeel::font (14.0f, false), txt);
        }
        juce::TextLayout tl; tl.createLayout (as, width); return tl;
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff04020a).withAlpha (0.6f));
        const auto r = cardBounds();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d0f2e), r.getX(), r.getY(), juce::Colour (0xff0c0616), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 20.0f);
        g.setColour (juce::Colour (0xffffd34d).withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.75f), 20.0f, 1.5f);
        auto in = r.reduced (24.0f, 20.0f);
        g.setColour (juce::Colour (0xfff4ecff).withAlpha (0.5f));
        g.setFont (TchiskibidiLookAndFeel::font (11.0f));
        g.drawText (title, in.removeFromTop (16.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffffd34d));
        g.setFont (TchiskibidiLookAndFeel::font (18.0f));
        g.drawFittedText (shortText, in.removeFromTop (48.0f).toNearestInt(), juce::Justification::centredLeft, 2);
        in.removeFromBottom (48.0f);
        layout (in.getWidth()).draw (g, in);
    }
    void resized() override
    {
        const auto r = cardBounds();
        ok.setBounds (juce::Rectangle<float> (r.getX() + 24.0f, r.getBottom() - 58.0f, r.getWidth() - 48.0f, 38.0f).toNearestInt());
    }
    void mouseDown (const juce::MouseEvent& e) override { if (! cardBounds().contains (e.position)) dismiss(); }
    bool keyPressed (const juce::KeyPress& k) override { if (k == juce::KeyPress::escapeKey || k == juce::KeyPress::returnKey) { dismiss(); return true; } return false; }
private:
    juce::String title, shortText, longText;
    juce::TextButton ok;
};

// The little "i": hovering shows the short line, a click opens the full card
class InfoIcon : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit InfoIcon (const juce::String& text, const juce::String& titleIn = {}) : title (titleIn)
    {
        const int sep = text.indexOf ("||");
        if (sep >= 0) { shortText = text.substring (0, sep).trim(); longText = text.substring (sep + 2).trim(); }
        else
        {
            longText = text;
            int end = -1;
            for (int i = 0; i < text.length() - 1; ++i) if ((text[i] == '.' || text[i] == '!' || text[i] == '?') && text[i + 1] == ' ') { end = i; break; }
            shortText = end > 0 ? text.substring (0, end + 1) : text;
        }
        setTooltip (shortText + "\n" + TK_FR ("(clique le i pour en savoir plus)"));
        setSize (18, 18);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool over = isMouseOver();
        g.setColour (over ? juce::Colour (0xffffd34d).withAlpha (0.25f) : juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (r);
        g.setColour (juce::Colour (0xfff4ecff).withAlpha (over ? 1.0f : 0.55f));
        g.drawEllipse (r, 1.4f);
        // the "i" drawn as shapes (no font needed): dot + slanted stem
        const auto c = r.getCentre();
        g.fillEllipse (c.x - 1.3f + 0.6f, c.y - 5.6f, 2.6f, 2.6f);
        juce::Path stem;
        stem.startNewSubPath (c.x + 0.4f, c.y - 1.6f);
        stem.lineTo (c.x - 0.6f, c.y + 4.6f);
        g.strokePath (stem, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        auto* host = getParentComponent();
        if (host == nullptr) return;
        auto* card = new InfoCard (title, shortText, longText);
        host->addAndMakeVisible (card); // the parent owns it until it is dismissed
        card->setBounds (host->getLocalBounds());
        card->toFront (true);
        card->grabKeyboardFocus();
    }
    juce::String title, shortText, longText;
};

//==============================================================================
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("settings") { setTooltip (TK_FR ("Réglages")); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const auto c = r.getCentre();
        const float ro = r.getWidth() * 0.5f, ri = ro * 0.72f;
        juce::Path gear;
        const int teeth = 8;
        for (int i = 0; i < teeth * 2; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * (float) i / (float) (teeth * 2);
            const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / (float) (teeth * 2);
            const float rad = (i % 2 == 0) ? ro : ri;
            const auto p0 = c.getPointOnCircumference (rad, a0 + 0.06f), p1 = c.getPointOnCircumference (rad, a1 - 0.06f);
            if (i == 0) gear.startNewSubPath (p0); else gear.lineTo (p0);
            gear.lineTo (p1);
        }
        gear.closeSubPath();
        gear.addEllipse (juce::Rectangle<float> (ro * 0.7f, ro * 0.7f).withCentre (c));
        gear.setUsingNonZeroWinding (false);
        g.setColour (juce::Colour (0xfff4ecff).withAlpha (down ? 1.0f : (over ? 0.95f : 0.6f)));
        g.fillPath (gear);
    }
};

//==============================================================================
struct SettingsExtra
{
    juce::String label;
    juce::StringArray choices;
    std::function<int()> get;
    std::function<void (int)> set;
};

class SettingsPanel : public juce::Component
{
public:
    SettingsPanel (const juce::String& pluginTitle, std::vector<SettingsExtra> extrasIn,
                   std::function<float()> getScaleIn, std::function<void (float)> setScaleIn, std::function<void()> resetIn)
        : title (pluginTitle), extras (std::move (extrasIn)), getScale (std::move (getScaleIn)), setScale (std::move (setScaleIn)), resetAll (std::move (resetIn))
    {
        auto addToggle = [this] (juce::ToggleButton& b, bool on, std::function<void (bool)> fn)
        {
            b.setClickingTogglesState (true);
            b.setToggleState (on, juce::dontSendNotification);
            b.setButtonText (on ? "ON" : "OFF");
            b.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffc6ff3d));
            b.onClick = [&b, fn] { b.setButtonText (b.getToggleState() ? "ON" : "OFF"); fn (b.getToggleState()); };
            addAndMakeVisible (b);
        };
        addToggle (animToggle, settings->animations(), [this] (bool v) { settings->set ("animations", v); });
        addToggle (reactToggle, settings->reactive(), [this] (bool v) { settings->set ("reactive", v); });
        addToggle (infoToggle, settings->infoBubbles(), [this] (bool v) { settings->set ("info", v); });

        fpsBox.addItemList (juce::StringArray { TK_FR ("Éco (12 i/s)"), juce::String ("Normale (24 i/s)"), juce::String ("Fluide (40 i/s)") }, 1);
        const int fps = settings->fps();
        fpsBox.setSelectedId (fps <= 14 ? 1 : (fps <= 30 ? 2 : 3), juce::dontSendNotification);
        fpsBox.onChange = [this] { const int v[] = { 12, 24, 40 }; settings->set ("fps", v[juce::jlimit (0, 2, fpsBox.getSelectedId() - 1)]); };
        addAndMakeVisible (fpsBox);

        const float scales[] = { 0.6f, 0.75f, 0.85f, 1.0f, 1.25f, 1.5f };
        for (int i = 0; i < 6; ++i) scaleBox.addItem (juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", i + 1);
        int best = 4; float bd = 9.0f;
        for (int i = 0; i < 6; ++i) if (std::abs (scales[i] - getScale()) < bd) { bd = std::abs (scales[i] - getScale()); best = i + 1; }
        scaleBox.setSelectedId (best, juce::dontSendNotification);
        scaleBox.onChange = [this, scales] { setScale (scales[juce::jlimit (0, 5, scaleBox.getSelectedId() - 1)]); };
        addAndMakeVisible (scaleBox);

        for (auto& e : extras)
        {
            auto* b = extraBoxes.add (new juce::ComboBox());
            b->addItemList (e.choices, 1);
            b->setSelectedId (e.get() + 1, juce::dontSendNotification);
            auto setter = e.set;
            b->onChange = [b, setter] { setter (b->getSelectedId() - 1); };
            addAndMakeVisible (b);
        }

        resetButton.onClick = [this]
        {
            if (! confirmReset) { confirmReset = true; resetButton.setButtonText (TK_FR ("SÛR ? CLIQUE ENCORE")); return; }
            confirmReset = false; resetButton.setButtonText (TK_FR ("RÉINITIALISER LE PLUGIN"));
            if (resetAll) resetAll();
        };
        closeButton.onClick = [this] { setVisible (false); };
        closeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffff4fd8));
        closeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff120a1f));
        addAndMakeVisible (resetButton);
        addAndMakeVisible (closeButton);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.6f));
        const auto card = cardBounds().toFloat();
        g.setColour (juce::Colour (0xff140a24).withAlpha (0.97f));
        g.fillRoundedRectangle (card, 16.0f);
        g.setColour (juce::Colour (0xffff4fd8).withAlpha (0.45f));
        g.drawRoundedRectangle (card.reduced (0.5f), 16.0f, 1.3f);

        auto r = card.reduced (22.0f, 16.0f);
        g.setColour (juce::Colour (0xffa898c4));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText ("T C H I S K I B I D I   " + title.toUpperCase(), r.removeFromTop (14.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xfff4ecff));
        g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        g.drawText (TK_FR ("RÉGLAGES"), r.removeFromTop (30.0f), juce::Justification::centredLeft);

        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        for (auto& row : rowLabels)
        {
            g.setColour (row.second ? juce::Colour (0xffa898c4) : juce::Colour (0xfff4ecff));
            g.drawText (row.first.text, row.first.area, juce::Justification::centredLeft);
        }
        g.setColour (juce::Colour (0xffa898c4));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::plain)));
        g.drawText (TK_FR ("Les réglages d'affichage valent pour tous les plugins Tchiskibidi."), footerArea, juce::Justification::centred);
    }

    void resized() override
    {
        rowLabels.clear();
        auto r = cardBounds().reduced (22, 16);
        r.removeFromTop (54);
        auto row = [this, &r] (const juce::String& text, juce::Component& c, int cw, bool dim = false)
        {
            auto line = r.removeFromTop (34);
            c.setBounds (line.removeFromRight (cw).withSizeKeepingCentre (cw, 26));
            rowLabels.push_back ({ { text, line.toFloat() }, dim });
        };
        row ("Animations du ciel", animToggle, 70);
        row (TK_FR ("Fluidité des animations"), fpsBox, 160);
        row (TK_FR ("Ciel réactif à la musique"), reactToggle, 70);
        row (TK_FR ("Bulles d'info « i »"), infoToggle, 70);
        row (TK_FR ("Taille de la fenêtre"), scaleBox, 110);
        for (int i = 0; i < extraBoxes.size(); ++i)
            row (extras[(size_t) i].label, *extraBoxes[i], 190);
        r.removeFromTop (8);
        resetButton.setBounds (r.removeFromTop (30));
        r.removeFromTop (8);
        closeButton.setBounds (r.removeFromTop (30));
        footerArea = r.removeFromTop (22).toFloat();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! cardBounds().contains (e.getPosition())) setVisible (false);
    }

private:
    juce::Rectangle<int> cardBounds() const
    {
        const int hgt = 54 + 34 * (5 + (int) extras.size()) + 8 + 30 + 8 + 30 + 22 + 32;
        return getLocalBounds().withSizeKeepingCentre (juce::jmin (420, getWidth() - 20), juce::jmin (hgt, getHeight() - 20));
    }

    struct Label { juce::String text; juce::Rectangle<float> area; };
    juce::SharedResourcePointer<UISettings> settings;
    juce::String title;
    std::vector<SettingsExtra> extras;
    std::function<float()> getScale;
    std::function<void (float)> setScale;
    std::function<void()> resetAll;
    juce::ToggleButton animToggle, reactToggle, infoToggle;
    juce::ComboBox fpsBox, scaleBox;
    juce::OwnedArray<juce::ComboBox> extraBoxes;
    juce::TextButton resetButton { TK_FR ("RÉINITIALISER LE PLUGIN") }, closeButton { "FERMER" };
    bool confirmReset = false;
    std::vector<std::pair<Label, bool>> rowLabels;
    juce::Rectangle<float> footerArea;
};

//==============================================================================
// Reset every parameter of a plugin to its default value
inline void resetAllParameters (juce::AudioProcessor& p)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            r->beginChangeGesture();
            r->setValueNotifyingHost (r->getDefaultValue());
            r->endChangeGesture();
        }
}
} // namespace tchk
