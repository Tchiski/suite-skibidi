#pragma once

// UI pieces for the Tchiskibidi instruments: pill chips, wheels, the MIDI grid,
// drag-out buttons and the big cosmic buttons (star, comet, black hole).

#include "TchiskibidiWidgets.h"
#include "TchiskibidiInstrument.h"
#include <set>

namespace tchk
{
inline double nowSec() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

//==============================================================================
class Chip : public juce::Button
{
public:
    Chip (const juce::String& label, juce::Colour accentIn) : juce::Button (label), accent (accentIn) {}
    juce::Colour accent;
    float fontSize = 13.0f;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        g.setColour (on ? accent : juce::Colours::white.withAlpha (over ? 0.14f : 0.08f));
        if (down) g.setColour (on ? accent.darker (0.2f) : juce::Colours::white.withAlpha (0.2f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        if (outline && ! on) { g.setColour (accent.withAlpha (0.6f)); g.drawRoundedRectangle (r.reduced (0.75f), r.getHeight() * 0.5f, 1.5f); }
        g.setColour (on ? juce::Colour (0xff14081f) : juce::Colour (0xfff4ecff).withAlpha (isEnabled() ? 1.0f : 0.4f));
        g.setFont (TchiskibidiLookAndFeel::font (fontSize));
        g.drawFittedText (getButtonText(), getLocalBounds().reduced (6, 0), juce::Justification::centred, 1, 0.8f);
    }
    bool outline = false;
};

//==============================================================================
// Round wheel of choices (keys, categories)
class Wheel : public juce::Component
{
public:
    Wheel (juce::StringArray itemsIn, juce::Colour accentIn) : items (std::move (itemsIn)), accent (accentIn) {}
    std::function<void (int)> onPick;
    void setSelected (int i) { selected = juce::jlimit (0, items.size() - 1, i); repaint(); }
    int getSelected() const { return selected; }
    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float R = juce::jmin (getWidth(), getHeight()) * 0.5f - 4.0f, ri = R * 0.42f;
        const int n = items.size();
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (c.x - R - 4, c.y - R - 4, (R + 4) * 2, (R + 4) * 2);
        for (int i = 0; i < n; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * ((float) i - 0.5f) / (float) n + 0.012f;
            const float a1 = juce::MathConstants<float>::twoPi * ((float) i + 0.5f) / (float) n - 0.012f;
            juce::Path seg; seg.addPieSegment (c.x - R, c.y - R, R * 2, R * 2, a0, a1, ri / R);
            g.setColour (i == selected ? accent : (i == hover ? juce::Colour (0xff2c1c48) : juce::Colour (0xff1c1428)));
            g.fillPath (seg);
            const auto p = c.getPointOnCircumference ((R + ri) * 0.5f, (a0 + a1) * 0.5f);
            g.setColour (i == selected ? juce::Colour (0xff14081f) : juce::Colour (0xfff4ecff));
            g.setFont (TchiskibidiLookAndFeel::font (n > 10 ? 11.0f : 12.5f));
            g.drawText (items[i].toUpperCase(), juce::Rectangle<float> (70, 18).withCentre (p), juce::Justification::centred);
        }
        g.setColour (juce::Colour (0xff0a0612)); g.fillEllipse (c.x - ri + 2, c.y - ri + 2, (ri - 2) * 2, (ri - 2) * 2);
        g.setColour (accent.withAlpha (0.6f)); g.drawEllipse (c.x - ri + 2, c.y - ri + 2, (ri - 2) * 2, (ri - 2) * 2, 1.5f);
        g.setColour (accent); g.setFont (TchiskibidiLookAndFeel::font (16.0f));
        g.drawText (centreText.isNotEmpty() ? centreText : items[selected].toUpperCase(), juce::Rectangle<float> (ri * 2, 24).withCentre (c), juce::Justification::centred);
    }
    int hitIndex (juce::Point<float> p) const
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float R = juce::jmin (getWidth(), getHeight()) * 0.5f - 4.0f, d = p.getDistanceFrom (c);
        if (d < R * 0.42f || d > R) return -1;
        float a = std::atan2 (p.x - c.x, -(p.y - c.y)); if (a < 0) a += juce::MathConstants<float>::twoPi;
        return ((int) std::floor (a / juce::MathConstants<float>::twoPi * (float) items.size() + 0.5f)) % items.size();
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = hitIndex (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { const int h = hitIndex (e.position); if (h >= 0) { setSelected (h); if (onPick) onPick (h); } }
    juce::String centreText;
private:
    juce::StringArray items;
    juce::Colour accent;
    int selected = 0, hover = -1;
};

//==============================================================================
// Editable MIDI grid. Click empty = add, drag = lasso, click a selected note again = delete,
// right edge = resize, Delete key, Cmd/Ctrl+A. Right click: glide (basses) or delete.
class PianoRoll : public juce::Component
{
public:
    std::vector<SeqNote> notes;
    int totalSteps = 64, stepsPerBar = 16, gridSnap = 1, newLength = 4;
    bool glideMode = false;
    juce::Colour accent { 0xffffd34d };
    std::function<void()> onEdit;
    std::function<void (int pitch)> onAudition;
    std::function<std::vector<std::tuple<int, int, juce::String>>()> getLabels; // start, length, text
    std::function<double()> getPlayStep;

