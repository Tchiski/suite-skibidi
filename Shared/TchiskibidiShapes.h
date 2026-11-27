#pragma once

// Non-rectangular looks for the Tchiskibidi instruments:
//   Supernova: broken crystal frame, cut facet panels, hexagon chips, pearl band
//   Milky Way: orbits, moon phases
//   Fusion: lava blobs, wavy surfaces
//   Twist: torn tape, stickers
// Everything here only draws (in the cached static layer or in the components' paint).

#include "TchiskibidiLookAndFeel.h"

namespace tchk::shapes
{
using juce::Colour;
using juce::Point;
using juce::Rectangle;

// point at distance t along the perimeter of r (clockwise from the top-left corner)
inline Point<float> perimeterPoint (Rectangle<float> r, float t)
{
    const float w = r.getWidth(), h = r.getHeight(), P = 2.0f * (w + h);
    t = std::fmod (std::fmod (t, P) + P, P);
    if (t < w) return { r.getX() + t, r.getY() };
    t -= w; if (t < h) return { r.getRight(), r.getY() + t };
    t -= h; if (t < w) return { r.getRight() - t, r.getBottom() };
    t -= w; return { r.getX(), r.getBottom() - t };
}

// inward normal of the rectangle side at perimeter position t
inline Point<float> inwardNormal (Rectangle<float> r, float t)
{
    const float w = r.getWidth(), h = r.getHeight(), P = 2.0f * (w + h);
    t = std::fmod (std::fmod (t, P) + P, P);
    if (t < w) return { 0, 1 };
    if (t < w + h) return { -1, 0 };
    if (t < 2 * w + h) return { 0, -1 };
    return { 1, 0 };
}

// shade of a facet lit from the top left
inline float facetLight (Point<float> a, Point<float> b, Point<float> c)
{
    const auto n = Point<float> (-(b.y - a.y) - (c.y - a.y), (b.x - a.x) + (c.x - a.x));
    const float len = std::sqrt (n.x * n.x + n.y * n.y) + 1e-6f;
    return juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * ((-n.x - n.y) / (len * 1.414f)));
}

