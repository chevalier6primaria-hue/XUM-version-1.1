#pragma once
#include "UI/Theme.h"

namespace xum
{
class XumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    XumLookAndFeel()
    {
        using namespace Theme;
        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, darkPanel2);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffd9e6ee));
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, darkPanel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return Theme::font (11.f); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 3.f, 1.f);

        juce::Path arrow;
        const float cx = (float) w - 11.f, cy = (float) h * 0.5f;
        arrow.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const float diam = (float) std::min (w, h) - 2.f;
        const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
        const float outer = diam * 0.5f, ringR = outer - 1.5f, body = outer - 5.f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto  arcColour = s.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;
        track.addCentredArc (cx, cy, ringR, ringR, 0.f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0x55000000));
        g.strokePath (track, juce::PathStrokeType (2.2f));

        float from = startAngle;
        if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
            from = startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle);
        juce::Path value;
        value.addCentredArc (cx, cy, ringR, ringR, 0.f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (arcColour);
        g.strokePath (value, juce::PathStrokeType (2.6f));

        g.setGradientFill (juce::ColourGradient (Theme::knobBody.brighter (0.25f), cx, cy - body,
                                                 Theme::knobBody.darker (0.4f), cx, cy + body, false));
        g.fillEllipse (cx - body, cy - body, body * 2.f, body * 2.f);
        g.setColour (juce::Colour (0x66000000));
        g.drawEllipse (cx - body, cy - body, body * 2.f, body * 2.f, 1.f);

        const float sa = std::sin (angle), ca = -std::cos (angle);
        g.setColour (juce::Colour (0xffe9f2f7));
        g.drawLine (cx + sa * body * 0.25f, cy + ca * body * 0.25f,
                    cx + sa * body * 0.92f, cy + ca * body * 0.92f, 2.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           const juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
            return;
        }
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.f, (float) h * 0.32f);
        g.setColour (Theme::darkPanel2);
        g.fillRoundedRectangle (r, 3.f);
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float mid = bipolar ? (float) x + (float) w * 0.5f : (float) x;
        g.setColour (Theme::accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (std::min (mid, pos), r.getY(), std::abs (pos - mid), r.getHeight()), 3.f);
        if (bipolar)
        {
            g.setColour (Theme::textDim);
            g.drawVerticalLine ((int) mid, r.getY(), r.getBottom());
        }
    }
};
}