    PianoRoll() { setWantsKeyboardFocus (true); getProperties().set ("keepFocus", true); }

    void fit()
    {
        if (notes.empty()) return;
        int mn = 127, mx = 0;
        for (auto& n : notes) { mn = juce::jmin (mn, n.pitch); mx = juce::jmax (mx, n.pitch); }
        lo = juce::jmin (mn - 2, mx - 15); hi = juce::jmax (mx + 2, lo + 15);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.42f)); g.fillRoundedRectangle (r, 16.0f);
        g.setColour (accent.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (0.75f), 16.0f, 1.5f);
        g.saveState(); g.reduceClipRegion (getLocalBounds().reduced (2));
        const float rh = rowH(), sw = stepW();
        for (int p = lo; p <= hi; ++p)
        {
            const float y = head + (float) (hi - p) * rh;
            const bool black = juce::Array<int> { 1, 3, 6, 8, 10 }.contains (p % 12);
            g.setColour (black ? juce::Colours::black.withAlpha (0.25f) : juce::Colours::white.withAlpha (0.03f));
            g.fillRect (gut, y, (float) getWidth() - gut, rh);
            if (p % 12 == 0 || p == lo)
            {
                g.setColour (LNF::textDim); g.setFont (LNF::font (9.0f, false));
                g.drawText (music::noteNames()[p % 12] + juce::String (p / 12 - 1), 4, (int) y - 2, (int) gut, (int) rh + 4, juce::Justification::centredLeft);
            }
        }
        for (int i = 0; i <= totalSteps; ++i)
        {
            const bool bar = i % stepsPerBar == 0, beat = i % 4 == 0;
            if (! bar && ! beat && sw < 9.0f) continue;
            g.setColour (juce::Colours::white.withAlpha (bar ? 0.25f : (beat ? 0.08f : 0.03f)));
            g.drawVerticalLine ((int) (gut + (float) i * sw), head, (float) getHeight());
        }
        if (getLabels)
            for (auto& [s, l, t] : getLabels())
            {
                g.setColour (accent); g.setFont (LNF::font ((float) l * sw < 60.0f ? 10.0f : 14.0f));
                g.drawText (t, (int) (gut + (float) s * sw + 4), 4, (int) juce::jmax (30.0f, (float) l * sw), (int) head - 6, juce::Justification::centredLeft);
            }
        for (size_t i = 0; i < notes.size(); ++i)
        {
            auto& n = notes[i];
            if (n.pitch < lo || n.pitch > hi) continue;
            auto nr = noteRect (n);
            if (glideMode && n.glide && i > 0)
            {
                auto pr = noteRect (notes[i - 1]);
                juce::Path p; p.startNewSubPath (pr.getRight() - 2, pr.getCentreY());
                p.cubicTo (pr.getRight() + 8, pr.getCentreY(), nr.getX() - 8, nr.getCentreY(), nr.getX() + 3, nr.getCentreY());
                float dashes[] = { 3.0f, 3.0f }; juce::Path d; juce::PathStrokeType (2.0f).createDashedStroke (d, p, dashes, 2);
                g.setColour (juce::Colours::white.withAlpha (0.8f)); g.fillPath (d);
            }
            const bool s = sel.count ((int) i) > 0;
            g.setColour (accent.withAlpha (0.25f)); g.fillRoundedRectangle (nr.expanded (1.5f), 4.0f);
            g.setColour (s ? juce::Colours::white : accent); g.fillRoundedRectangle (nr, 3.0f);
            if (glideMode && n.glide) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (nr, 3.0f, 1.2f); }
        }
        if (getPlayStep)
        {
            const double ps = getPlayStep();
            if (ps >= 0) { g.setColour (accent.withAlpha (0.16f)); g.fillRect (gut + (float) std::floor (ps) * sw, head, sw, (float) getHeight() - head); }
        }
        if (lasso) { g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRect (lassoRect); g.setColour (juce::Colours::white.withAlpha (0.6f)); g.drawRect (lassoRect, 1.0f); }
        g.restoreState();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.y < head || e.x < gut) return;
        const int hit = hitNote (e.position);
        if (e.mods.isPopupMenu())
        {
            if (hit < 0) return;
            if (glideMode) notes[(size_t) hit].glide = ! notes[(size_t) hit].glide;
            else { notes.erase (notes.begin() + hit); sel.clear(); }
            changed(); return;
        }
        drag = {}; drag.active = true; drag.startPos = e.position; drag.startStep = stepAt (e.x); drag.startPitch = pitchAt (e.y);
        if (hit >= 0)
        {
            drag.wasSelected = sel.count (hit) > 0;
            if (! drag.wasSelected) { if (! e.mods.isShiftDown()) sel.clear(); sel.insert (hit); }
            drag.hit = hit;
            drag.resize = e.x > noteRect (notes[(size_t) hit]).getRight() - 7.0f;
            for (int i : sel) drag.orig.push_back ({ i, notes[(size_t) i] });
            if (onAudition) onAudition (notes[(size_t) hit].pitch);
        }
        else
        {
            if (! e.mods.isShiftDown()) sel.clear();
            lasso = true; lassoRect = {}; drag.baseSel = sel;
        }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! drag.active) return;
        if (e.position.getDistanceFrom (drag.startPos) > 4.0f) drag.moved = true;
        if (lasso)
        {
            lassoRect = juce::Rectangle<float> (drag.startPos, e.position);
            sel = drag.baseSel;
            for (size_t i = 0; i < notes.size(); ++i) if (noteRect (notes[i]).intersects (lassoRect)) sel.insert ((int) i);
            repaint(); return;
        }
        const int ds = stepAt (e.x) - drag.startStep, dp = pitchAt (e.y) - drag.startPitch;
        for (auto& [i, o] : drag.orig)
        {
            auto& n = notes[(size_t) i];
            if (drag.resize) n.length = juce::jlimit (1, totalSteps - n.start, o.length + ds);
            else
            {
                n.start = juce::jlimit (0, totalSteps - n.length, o.start + ds);
                const int np = juce::jlimit (lo, hi, o.pitch + dp);
                if (np != n.pitch && i == drag.hit && onAudition) onAudition (np);
                n.pitch = np;
            }
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! drag.active) return;
        if (lasso)
        {
            lasso = false;
            if (drag.moved) { drag.active = false; if (sel.empty()) giveAwayKeyboardFocus(); repaint(); return; }
            if (e.y >= head && e.x >= gut)   // simple click in the void: add a note
            {
                SeqNote n; n.start = juce::jlimit (0, totalSteps - 1, (stepAt (e.x) / gridSnap) * gridSnap); n.pitch = pitchAt (e.y);
                n.length = juce::jmin (newLength, totalSteps - n.start);
                notes.push_back (n); sel.clear(); sel.insert ((int) notes.size() - 1);
                if (onAudition) onAudition (n.pitch);
            }
        }
        else if (! drag.moved && drag.wasSelected)   // clicked again on a selected note: delete (the whole selection)
        {
            std::vector<SeqNote> keep;
            for (size_t i = 0; i < notes.size(); ++i) if (sel.count ((int) i) == 0) keep.push_back (notes[i]);
            notes = keep; sel.clear();
        }
        drag.active = false;
        changed();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && ! sel.empty())
        {
            std::vector<SeqNote> keep;
            for (size_t i = 0; i < notes.size(); ++i) if (sel.count ((int) i) == 0) keep.push_back (notes[i]);
            notes = keep; sel.clear(); changed(); return true;
        }
        if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'A' || k.getKeyCode() == 'a'))
        {
            sel.clear(); for (size_t i = 0; i < notes.size(); ++i) sel.insert ((int) i); repaint(); return true;
        }
        return false;
    }