//==============================================================================
// SUPERNOVA: the whole window is held in a broken crystal (red, magenta, violet facets,
// glowing cracks). `hole` returns the inner outline (so nothing important is drawn under the crystal).
inline juce::Path drawCrystalFrame (juce::Graphics& g, Rectangle<float> area, int seed, float thick,
                                    std::array<Colour, 4> pal = { Colour (0xff7a0f2e), Colour (0xffc2185b), Colour (0xffe0245e), Colour (0xff5a1a8a) },
                                    Colour crack = Colour (0xffff7ab8))
{
    juce::Random r (seed);
    const float W = area.getWidth(), H = area.getHeight(), P = 2.0f * (W + H);
    std::vector<Point<float>> outer, ridge, inner;
    float t = 0;
    while (t < P - 8.0f)
    {
        const auto nrm = inwardNormal (area, t);
        // corners and some spots get big rocks that bulge into the window
        const auto pt = perimeterPoint (area, t);
        const float cornerD = juce::jmin (juce::jmin (pt.x - area.getX(), area.getRight() - pt.x) + juce::jmin (pt.y - area.getY(), area.getBottom() - pt.y), 200.0f);
        const float bulge = (cornerD < 90.0f ? 1.6f - cornerD / 150.0f : 1.0f) * (r.nextFloat() < 0.22f ? 1.45f : 1.0f);
        const float o = r.nextFloat() * thick * 0.6f;
        const float in = thick * (0.9f + r.nextFloat() * 0.6f) * bulge;
        const float rd = o + (in - o) * (0.4f + r.nextFloat() * 0.3f);
        const auto p = perimeterPoint (area, t);
        outer.push_back (p + nrm * o);
        ridge.push_back (perimeterPoint (area, t + (r.nextFloat() - 0.5f) * 14.0f) + nrm * rd);
        inner.push_back (perimeterPoint (area, t + (r.nextFloat() - 0.5f) * 18.0f) + nrm * in);
        t += 26.0f + r.nextFloat() * 30.0f;
    }
    const size_t n = outer.size();
    auto polyPath = [] (const std::vector<Point<float>>& pts)
    {
        juce::Path p; p.startNewSubPath (pts[0]); for (size_t i = 1; i < pts.size(); ++i) p.lineTo (pts[i]); p.closeSubPath(); return p;
    };
    const auto outerPath = polyPath (outer), innerPath = polyPath (inner);

    // outside the crystal: dark, so the broken silhouette reads
    {
        juce::Path outside; outside.addRectangle (area.expanded (2.0f)); outside.addPath (outerPath); outside.setUsingNonZeroWinding (false);
        g.setColour (Colour (0xff07020c)); g.fillPath (outside);
    }
    // the facets: outer -> ridge (lit side) and ridge -> inner (shadow side)
    auto facet = [&] (Point<float> a, Point<float> b, Point<float> c, float bias)
    {
        juce::Path p; p.addTriangle (a, b, c);
        const float L = juce::jlimit (0.0f, 1.0f, facetLight (a, b, c) * 0.75f + bias + (r.nextFloat() - 0.5f) * 0.25f);
        auto col = pal[(size_t) r.nextInt (4)];
        col = L > 0.5f ? col.interpolatedWith (Colour (0xffffb3d1), (L - 0.5f) * 0.9f) : col.interpolatedWith (Colour (0xff1a0412), (0.5f - L) * 1.3f);
        g.setColour (col); g.fillPath (p);
        g.setColour (Colour (0xffffc2dc).withAlpha (0.18f + 0.3f * L)); g.strokePath (p, juce::PathStrokeType (0.7f));
    };
    for (size_t i = 0; i < n; ++i)
    {
        const size_t j = (i + 1) % n;
        facet (outer[i], outer[j], ridge[i], 0.15f);
        facet (ridge[i], outer[j], ridge[j], 0.05f);
        facet (ridge[i], ridge[j], inner[i], -0.12f);
        facet (inner[i], ridge[j], inner[j], -0.2f);
    }
    // bright arêtes on the ridge line
    {
        juce::Path rl = polyPath (ridge);
        g.setColour (Colour (0xffffd6e8).withAlpha (0.35f)); g.strokePath (rl, juce::PathStrokeType (1.1f));
    }
    // glowing inner rim
    g.setColour (crack.withAlpha (0.16f)); g.strokePath (innerPath, juce::PathStrokeType (7.0f));
    g.setColour (crack.withAlpha (0.75f)); g.strokePath (innerPath, juce::PathStrokeType (1.4f));
    // a few luminous cracks running into the crystal
    for (int k = 0; k < 9; ++k)
    {
        const size_t i = (size_t) r.nextInt ((int) n);
        auto a = inner[i], b = outer[i];
        juce::Path c; c.startNewSubPath (a);
        for (int s = 1; s <= 4; ++s)
        {
            const float u = (float) s / 4.0f;
            c.lineTo (a + (b - a) * u + Point<float> ((r.nextFloat() - 0.5f) * 9.0f, (r.nextFloat() - 0.5f) * 9.0f));
        }
        g.setColour (crack.withAlpha (0.22f)); g.strokePath (c, juce::PathStrokeType (4.0f));
        g.setColour (Colour (0xfffff0f6).withAlpha (0.85f)); g.strokePath (c, juce::PathStrokeType (1.0f));
    }
    // small sparkles on the crystal
    for (int k = 0; k < 14; ++k)
    {
        const auto p = ridge[(size_t) r.nextInt ((int) n)];
        const float s = 2.0f + r.nextFloat() * 3.5f;
        juce::Path sp; sp.startNewSubPath (p.x, p.y - s); sp.lineTo (p.x + s * 0.25f, p.y - s * 0.25f); sp.lineTo (p.x + s, p.y); sp.lineTo (p.x + s * 0.25f, p.y + s * 0.25f);
        sp.lineTo (p.x, p.y + s); sp.lineTo (p.x - s * 0.25f, p.y + s * 0.25f); sp.lineTo (p.x - s, p.y); sp.lineTo (p.x - s * 0.25f, p.y - s * 0.25f); sp.closeSubPath();
        g.setColour (Colour (0xfffff3f8).withAlpha (0.9f)); g.fillPath (sp);
    }
    return innerPath;
}

// cut-gem panel: chamfered corners of different sizes, glowing border, lit facets on the corners
inline juce::Path facetPath (Rectangle<float> r, float cut, int seed = 0)
{
    juce::Random rnd (seed);
    auto c = [&] { return cut * (0.6f + rnd.nextFloat() * 0.8f); };
    const float a = c(), b = c(), d = c(), e = c();
    juce::Path p;
    p.startNewSubPath (r.getX() + a, r.getY());
    p.lineTo (r.getRight() - b, r.getY()); p.lineTo (r.getRight(), r.getY() + b * 0.9f);
    p.lineTo (r.getRight(), r.getBottom() - d * 0.9f); p.lineTo (r.getRight() - d, r.getBottom());
    p.lineTo (r.getX() + e, r.getBottom()); p.lineTo (r.getX(), r.getBottom() - e * 0.9f);
    p.lineTo (r.getX(), r.getY() + a * 0.9f); p.closeSubPath();
    return p;
}

