#pragma once

// Twist: every effect is a die-cut sticker with its own little drawing.

#include "PluginProcessor.h"
#include "TchiskibidiInstrumentUI.h"

namespace twist
{
// the little drawing of each effect, in r, with the colour ink
inline void drawFxGlyph (juce::Graphics& g, int fx, juce::Rectangle<float> r, juce::Colour ink)
{
    using P = juce::Point<float>;
    const auto c = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const float lw = juce::jmax (1.6f, s * 0.13f);
    const juce::PathStrokeType st (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (ink);
    auto line = [&] (P a, P b) { juce::Path p; p.startNewSubPath (a); p.lineTo (b); g.strokePath (p, st); };
    auto at = [&] (float x, float y) { return P (c.x + x * s, c.y + y * s); };
    auto circle = [&] (float x, float y, float rr, bool fill) { auto q = at (x, y); juce::Rectangle<float> b (q.x - rr * s, q.y - rr * s, rr * s * 2, rr * s * 2); if (fill) g.fillEllipse (b); else g.drawEllipse (b, lw); };
    auto tri = [&] (P a, P b, P d) { juce::Path p; p.addTriangle (a, b, d); g.fillPath (p); };
    auto vinyl = [&]
    {
        circle (0, 0, 0.8f, false); circle (0, 0, 0.5f, false); circle (0, 0, 0.12f, true);
    };
    switch (fx)
    {
        case TapeStop: case TapeStart:   // cassette
        {
            juce::Path b; b.addRoundedRectangle (at (-0.85f, -0.55f).x, at (-0.85f, -0.55f).y, 1.7f * s, 1.1f * s, 0.15f * s); g.strokePath (b, st);
            circle (-0.38f, -0.08f, 0.18f, false); circle (0.38f, -0.08f, 0.18f, false);
            line (at (-0.45f, 0.3f), at (0.45f, 0.3f));
            if (fx == TapeStart) tri (at (-0.2f, -0.95f), at (0.25f, -0.75f), at (-0.2f, -0.55f));
            else { line (at (0.55f, -0.95f), at (0.55f, -0.62f)); line (at (0.75f, -0.95f), at (0.75f, -0.62f)); }
            break;
        }
        case BulletTime:   // a bullet with speed lines
        {
            juce::Path b; b.startNewSubPath (at (-0.3f, -0.25f)); b.lineTo (at (0.35f, -0.25f)); b.quadraticTo (at (0.85f, 0), at (0.35f, 0.25f)); b.lineTo (at (-0.3f, 0.25f)); b.closeSubPath(); g.fillPath (b);
            for (float y : { -0.45f, 0.0f, 0.45f }) line (at (-0.9f, y), at (-0.5f, y));
            break;
        }
        case Rewind:
            tri (at (-0.05f, -0.5f), at (-0.05f, 0.5f), at (-0.75f, 0)); tri (at (0.7f, -0.5f), at (0.7f, 0.5f), at (0.0f, 0)); break;
        case Scratch:
            vinyl(); line (at (0.2f, -0.9f), at (0.85f, 0.4f)); break;
        case Skip:   // scratched record
            vinyl(); { juce::Path z; z.startNewSubPath (at (-0.7f, -0.2f)); z.lineTo (at (-0.3f, -0.05f)); z.lineTo (at (-0.1f, -0.35f)); z.lineTo (at (0.3f, -0.1f)); g.strokePath (z, st); } break;
        case Accordion:   // bellows
        {
            juce::Path z; z.startNewSubPath (at (-0.85f, -0.5f));
            for (int i = 0; i < 6; ++i) z.lineTo (at (-0.85f + 0.34f * (float) (i + 1), i % 2 ? -0.5f : 0.5f));
            g.strokePath (z, st);
            line (at (-0.85f, -0.5f), at (-0.85f, 0.5f)); line (at (0.85f, -0.5f), at (0.85f, 0.5f));
            break;
        }
        case Freeze:   // snowflake
            for (int i = 0; i < 3; ++i)
            {
                const float a = (float) i * juce::MathConstants<float>::pi / 3.0f;
                line (c + P (std::cos (a), std::sin (a)) * s * 0.85f, c - P (std::cos (a), std::sin (a)) * s * 0.85f);
                for (float sg : { 1.0f, -1.0f })
                {
                    const auto m = c + P (std::cos (a), std::sin (a)) * s * 0.55f * sg;
                    line (m, m + P (std::cos (a + 0.8f), std::sin (a + 0.8f)) * s * 0.25f * sg);
                    line (m, m + P (std::cos (a - 0.8f), std::sin (a - 0.8f)) * s * 0.25f * sg);
                }
            }
            break;
        case Reverse:   // U-turn arrow
        {
            juce::Path u; u.startNewSubPath (at (0.55f, 0.6f)); u.lineTo (at (0.55f, -0.1f)); u.cubicTo (at (0.55f, -0.75f), at (-0.55f, -0.75f), at (-0.55f, -0.1f)); u.lineTo (at (-0.55f, 0.3f));
            g.strokePath (u, st); tri (at (-0.85f, 0.2f), at (-0.25f, 0.2f), at (-0.55f, 0.75f));
            break;
        }
        case Roll:   // burst of bars
            for (int i = 0; i < 4; ++i) { const float x = -0.75f + 0.48f * (float) i, h = 0.3f + 0.2f * (float) i; g.fillRect (juce::Rectangle<float> (at (x, 0.7f - h * 2).x, at (x, 0.7f - h * 2).y, s * 0.3f, s * h * 2)); }
            break;
        case Stutter:
            for (int i = 0; i < 5; ++i) g.fillRoundedRectangle (at (-0.85f + 0.36f * (float) i, -0.35f).x, at (0, -0.35f).y, s * 0.2f, s * 0.7f, s * 0.06f);
            break;
        case Hiccup:   // bubbles
            circle (-0.35f, 0.3f, 0.32f, false); circle (0.3f, -0.15f, 0.22f, false); circle (0.65f, -0.6f, 0.12f, true); break;
        case Glitch:
            g.fillRect (juce::Rectangle<float> (at (-0.8f, -0.6f), at (0.2f, -0.2f))); g.fillRect (juce::Rectangle<float> (at (-0.4f, -0.1f), at (0.8f, 0.25f)));
            g.fillRect (juce::Rectangle<float> (at (-0.7f, 0.35f), at (0.3f, 0.7f))); break;
        case Bug8:   // pixel bug
        {
            const char* px[] = { "0100010", "0011100", "1111111", "0110110", "1111111", "0101010" };
            const float cell = s * 0.26f;
            for (int y = 0; y < 6; ++y) for (int x = 0; x < 7; ++x)
                if (px[y][x] == '1') g.fillRect (c.x + ((float) x - 3.5f) * cell, c.y + ((float) y - 3.0f) * cell, cell - 0.6f, cell - 0.6f);
            break;
        }
        case Helium:   // balloon
        {
            g.fillEllipse (at (-0.45f, -0.9f).x, at (0, -0.9f).y, s * 0.9f, s * 1.1f);
            juce::Path str; str.startNewSubPath (at (0, 0.2f)); str.cubicTo (at (0.25f, 0.45f), at (-0.25f, 0.6f), at (0.05f, 0.9f)); g.strokePath (str, juce::PathStrokeType (lw * 0.7f));
            break;
        }
        case Demon:   // horns
        {
            circle (0, 0.2f, 0.55f, false);
            tri (at (-0.5f, -0.15f), at (-0.2f, -0.3f), at (-0.65f, -0.85f)); tri (at (0.5f, -0.15f), at (0.2f, -0.3f), at (0.65f, -0.85f));
            circle (-0.2f, 0.12f, 0.08f, true); circle (0.2f, 0.12f, 0.08f, true);
            break;
        }
        case Robot:
        {
            juce::Path h; h.addRoundedRectangle (at (-0.65f, -0.5f).x, at (0, -0.5f).y, s * 1.3f, s * 1.1f, s * 0.15f); g.strokePath (h, st);
            g.fillRect (juce::Rectangle<float> (at (-0.4f, -0.2f), at (-0.12f, 0.05f))); g.fillRect (juce::Rectangle<float> (at (0.12f, -0.2f), at (0.4f, 0.05f)));
            line (at (-0.3f, 0.32f), at (0.3f, 0.32f)); line (at (0, -0.5f), at (0, -0.8f)); circle (0, -0.88f, 0.1f, true);
            break;
        }
        case Elevator:   // up / down arrows in a box
        {
            juce::Path b; b.addRectangle (juce::Rectangle<float> (at (-0.6f, -0.8f), at (0.6f, 0.8f))); g.strokePath (b, st);
            tri (at (-0.3f, -0.1f), at (0.3f, -0.1f), at (0, -0.55f)); tri (at (-0.3f, 0.1f), at (0.3f, 0.1f), at (0, 0.55f));
            break;
        }
        case Arp:   // stairs
        {
            juce::Path z; z.startNewSubPath (at (-0.85f, 0.7f));
            for (int i = 0; i < 4; ++i) { z.lineTo (at (-0.85f + 0.42f * (float) i, 0.7f - 0.4f * (float) (i + 1))); z.lineTo (at (-0.85f + 0.42f * (float) (i + 1), 0.7f - 0.4f * (float) (i + 1))); }
            g.strokePath (z, st);
            break;
        }
        case PitchDrop:
        {
            juce::Path d; d.startNewSubPath (at (-0.8f, -0.6f)); d.cubicTo (at (0.1f, -0.6f), at (0.3f, -0.2f), at (0.4f, 0.4f)); g.strokePath (d, st);
            tri (at (0.1f, 0.3f), at (0.7f, 0.3f), at (0.42f, 0.85f));
            break;
        }
        case Spin:   // spinning top
        {
            juce::Path t; t.startNewSubPath (at (-0.7f, -0.2f)); t.quadraticTo (at (0, -0.65f), at (0.7f, -0.2f)); t.lineTo (at (0, 0.8f)); t.closeSubPath(); g.fillPath (t);
            line (at (0, -0.45f), at (0, -0.9f));
            juce::Path a; a.addCentredArc (c.x, c.y - s * 0.1f, s * 0.95f, s * 0.35f, 0, -2.4f, 0.6f, true); g.strokePath (a, juce::PathStrokeType (lw * 0.7f));
            break;
        }
        case BlackHole:   // spiral
        {
            juce::Path sp;
            for (int i = 0; i <= 60; ++i) { const float u = (float) i / 60.0f, a = u * 12.0f, rr = 0.9f * (1.0f - u); const auto q = c + P (std::cos (a), std::sin (a)) * rr * s; if (i == 0) sp.startNewSubPath (q); else sp.lineTo (q); }
            g.strokePath (sp, st);
            break;
        }
        case Ghost:
        {
            juce::Path gh; gh.startNewSubPath (at (-0.6f, 0.8f)); gh.lineTo (at (-0.6f, -0.2f)); gh.cubicTo (at (-0.6f, -0.95f), at (0.6f, -0.95f), at (0.6f, -0.2f)); gh.lineTo (at (0.6f, 0.8f));
            gh.lineTo (at (0.3f, 0.55f)); gh.lineTo (at (0, 0.8f)); gh.lineTo (at (-0.3f, 0.55f)); gh.closeSubPath(); g.fillPath (gh);
            g.setColour (juce::Colours::white); circle (-0.22f, -0.2f, 0.12f, true); circle (0.22f, -0.2f, 0.12f, true);
            break;
        }
        case Aquarium:   // fish + bubble
        {
            juce::Path f; f.addEllipse (at (-0.65f, -0.35f).x, at (0, -0.35f).y, s * 1.0f, s * 0.7f); g.fillPath (f);
            tri (at (0.3f, 0), at (0.85f, -0.4f), at (0.85f, 0.4f));
            g.setColour (juce::Colours::white); circle (-0.38f, -0.06f, 0.08f, true); g.setColour (ink);
            circle (-0.5f, -0.75f, 0.12f, false);
            break;
        }
        case Radio:
        {
            juce::Path b; b.addRoundedRectangle (at (-0.8f, -0.3f).x, at (0, -0.3f).y, s * 1.6f, s * 1.0f, s * 0.15f); g.strokePath (b, st);
            circle (-0.35f, 0.2f, 0.22f, false);
            for (float y : { 0.0f, 0.2f, 0.4f }) line (at (0.15f, y), at (0.55f, y));
            line (at (0.3f, -0.3f), at (0.7f, -0.9f));
            break;
        }
        default: circle (0, 0, 0.5f, true); break;
    }
}

// sticker outline, a different cut for each effect
inline juce::Path stickerShape (int fx, juce::Rectangle<float> r)
{
    switch (fx % 6)
    {
        case 0: { juce::Path p; p.addEllipse (r); return p; }
        case 1: { juce::Path p; p.addRoundedRectangle (r, r.getWidth() * 0.22f); return p; }
        case 2: return tchk::shapes::starPath (r.getCentre(), r.getWidth() * 0.56f, r.getWidth() * 0.36f, 0.0f);
        case 3: return tchk::shapes::blobPath (r, fx * 13, 0.09f, 7);
        case 4: return tchk::shapes::hexPath (r.withSizeKeepingCentre (r.getWidth() * 1.08f, r.getHeight() * 0.92f));
        default:
        {
            // ticket
            juce::Path p; p.addRoundedRectangle (r.reduced (0, r.getHeight() * 0.1f), r.getHeight() * 0.18f);
            return p;
        }
    }
}

// one effect: a sticker you click to choose it
class Sticker : public juce::Button
{
public:
    explicit Sticker (int fxIn) : juce::Button (fxNames()[fxIn]), fx (fxIn)
    {
        juce::Random r (fx * 977 + 3);
        tilt = (r.nextFloat() - 0.5f) * 0.28f;
    }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState();
        const auto col = fxColour (fx);
        auto area = getLocalBounds().toFloat();
        auto icon = area.removeFromTop (area.getHeight() - 15.0f).reduced (6.0f, 3.0f);
        icon = icon.withSizeKeepingCentre (juce::jmin (icon.getWidth(), icon.getHeight()), juce::jmin (icon.getWidth(), icon.getHeight()));
        const float a = on ? 0.0f : tilt * (over ? 0.4f : 1.0f);
        const float sc = on ? 1.1f : (down ? 0.94f : 1.0f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (a, icon.getCentreX(), icon.getCentreY()).scaled (sc, sc, icon.getCentreX(), icon.getCentreY()));
        const auto shape = stickerShape (fx, icon.reduced (2.0f));
        // shadow, white die-cut border, colour, drawing
        g.setColour (juce::Colours::black.withAlpha (on ? 0.5f : 0.35f));
        g.fillPath (shape, juce::AffineTransform::translation (on ? 3.0f : 2.0f, on ? 4.0f : 2.5f));
        g.setColour (juce::Colours::white); g.strokePath (shape, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); g.fillPath (shape);
        g.setGradientFill (juce::ColourGradient (col.brighter (0.35f), icon.getX(), icon.getY(), col.darker (0.15f), icon.getRight(), icon.getBottom(), false));
        g.fillPath (shape);
        drawFxGlyph (g, fx, icon.reduced (icon.getWidth() * 0.24f), juce::Colour (0xff1a0d2e));
        // glossy corner
        g.saveState(); g.reduceClipRegion (shape);
        juce::Path gloss; gloss.addTriangle (icon.getX(), icon.getY(), icon.getX() + icon.getWidth() * 0.6f, icon.getY(), icon.getX(), icon.getY() + icon.getHeight() * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillPath (gloss);
        g.restoreState();
        g.restoreState();
        // the name, on a little label
        const auto lab = area.reduced (1.0f, 0.5f);
        g.setColour (on ? juce::Colours::white : juce::Colours::white.withAlpha (over ? 0.85f : 0.0f));
        if (on || over) g.fillRoundedRectangle (lab, 3.0f);
        g.setColour (on || over ? juce::Colour (0xff1a0d2e) : juce::Colour (0xffe8dcff));
        g.setFont (tchk::LNF::font (9.5f));
        g.drawFittedText (getButtonText(), lab.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
private:
    int fx;
    float tilt = 0.0f;
};
} // namespace twist