private:
    struct Drag { bool active = false, moved = false, wasSelected = false, resize = false; int hit = -1, startStep = 0, startPitch = 0;
                  juce::Point<float> startPos; std::vector<std::pair<int, SeqNote>> orig; std::set<int> baseSel; };
    Drag drag;
    std::set<int> sel;
    bool lasso = false;
    juce::Rectangle<float> lassoRect;
    int lo = 45, hi = 64;
    const float head = 26.0f, gut = 34.0f;

    float rowH() const { return ((float) getHeight() - head) / (float) (hi - lo + 1); }
    float stepW() const { return ((float) getWidth() - gut) / (float) juce::jmax (1, totalSteps); }
    int stepAt (int x) const { return juce::jlimit (0, totalSteps - 1, (int) std::floor (((float) x - gut) / stepW())); }
    int pitchAt (int y) const { return juce::jlimit (lo, hi, hi - (int) std::floor (((float) y - head) / rowH())); }
    juce::Rectangle<float> noteRect (const SeqNote& n) const
    {
        return { gut + (float) n.start * stepW() + 1.0f, head + (float) (hi - n.pitch) * rowH() + 0.5f, juce::jmax (4.0f, (float) n.length * stepW() - 2.0f), juce::jmax (3.0f, rowH() - 1.0f) };
    }
    int hitNote (juce::Point<float> p) const
    {
        for (int i = (int) notes.size(); --i >= 0;) if (noteRect (notes[(size_t) i]).expanded (0, 1).contains (p)) return i;
        return -1;
    }
    void changed()
    {
        // sort the notes but keep the selection on the same notes
        std::vector<std::pair<SeqNote, bool>> tmp;
        for (size_t i = 0; i < notes.size(); ++i) tmp.push_back ({ notes[i], sel.count ((int) i) > 0 });
        std::stable_sort (tmp.begin(), tmp.end(), [] (const auto& a, const auto& b) { return a.first.start < b.first.start || (a.first.start == b.first.start && a.first.pitch < b.first.pitch); });
        notes.clear(); sel.clear();
        for (size_t i = 0; i < tmp.size(); ++i) { notes.push_back (tmp[i].first); if (tmp[i].second) sel.insert ((int) i); }
        if (sel.empty()) giveAwayKeyboardFocus();   // hand the keyboard back to the host (computer keyboard = notes)
        repaint();
        if (onEdit) onEdit();
    }
};