inline void drawFacetPanel (juce::Graphics& g, Rectangle<float> r, Colour accent, float cut = 22.0f, int seed = 0, float fillAlpha = 0.86f)
{
    const auto p = facetPath (r, cut, seed);
    g.setColour (accent.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (12.0f));
    g.setGradientFill (juce::ColourGradient (Colour (0xff2c1656).withAlpha (fillAlpha), r.getX(), r.getY(), Colour (0xff12082a).withAlpha (fillAlpha), r.getX(), r.getBottom(), false));
    g.fillPath (p);
    // lit facets in the top corners
    {
        juce::Path f1; f1.addTriangle (r.getX(), r.getY() + cut * 2.2f, r.getX() + cut * 2.6f, r.getY(), r.getX() + cut * 0.6f, r.getY() + cut * 0.6f);
        juce::Path f2; f2.addTriangle (r.getRight(), r.getY() + cut * 1.6f, r.getRight() - cut * 3.4f, r.getY(), r.getRight() - cut * 0.7f, r.getY() + cut * 0.5f);
        g.saveState(); g.reduceClipRegion (p);
        g.setColour (accent.withAlpha (0.16f)); g.fillPath (f1);
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillPath (f2);
        // a diagonal sheen
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.05f), r.getCentreX(), r.getCentreY(), false));
        g.fillRect (r);
        g.restoreState();
    }
    g.setColour (accent.withAlpha (0.28f)); g.strokePath (p, juce::PathStrokeType (4.5f));
    g.setColour (accent.brighter (0.25f)); g.strokePath (p, juce::PathStrokeType (1.5f));
    const auto inner = facetPath (r.reduced (6.0f), cut * 0.8f, seed);
    g.setColour (accent.withAlpha (0.22f)); g.strokePath (inner, juce::PathStrokeType (0.8f));
}

// pearl band (the MIDI strip at the bottom of Supernova)
inline void drawPearlBand (juce::Graphics& g, Rectangle<float> r, Colour edge)
{
    juce::Path p;
    const float s = r.getHeight() * 0.45f;
    p.startNewSubPath (r.getX() + s, r.getY()); p.lineTo (r.getRight() - s * 0.4f, r.getY()); p.lineTo (r.getRight(), r.getCentreY());
    p.lineTo (r.getRight() - s, r.getBottom()); p.lineTo (r.getX() + s * 0.4f, r.getBottom()); p.lineTo (r.getX(), r.getCentreY()); p.closeSubPath();
    g.setColour (edge.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (8.0f));
    juce::ColourGradient grad (Colour (0xfff6f1ff), r.getX(), r.getY(), Colour (0xffb2a5d6), r.getX(), r.getBottom(), false);
    grad.addColour (0.45, Colour (0xffe1d8f4));
    g.setGradientFill (grad); g.fillPath (p);
    // nacre shimmer
    g.saveState(); g.reduceClipRegion (p);
    for (int i = 0; i < 6; ++i)
    {
        const float x = r.getX() + r.getWidth() * (0.08f + 0.17f * (float) i);
        juce::Path s1; s1.addTriangle (x, r.getY(), x + 60, r.getY(), x + 20, r.getBottom());
        g.setColour ((i % 2 ? Colour (0xffffd6f0) : Colour (0xffc8f0ff)).withAlpha (0.18f)); g.fillPath (s1);
    }
    g.restoreState();
    g.setColour (edge); g.strokePath (p, juce::PathStrokeType (1.6f));
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawLine (r.getX() + s + 4, r.getY() + 2.0f, r.getRight() - s * 0.4f - 4, r.getY() + 2.0f, 1.0f);
}

// hexagon (points left and right)
inline juce::Path hexPath (Rectangle<float> r)
{
    const float k = juce::jmin (r.getHeight() * 0.42f, r.getWidth() * 0.2f);
    juce::Path p;
    p.startNewSubPath (r.getX() + k, r.getY()); p.lineTo (r.getRight() - k, r.getY()); p.lineTo (r.getRight(), r.getCentreY());
    p.lineTo (r.getRight() - k, r.getBottom()); p.lineTo (r.getX() + k, r.getBottom()); p.lineTo (r.getX(), r.getCentreY()); p.closeSubPath();
    return p;
}

//==============================================================================
// FUSION: a smooth blob around a rectangle (lava), different for each seed
inline juce::Path blobPath (Rectangle<float> r, int seed, float wobble = 0.08f, int points = 9, float phase = 0.0f)
{
    juce::Random rnd (seed);
    std::vector<Point<float>> pts;
    const auto c = r.getCentre();
    for (int i = 0; i < points; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) points + rnd.nextFloat() * 0.25f;
        const float k = 1.0f + (rnd.nextFloat() - 0.5f) * 2.0f * wobble + wobble * 0.6f * std::sin (phase + (float) i * 1.7f);
        // superellipse so the blob fills the rectangle
        const float cx = std::cos (a), sy = std::sin (a);
        const float ex = std::copysign (std::pow (std::abs (cx), 0.55f), cx), ey = std::copysign (std::pow (std::abs (sy), 0.55f), sy);
        pts.push_back ({ c.x + ex * r.getWidth() * 0.5f * k, c.y + ey * r.getHeight() * 0.5f * k });
    }
    juce::Path p;
    const int n = (int) pts.size();
    auto mid = [&] (int i) { return (pts[(size_t) ((i + n) % n)] + pts[(size_t) ((i + 1 + n) % n)]) * 0.5f; };
    p.startNewSubPath (mid (0));
    for (int i = 1; i <= n; ++i) p.quadraticTo (pts[(size_t) (i % n)], mid (i));
    p.closeSubPath();
    return p;
}

