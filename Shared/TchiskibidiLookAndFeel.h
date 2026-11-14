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

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
    juce::Font getPopupMenuFont() override { return font (14.0f, false); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (12, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }
};