//==============================================================================
// "⠿ Glisser le MIDI": drag it onto a track; the file is made when the drag starts
class DragOut : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragOut (const juce::String& t, std::function<juce::File()> make) : text (t), makeFile (std::move (make))
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip (TK_FR ("Maintiens le clic et glisse sur une piste d'Ableton"));
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        juce::Path p; p.addRoundedRectangle (r, r.getHeight() * 0.5f);
        float dashes[] = { 5.0f, 4.0f }; juce::Path d; juce::PathStrokeType (1.5f).createDashedStroke (d, p, dashes, 2);
        g.setColour (juce::Colours::white.withAlpha (isMouseOver() ? 0.8f : 0.45f)); g.fillPath (d);
        g.setColour (LNF::text); g.setFont (LNF::font (13.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\xa0\xbf ")) + text, getLocalBounds(), juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        auto f = makeFile ? makeFile() : juce::File();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
        else dragging = false;
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    juce::String text;
    std::function<juce::File()> makeFile;
    bool dragging = false;
};

//==============================================================================
// Small badge that shows the instrument follows the host transport
class TransportBadge : public juce::Component
{
public:
    std::function<bool()> isPlaying;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r, 12.0f);
        const bool on = isPlaying && isPlaying();
        const auto dot = juce::Rectangle<float> (10, 10).withCentre ({ r.getX() + 16, r.getCentreY() });
        g.setColour (on ? juce::Colour (0xff5dff8a) : juce::Colours::white.withAlpha (0.3f)); g.fillEllipse (dot);
        g.setColour (LNF::textDim); g.setFont (LNF::font (10.5f));
        g.drawFittedText (on ? TK_FR ("JOUE AVEC ABLETON") : TK_FR ("LANCE ABLETON (ESPACE)\nPOUR ENTENDRE"), r.withTrimmedLeft (28).toNearestInt(), juce::Justification::centredLeft, 2);
    }
};