inline void drawLavaPanel (juce::Graphics& g, Rectangle<float> r, Colour accent, int seed)
{
    const auto p = blobPath (r, seed, 0.05f, 10);
    g.setColour (accent.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (14.0f));
    g.setGradientFill (juce::ColourGradient (Colour (0xff3a0d2c).withAlpha (0.9f), r.getCentreX(), r.getY(), Colour (0xff1a0614).withAlpha (0.92f), r.getCentreX(), r.getBottom(), false));
    g.fillPath (p);
    g.saveState(); g.reduceClipRegion (p);
    // glowing magma at the bottom
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.0f), r.getCentreX(), r.getBottom() - r.getHeight() * 0.45f, Colour (0xffff7a3d).withAlpha (0.22f), r.getCentreX(), r.getBottom(), false));
    g.fillRect (r);
    g.restoreState();
    g.setColour (accent.withAlpha (0.55f)); g.strokePath (p, juce::PathStrokeType (1.6f));
}

// wavy top edge (lava surface) over a rounded body
inline juce::Path lavaPoolPath (Rectangle<float> r, float wave, int seed)
{
    juce::Random rnd (seed);
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getY() + wave);
    const int n = 9;
    for (int i = 1; i <= n; ++i)
    {
        const float x0 = r.getX() + r.getWidth() * (float) (i - 1) / n, x1 = r.getX() + r.getWidth() * (float) i / n;
        const float y = r.getY() + wave * (i % 2 ? rnd.nextFloat() * 0.6f : 1.0f + rnd.nextFloat() * 0.6f);
        p.quadraticTo ((x0 + x1) * 0.5f, y - wave * 0.3f, x1, r.getY() + wave);
    }
    p.lineTo (r.getRight(), r.getBottom() - 30.0f);
    p.quadraticTo (r.getRight(), r.getBottom(), r.getRight() - 30.0f, r.getBottom());
    p.lineTo (r.getX() + 30.0f, r.getBottom());
    p.quadraticTo (r.getX(), r.getBottom(), r.getX(), r.getBottom() - 30.0f);
    p.closeSubPath();
    return p;
}

//==============================================================================
// TWIST: torn edges (zigzag) around a rectangle
inline juce::Path tornPath (Rectangle<float> r, int seed, float tooth = 5.0f, bool tornLeftRight = true, bool tornTopBottom = false)
{
    juce::Random rnd (seed);
    juce::Path p;
    auto edge = [&] (Point<float> a, Point<float> b, bool torn, Point<float> nrm)
    {
        if (! torn) { p.lineTo (b); return; }
        const float len = a.getDistanceFrom (b);
        const int steps = juce::jmax (2, (int) (len / (tooth * 1.6f)));
        for (int i = 1; i <= steps; ++i)
        {
            const float u = (float) i / (float) steps;
            const auto q = a + (b - a) * u;
            p.lineTo (q + nrm * ((i % 2 ? 1.0f : 0.15f) * tooth * (0.5f + rnd.nextFloat())));
        }
    };
    p.startNewSubPath (r.getTopLeft());
    edge (r.getTopLeft(), r.getTopRight(), tornTopBottom, { 0, 1 });
    edge (r.getTopRight(), r.getBottomRight(), tornLeftRight, { -1, 0 });
    edge (r.getBottomRight(), r.getBottomLeft(), tornTopBottom, { 0, -1 });
    edge (r.getBottomLeft(), r.getTopLeft(), tornLeftRight, { 1, 0 });
    p.closeSubPath();
    return p;
}

// a five-branch star path
inline juce::Path starPath (Point<float> c, float R, float r, float rot = 0.0f)
{
    juce::Path p;
    for (int i = 0; i < 10; ++i)
    {
        const float a = -juce::MathConstants<float>::halfPi + rot + (float) i * juce::MathConstants<float>::pi / 5.0f;
        const auto q = c + Point<float> (std::cos (a), std::sin (a)) * ((i % 2) ? r : R);
        if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
    }
    p.closeSubPath();
    return p;
}
} // namespace tchk::shapes
