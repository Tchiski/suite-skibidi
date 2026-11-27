#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Look & feel shared by every plugin of the Tchiskibidi suite,
// so they all share the same visual identity.
class TchiskibidiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // --- Palette ----------------------------------------------------------
    static inline const juce::Colour background { 0xff120a1f };
    static inline const juce::Colour backgroundLight { 0xff22113a };
    static inline const juce::Colour panel { 0xff1c1030 };
    static inline const juce::Colour panelLight { 0xff2c1c48 };
    static inline const juce::Colour text { 0xfff4ecff };
    static inline const juce::Colour textDim { 0xffa898c4 };
    static inline const juce::Colour pink { 0xffff4fd8 };
    static inline const juce::Colour cyan { 0xff3df5ff };
    static inline const juce::Colour lime { 0xffc6ff3d };

    static juce::Font font (float height, bool bold = true)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    TchiskibidiLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, pink.withAlpha (0.4f));
        setColour (juce::Label::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, panelLight);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, text);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, pink.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, pink);
        setColour (juce::TextButton::buttonColourId, panelLight);
        setColour (juce::TextButton::buttonOnColourId, pink);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, background);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = startAngle + sliderPos * (endAngle - startAngle);
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const bool enabled = slider.isEnabled();
        if (slider.getProperties()["tk"].toString() == "porthole")
        {
            drawPortholeKnob (g, bounds, sliderPos, startAngle, endAngle, accent, enabled);
            return;
        }
        if (slider.getProperties()["tk"].toString() == "lava")
        {
            drawLavaKnob (g, bounds, sliderPos, startAngle, endAngle, accent, enabled);
            return;
        }
        if (slider.getProperties()["tk"].toString() == "orbit")
        {
            drawOrbit (g, bounds, sliderPos, startAngle, endAngle, accent, enabled);
            return;
        }

        const float lineWidth = juce::jmax (3.0f, radius * 0.09f);
        const float arcRadius = radius - lineWidth * 0.5f;
        const juce::PathStrokeType stroke (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        // Track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (panelLight);
        g.strokePath (track, stroke);

        // Value arc (with a soft glow)
        if (enabled)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
            g.setColour (accent.withAlpha (0.25f));
            g.strokePath (value, juce::PathStrokeType (lineWidth * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (value, stroke);
        }

        // Knob body
        const float knobRadius = arcRadius - lineWidth * 1.6f;
        if (knobRadius <= 2.0f)
            return;

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a2860), centre.x, centre.y - knobRadius,
                                                 juce::Colour (0xff150b26), centre.x, centre.y + knobRadius, false));
        g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        // Pointer
        const auto p1 = centre.getPointOnCircumference (knobRadius * 0.30f, angle);
        const auto p2 = centre.getPointOnCircumference (knobRadius * 0.85f, angle);
        g.setColour (enabled ? text : textDim.withAlpha (0.5f));
        g.drawLine ({ p1, p2 }, juce::jmax (2.0f, lineWidth * 0.7f));
    }

    // planet + sun: the sun travels on the dashed orbit, the arc behind it shows the value
    // SPACE: a rocket porthole (riveted metal ring, glass, glowing needle)
    static void drawPortholeKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float pos, float a0, float a1, juce::Colour accent, bool enabled)
    {
        const auto c = bounds.getCentre();
        const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
        const float ang = a0 + pos * (a1 - a0);
        // metal ring
        juce::ColourGradient metal (juce::Colour (0xffb8d8f0), c.x - R, c.y - R, juce::Colour (0xff1d3550), c.x + R, c.y + R, false);
        metal.addColour (0.5, juce::Colour (0xff4d7fa6));
        g.setGradientFill (metal); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        const float ri = R * 0.78f;
        // rivets
        for (int i = 0; i < 8; ++i)
        {
            const auto p = c.getPointOnCircumference (R * 0.89f, juce::MathConstants<float>::twoPi * (float) i / 8.0f + 0.39f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeaf6ff), p.x - 1.5f, p.y - 1.5f, juce::Colour (0xff3a5a78), p.x + 2, p.y + 2, false));
            g.fillEllipse (p.x - R * 0.06f, p.y - R * 0.06f, R * 0.12f, R * 0.12f);
        }
        // glass
        juce::ColourGradient glass (juce::Colour (0xff1c3a5e), c.x - ri * 0.3f, c.y - ri * 0.4f, juce::Colour (0xff040a16), c.x + ri, c.y + ri, true);
        g.setGradientFill (glass); g.fillEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2, 1.5f);
        // value arc on the inner edge
        juce::Path track; track.addCentredArc (c.x, c.y, ri * 0.84f, ri * 0.84f, 0.0f, a0, a1, true);
        g.setColour (accent.withAlpha (0.15f)); g.strokePath (track, juce::PathStrokeType (ri * 0.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (enabled)
        {
            juce::Path v; v.addCentredArc (c.x, c.y, ri * 0.84f, ri * 0.84f, 0.0f, a0, ang, true);
            g.setColour (accent.withAlpha (0.3f)); g.strokePath (v, juce::PathStrokeType (ri * 0.26f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent); g.strokePath (v, juce::PathStrokeType (ri * 0.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        // needle
        const auto tip = c.getPointOnCircumference (ri * 0.66f, ang);
        g.setColour (accent.withAlpha (enabled ? 0.35f : 0.1f)); g.drawLine (c.x, c.y, tip.x, tip.y, ri * 0.16f);
        g.setColour (enabled ? accent.brighter (0.4f) : juce::Colours::grey); g.drawLine (c.x, c.y, tip.x, tip.y, ri * 0.06f);
        g.setColour (juce::Colour (0xff9fc8e6)); g.fillEllipse (c.x - ri * 0.1f, c.y - ri * 0.1f, ri * 0.2f, ri * 0.2f);
        // reflection on the glass
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        juce::Path refl; refl.addCentredArc (c.x, c.y, ri * 0.7f, ri * 0.7f, 0.0f, -2.2f, -0.9f, true);
        g.strokePath (refl, juce::PathStrokeType (ri * 0.12f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // FUSION: a drop of magma; the value is a glowing ring of lava
    static void drawLavaKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float pos, float a0, float a1, juce::Colour accent, bool enabled)
    {
        const auto c = bounds.getCentre();
        const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float ang = a0 + pos * (a1 - a0);
        juce::Path track; track.addCentredArc (c.x, c.y, R * 0.86f, R * 0.86f, 0.0f, a0, a1, true);
        g.setColour (juce::Colour (0xff2a0a24)); g.strokePath (track, juce::PathStrokeType (R * 0.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (enabled && pos > 0.001f)
        {
            juce::Path v; v.addCentredArc (c.x, c.y, R * 0.86f, R * 0.86f, 0.0f, a0, ang, true);
            g.setColour (accent.withAlpha (0.3f)); g.strokePath (v, juce::PathStrokeType (R * 0.38f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::ColourGradient lg (juce::Colour (0xffffd27a), c.x, c.y + R, accent, c.x, c.y - R, false);
            g.setGradientFill (lg); g.strokePath (v, juce::PathStrokeType (R * 0.17f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        // the drop (a soft blob, a bit heavier at the bottom)
        const float r = R * 0.58f;
        juce::Path drop;
        drop.startNewSubPath (c.x, c.y - r * 1.05f);
        drop.cubicTo (c.x + r * 0.7f, c.y - r * 0.8f, c.x + r * 1.05f, c.y + r * 0.2f, c.x + r * 0.75f, c.y + r * 0.72f);
        drop.cubicTo (c.x + r * 0.45f, c.y + r * 1.08f, c.x - r * 0.45f, c.y + r * 1.08f, c.x - r * 0.75f, c.y + r * 0.72f);
        drop.cubicTo (c.x - r * 1.05f, c.y + r * 0.2f, c.x - r * 0.7f, c.y - r * 0.8f, c.x, c.y - r * 1.05f);
        drop.closeSubPath();
        juce::ColourGradient body (juce::Colour (0xffffc8d8), c.x - r * 0.4f, c.y - r * 0.5f, juce::Colour (0xff6a0a2a), c.x + r * 0.6f, c.y + r, true);
        body.addColour (0.45, accent);
        g.setGradientFill (body); g.fillPath (drop);
        g.setColour (juce::Colours::white.withAlpha (0.55f)); g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.55f, r * 0.32f, r * 0.22f);
        // pointer: a bright bubble on the ring
        const auto p = c.getPointOnCircumference (R * 0.86f, ang);
        g.setColour (juce::Colour (0xfffff0c0)); g.fillEllipse (p.x - R * 0.12f, p.y - R * 0.12f, R * 0.24f, R * 0.24f);
    }

    static void drawOrbit (juce::Graphics& g, juce::Rectangle<float> bounds, float pos, float a0, float a1, juce::Colour accent, bool enabled)
    {
        const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 5.0f;
        if (R < 6.0f) return;
        const auto c = bounds.getCentre();
        const auto angle = a0 + pos * (a1 - a0);
        juce::Path orbit; orbit.addCentredArc (c.x, c.y, R, R, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        float dashes[] = { 2.0f, 4.0f };
        juce::Path dashed; juce::PathStrokeType (1.0f).createDashedStroke (dashed, orbit, dashes, 2);
        g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillPath (dashed);
        juce::Path trail; trail.addCentredArc (c.x, c.y, R, R, 0.0f, a0, angle, true);
        g.setColour (juce::Colour (0xffffd34d).withAlpha (enabled ? 0.85f : 0.3f));
        g.strokePath (trail, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // planet with its ring
        const float pr = R * 0.42f;
        g.setGradientFill (juce::ColourGradient (accent.brighter (0.8f), c.x - pr * 0.4f, c.y - pr * 0.5f, juce::Colour (0xff120a1f), c.x + pr, c.y + pr, true));
        g.fillEllipse (c.x - pr, c.y - pr, pr * 2.0f, pr * 2.0f);
        g.setColour (accent.withAlpha (0.75f));
        juce::Path ring; ring.addEllipse (juce::Rectangle<float> (pr * 3.1f, pr * 0.76f).withCentre (c));
        g.strokePath (ring, juce::PathStrokeType (1.4f), juce::AffineTransform::rotation (-0.31f, c.x, c.y));
        // the sun
        const auto s = c.getPointOnCircumference (R, angle);
        g.setColour (juce::Colour (0xffffd34d).withAlpha (0.25f)); g.fillEllipse (s.x - 10.0f, s.y - 10.0f, 20.0f, 20.0f);
        g.setColour (juce::Colour (0xffffe680)); g.fillEllipse (s.x - 6.0f, s.y - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colour (0xffffb020)); g.drawEllipse (s.x - 6.0f, s.y - 6.0f, 12.0f, 12.0f, 1.2f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (slider.getProperties()["tk"].toString() != "rocket")
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
            return;
        }
        // rocket fader: dashed launch rail, glowing flight trail, rocket at the value
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const float cx = (float) x + (float) width * 0.5f;
        float dashes[] = { 2.0f, 5.0f };
        juce::Path rail; rail.startNewSubPath (cx, (float) y + 4.0f); rail.lineTo (cx, (float) (y + height) - 2.0f);
        juce::Path d; juce::PathStrokeType (1.2f).createDashedStroke (d, rail, dashes, 2);
        g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillPath (d);
        const float ry = juce::jlimit ((float) y + 14.0f, (float) (y + height) - 10.0f, sliderPos);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.0f), cx, (float) (y + height), accent.withAlpha (0.6f), cx, ry, false));
        g.fillRoundedRectangle (cx - 2.5f, ry, 5.0f, (float) (y + height) - ry, 2.5f);
        const float w = juce::jlimit (10.0f, 18.0f, (float) width * 0.35f), h = w * 2.0f;
        // flame
        juce::Path flame; flame.startNewSubPath (cx - w * 0.3f, ry + h * 0.38f); flame.quadraticTo (cx, ry + h * 0.38f + w * 1.2f, cx + w * 0.3f, ry + h * 0.38f); flame.closeSubPath();
        g.setColour (juce::Colour (0xffffa23d)); g.fillPath (flame);
        // body
        juce::Path body;
        body.startNewSubPath (cx, ry - h * 0.62f);
        body.cubicTo (cx + w * 0.55f, ry - h * 0.35f, cx + w * 0.5f, ry + h * 0.2f, cx + w * 0.38f, ry + h * 0.4f);
        body.lineTo (cx - w * 0.38f, ry + h * 0.4f);
        body.cubicTo (cx - w * 0.5f, ry + h * 0.2f, cx - w * 0.55f, ry - h * 0.35f, cx, ry - h * 0.62f);
        body.closeSubPath();
        g.setColour (juce::Colour (0xfff4ecff)); g.fillPath (body);
        // fins + window
        g.setColour (accent);
        juce::Path fins;
        fins.addTriangle (cx - w * 0.42f, ry + h * 0.05f, cx - w * 0.85f, ry + h * 0.45f, cx - w * 0.38f, ry + h * 0.38f);
        fins.addTriangle (cx + w * 0.42f, ry + h * 0.05f, cx + w * 0.85f, ry + h * 0.45f, cx + w * 0.38f, ry + h * 0.38f);
        g.fillPath (fins);
        g.fillEllipse (cx - w * 0.2f, ry - h * 0.22f, w * 0.4f, w * 0.4f);
        g.setColour (juce::Colour (0xff120a1f)); g.drawEllipse (cx - w * 0.2f, ry - h * 0.22f, w * 0.4f, w * 0.4f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool highlighted, bool /*down*/) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
        const auto accent = button.findColour (juce::ToggleButton::tickColourId);
        const bool on = button.getToggleState();
        const float alpha = button.isEnabled() ? 1.0f : 0.4f;

        g.setColour ((on ? accent : panelLight.brighter (highlighted ? 0.2f : 0.0f)).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

        g.setColour ((on ? background : text).withMultipliedAlpha (alpha));
        g.setFont (font (13.0f));
        g.drawText (button.getButtonText().toUpperCase(), bounds, juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool /*down*/,
                       int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

        juce::Path arrow;
        const float ax = (float) width - 20.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax, ay - 3.0f, ax + 8.0f, ay - 3.0f, ax + 4.0f, ay + 3.0f);
        g.setColour (text.withAlpha (box.isEnabled() ? 0.9f : 0.3f));
        g.fillPath (arrow);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = button.getToggleState();
        auto c = on ? button.findColour (juce::TextButton::buttonOnColourId)
                    : button.findColour (juce::TextButton::buttonColourId);
        if (highlighted) c = c.brighter (0.15f);
        if (down) c = c.darker (0.2f);
        g.setColour (c.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle (bounds, 6.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return font (juce::jmin (13.0f, (float) buttonHeight * 0.5f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
    juce::Font getPopupMenuFont() override { return font (14.0f, false); }

    // ---- tooltips (the "i" bubbles) ----
    static juce::TextLayout layoutTooltip (const juce::String& tipText)
    {
        juce::AttributedString as;
        as.setJustification (juce::Justification::topLeft);
        as.append (tipText, font (13.0f, false), juce::Colour (0xfff4ecff));
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (as, 250.0f);
        return tl;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parentArea) override
    {
        const auto tl = layoutTooltip (tip);
        const int w = (int) std::ceil (tl.getWidth()) + 24, h = (int) std::ceil (tl.getHeight()) + 18;
        const int x = pos.x > parentArea.getCentreX() ? pos.x - (w + 10) : pos.x + 14;
        const int y = pos.y > parentArea.getCentreY() ? pos.y - (h + 8) : pos.y + 10;
        return juce::Rectangle<int> (x, y, w, h).constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& tipText, int width, int height) override
    {
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        g.setColour (juce::Colour (0xff0a0616).withAlpha (0.97f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colour (0xfff4ecff).withAlpha (0.25f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        layoutTooltip (tipText).draw (g, r.reduced (12.0f, 9.0f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (12, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }
};