//==============================================================================
// The big faceted 3D star (Accords): click = new progression
class Star3D : public juce::Component
{
public:
    juce::Colour accent { 0xffffd34d };
    std::function<void()> onClick;
    void pop() { popT = nowSec(); }
    void paint (juce::Graphics& g) override
    {
        const double t = nowSec();
        const auto c = getLocalBounds().toFloat().getCentre();
        const float size = juce::jmin (getWidth(), getHeight()) * 1.0f;
        const float popK = (float) juce::jmax (0.0, 1.0 - (t - popT) / 0.45);
        const float scale = 1.0f + 0.12f * popK * std::sin ((float) (t - popT) * 18.0f);
        const float R = size * 0.40f * scale, r = size * 0.19f * scale, H = size * 0.16f * scale;
        // rays + glow
        for (int i = 0; i < 12; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::pi / 6.0f + (float) t * 0.12f;
            juce::Path ray; ray.addTriangle (c, c.getPointOnCircumference (size * 0.62f, a - 0.05f), c.getPointOnCircumference (size * 0.62f, a + 0.05f));
            g.setColour (accent.withAlpha (0.08f)); g.fillPath (ray);
        }
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.45f), c.x, c.y, accent.withAlpha (0.0f), c.x + size * 0.5f, c.y, true));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (c));
        // rotate the star around its vertical axis, tilted a little
        const float yaw = (float) std::sin (t * 0.7) * 0.55f + popK * 6.28f, pitchA = -0.25f;
        struct P3 { float x, y, z; };
        auto proj = [&] (P3 p)
        {
            const float x1 = p.x * std::cos (yaw) + p.z * std::sin (yaw), z1 = -p.x * std::sin (yaw) + p.z * std::cos (yaw);
            const float y2 = p.y * std::cos (pitchA) - z1 * std::sin (pitchA), z2 = p.y * std::sin (pitchA) + z1 * std::cos (pitchA);
            const float f = size * 3.2f / (size * 3.2f + z2);
            return std::make_pair (juce::Point<float> (c.x + x1 * f, c.y + y2 * f), z2);
        };
        std::vector<P3> rim;
        for (int i = 0; i < 10; ++i) { const float a = -juce::MathConstants<float>::halfPi + (float) i * juce::MathConstants<float>::pi / 5.0f; const float rr = (i % 2) ? r : R; rim.push_back ({ std::cos (a) * rr, std::sin (a) * rr, 0 }); }
        struct Face { juce::Path p; float z; float shade; };
        std::vector<Face> faces;
        for (int side = 0; side < 2; ++side)
        {
            const P3 apex { 0, 0, side ? H : -H };
            for (int i = 0; i < 10; ++i)
            {
                auto [a, za] = proj (apex); auto [b, zb] = proj (rim[(size_t) i]); auto [d, zd] = proj (rim[(size_t) ((i + 1) % 10)]);
                juce::Path p; p.addTriangle (a, b, d);
                const float cross = (b.x - a.x) * (d.y - a.y) - (b.y - a.y) * (d.x - a.x);
                const float shade = 0.55f + 0.45f * std::sin ((float) i * 0.9f + yaw * 1.3f + (side ? 0.0f : 2.0f));
                if ((side == 0) == (cross > 0)) faces.push_back ({ p, (za + zb + zd) / 3.0f, shade });
            }
        }
        std::sort (faces.begin(), faces.end(), [] (const Face& x, const Face& y) { return x.z > y.z; });
        for (auto& f : faces)
        {
            g.setColour (accent.interpolatedWith (juce::Colour (0xff8a5a00), 1.0f - f.shade).brighter (f.shade * 0.35f));
            g.fillPath (f.p);
            g.setColour (juce::Colour (0xfffff3c4).withAlpha (0.25f)); g.strokePath (f.p, juce::PathStrokeType (0.6f));
        }
        // sparkles
        for (int i = 0; i < 6; ++i)
        {
            const float a = (float) i * 1.05f + (float) t * 0.3f, d = size * (0.36f + 0.08f * std::sin ((float) t * 1.7f + (float) i));
            const auto p = c.getPointOnCircumference (d, a);
            const float s = 3.0f + 2.0f * std::sin ((float) t * 3.0f + (float) i * 2.0f);
            g.setColour (juce::Colour (0xfffff7d0).withAlpha (0.8f));
            juce::Path sp; sp.startNewSubPath (p.x, p.y - s); sp.lineTo (p.x + s * 0.3f, p.y - s * 0.3f); sp.lineTo (p.x + s, p.y); sp.lineTo (p.x + s * 0.3f, p.y + s * 0.3f);
            sp.lineTo (p.x, p.y + s); sp.lineTo (p.x - s * 0.3f, p.y + s * 0.3f); sp.lineTo (p.x - s, p.y); sp.lineTo (p.x - s * 0.3f, p.y - s * 0.3f); sp.closeSubPath();
            g.fillPath (sp);
        }
    }
    void mouseDown (const juce::MouseEvent&) override { pop(); if (onClick) onClick(); }
private:
    double popT = -10.0;
};

//==============================================================================
// The sleepy comet (Synthé): click = random settings. It wakes up, dashes off, comes back, falls asleep.
class Comet : public juce::Component
{
public:
    juce::Colour accent { 0xffff8a3d }, accent2 { 0xffffe2cc };
    std::function<void()> onClick;
    void paint (juce::Graphics& g) override
    {
        const double t = nowSec(), ft = t - flyT;
        float dx = 0, dy = 0, sc = 1, alpha = 1;
        const bool awake = ft < 1.75;
        if (ft < 1.1)
        {
            const float u = (float) (ft / 1.1);
            if (u < 0.12f) { dx = -6 * u / 0.12f; dy = 4 * u / 0.12f; }
            else if (u < 0.42f) { const float k = (u - 0.12f) / 0.3f; dx = juce::jmap (k, -6.0f, 260.0f); dy = juce::jmap (k, 4.0f, -150.0f); sc = juce::jmap (k, 1.0f, 0.35f); alpha = 1.0f - k; }
            else if (u < 0.8f) { const float k = (u - 0.42f) / 0.38f; dx = juce::jmap (k, -260.0f, 10.0f); dy = juce::jmap (k, 60.0f, -4.0f); sc = juce::jmap (k, 0.35f, 1.08f); alpha = k; }
            else { const float k = (u - 0.8f) / 0.2f; dx = 10 * (1 - k); dy = -4 * (1 - k); sc = 1.08f - 0.08f * k; }
        }
        else { dy = -3.0f * std::sin ((float) t * 2.4f); }
        g.addTransform (juce::AffineTransform::scale (sc, sc, getWidth() * 0.66f, getHeight() * 0.36f).translated (dx, dy));
        g.beginTransparencyLayer (juce::jlimit (0.0f, 1.0f, alpha));
        const float W = (float) getWidth(), Hh = (float) getHeight();
        const auto head = juce::Point<float> (W * 0.66f, Hh * 0.36f);
        const float hr = W * 0.2f;
        // tail
        for (int k = 0; k < 3; ++k)
        {
            juce::Path tail;
            const float off = (float) k * 0.14f - 0.14f, wob = 0.04f * std::sin ((float) t * 4.0f + (float) k);
            tail.startNewSubPath (head.x - hr * 0.3f, head.y - hr * (0.8f - (float) k * 0.6f));
            tail.quadraticTo (W * (0.35f + off), Hh * (0.45f + wob), W * (0.02f + off * 0.5f), Hh * (0.95f - (float) k * 0.18f));
            tail.quadraticTo (W * (0.4f + off), Hh * (0.6f + wob), head.x + hr * 0.2f, head.y + hr * (0.6f - (float) k * 0.3f));
            tail.closeSubPath();
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.9f - (float) k * 0.2f), head.x, head.y, juce::Colour (0xff6b7bff).withAlpha (0.0f), 0, Hh, false));
            g.fillPath (tail);
        }
        g.setColour (accent.withAlpha (0.25f)); g.fillEllipse (juce::Rectangle<float> (hr * 2.5f, hr * 2.5f).withCentre (head));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffffbe8), head.x - hr * 0.3f, head.y - hr * 0.4f, accent, head.x + hr, head.y + hr, true));
        g.fillEllipse (juce::Rectangle<float> (hr * 2, hr * 2).withCentre (head));
        // face
        const auto dark = juce::Colour (0xff3a1530);
        g.setColour (dark);
        if (awake)
        {
            g.fillEllipse (head.x - hr * 0.48f, head.y - hr * 0.3f, hr * 0.42f, hr * 0.42f);
            g.fillEllipse (head.x + hr * 0.08f, head.y - hr * 0.3f, hr * 0.42f, hr * 0.42f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (head.x - hr * 0.36f, head.y - hr * 0.26f, hr * 0.14f, hr * 0.14f);
            g.fillEllipse (head.x + hr * 0.2f, head.y - hr * 0.26f, hr * 0.14f, hr * 0.14f);
        }
        else
        {
            for (float ex : { -0.28f, 0.28f })
            {
                juce::Path lid; lid.startNewSubPath (head.x + hr * (ex - 0.18f), head.y - hr * 0.1f);
                lid.quadraticTo (head.x + hr * ex, head.y + hr * 0.12f, head.x + hr * (ex + 0.18f), head.y - hr * 0.1f);
                g.strokePath (lid, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            const float zz = (float) std::fmod (t, 2.4) / 2.4f;
            g.setColour (juce::Colour (0xfffff7d0).withAlpha (juce::jlimit (0.0f, 1.0f, 1.0f - zz)));
            g.setFont (LNF::font (11.0f));
            g.drawText ("z", (int) (head.x + hr * 0.9f + zz * 6), (int) (head.y - hr * 1.3f - zz * 10), 14, 14, juce::Justification::centred);
        }
        g.setColour (juce::Colour (0xffff7aa8));
        juce::Path mouth; mouth.startNewSubPath (head.x - hr * 0.2f, head.y + hr * 0.3f); mouth.quadraticTo (head.x, head.y + hr * 0.55f, head.x + hr * 0.22f, head.y + hr * 0.3f);
        g.strokePath (mouth, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffff7aa8).withAlpha (0.5f));
        g.fillEllipse (head.x - hr * 0.75f, head.y + hr * 0.12f, hr * 0.3f, hr * 0.3f);
        g.fillEllipse (head.x + hr * 0.48f, head.y + hr * 0.12f, hr * 0.3f, hr * 0.3f);
        g.endTransparencyLayer();
    }
    void mouseDown (const juce::MouseEvent&) override { if (nowSec() - flyT > 1.75) flyT = nowSec(); if (onClick) onClick(); }
private:
    double flyT = -10.0;
};

//==============================================================================
// The black hole with eyes (Basse): click = new bass line, it throbs on every note
class BlackHole : public juce::Component
{
public:
    juce::Colour accent { 0xffff4d7d }, accent2 { 0xffffd0de };
    std::function<void()> onClick;
    void pulse (float k = 1.0f) { pulseT = nowSec(); pulseK = k; }
    void wake() { wakeT = nowSec(); }
    void paint (juce::Graphics& g) override
    {
        const double t = nowSec();
        const auto c = getLocalBounds().toFloat().getCentre();
        const float s = juce::jmin (getWidth(), getHeight()) * 0.5f;
        const float pk = (float) juce::jmax (0.0, 1.0 - (t - pulseT) / 0.22) * pulseK;
        const float wk = (float) juce::jmax (0.0, 1.0 - (t - wakeT) / 0.5);
        const float sc = 1.0f + 0.07f * pk + 0.12f * wk * std::sin ((float) (t - wakeT) * 14.0f);
        g.addTransform (juce::AffineTransform::scale (sc, sc, c.x, c.y));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black, c.x, c.y, accent.withAlpha (0.0f), c.x + s, c.y, true));
        juce::ColourGradient glow (juce::Colours::black, c.x, c.y, accent.withAlpha (0.0f), c.x + s * 0.95f, c.y, true);
        glow.addColour (0.48, juce::Colours::black); glow.addColour (0.66, accent.withAlpha (0.9f)); glow.addColour (0.85, accent2.withAlpha (0.35f));
        g.setGradientFill (glow); g.fillEllipse (juce::Rectangle<float> (s * 1.9f, s * 1.9f).withCentre (c));
        const float tilt = 0.07f * std::sin ((float) t * 0.6f);
        auto ring = [&] (bool back)
        {
            juce::Path p; p.addCentredArc (c.x, c.y, s * 0.98f, s * 0.22f, tilt, back ? -juce::MathConstants<float>::halfPi : juce::MathConstants<float>::halfPi,
                                           back ? juce::MathConstants<float>::halfPi : juce::MathConstants<float>::halfPi * 3.0f, true);
            g.setColour (accent2.withAlpha (back ? 0.5f : 0.9f)); g.strokePath (p, juce::PathStrokeType (s * 0.075f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        ring (true);
        g.setColour (juce::Colour (0xff06010a)); g.fillEllipse (juce::Rectangle<float> (s * 0.78f, s * 0.78f).withCentre (c));
        g.setColour (accent2.withAlpha (0.6f)); g.drawEllipse (juce::Rectangle<float> (s * 0.78f, s * 0.78f).withCentre (c), 2.0f);
        // eyes look around
        const float look = 2.0f * std::sin ((float) t * 0.9f);
        const bool blink = std::fmod (t, 4.3) < 0.12;
        for (float ex : { -0.13f, 0.13f })
        {
            const auto e = juce::Point<float> (c.x + s * ex, c.y - s * 0.04f);
            g.setColour (juce::Colours::white);
            if (blink) g.fillRect (e.x - 5, e.y, 10.0f, 2.0f);
            else { g.fillEllipse (e.x - 6, e.y - 7, 12, 14); g.setColour (juce::Colour (0xff06010a)); g.fillEllipse (e.x - 3 + look, e.y - 2, 6, 6); }
        }
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        juce::Path m; m.startNewSubPath (c.x - s * 0.08f, c.y + s * 0.12f); m.quadraticTo (c.x, c.y + s * 0.19f, c.x + s * 0.08f, c.y + s * 0.12f);
        g.strokePath (m, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        ring (false);
    }
    void mouseDown (const juce::MouseEvent&) override { wake(); if (onClick) onClick(); }
private:
    double pulseT = -10, wakeT = -10;
    float pulseK = 1.0f;
};
} // namespace tchk
