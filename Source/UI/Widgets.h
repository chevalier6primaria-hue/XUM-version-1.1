#pragma once
#include "UI/Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace xum
{
using APVTS     = juce::AudioProcessorValueTreeState;
using SliderAtt = APVTS::SliderAttachment;
using ComboAtt  = APVTS::ComboBoxAttachment;
using ButtonAtt = APVTS::ButtonAttachment;

inline juce::RangedAudioParameter& paramRef (APVTS& apvts, const juce::String& id)
{
    auto* p = apvts.getParameter (id);
    jassert (p != nullptr);
    return *p;
}

// ------------------------------------------------------------------ Knob con etiqueta
class Knob : public juce::Component
{
public:
    Knob (APVTS& a, const juce::String& paramId, const juce::String& text,
          juce::Colour arc = Theme::accent, juce::Colour labelColour = Theme::ink)
        : apvts (a)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, arc);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (Theme::font (10.f));
        label.setColour (juce::Label::textColourId, labelColour);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attach (paramId);
    }

    void attach (const juce::String& paramId)
    {
        att.reset();
        att = std::make_unique<SliderAtt> (apvts, paramId, slider);
        auto& p = paramRef (apvts, paramId);
        slider.setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue()));
    }

    void setLabel (const juce::String& t) { label.setText (t, juce::dontSendNotification); }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromBottom (13));
        slider.setBounds (b);
    }

    juce::Slider slider;

private:
    APVTS& apvts;
    juce::Label label;
    std::unique_ptr<SliderAtt> att;
};

// ------------------------------------------------------------------ Caja numérica arrastrable (OCT +2, UNISON 3...)
class NumBox : public juce::Component
{
public:
    NumBox (juce::RangedAudioParameter& p, const juce::String& prefixText, bool signedValue = false,
            const juce::String& captionText = {})
        : param (p), prefix (prefixText), showSign (signedValue), caption (captionText),
          att (p, [this] (float v) { value = v; repaint(); })
    {
        att.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto box = caption.isEmpty() ? b : b.removeFromTop (b.getHeight() - 14.f);
        g.setColour (Theme::darkPanel2);
        g.fillRoundedRectangle (box, 3.f);
        const int v = (int) std::lround (value);
        juce::String t = juce::String (v);
        if (showSign && v > 0) t = "+" + t;
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.f));
        g.drawText (prefix, box.reduced (6.f, 0.f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffd9e6ee));
        g.setFont (Theme::font (12.f));
        g.drawText (t, box.reduced (6.f, 0.f), juce::Justification::centredRight);
        if (caption.isNotEmpty())
        {
            g.setColour (Theme::ink);
            g.setFont (Theme::font (10.f));
            g.drawText (caption, b, juce::Justification::centred);   // b = franja inferior tras removeFromTop
        }
    }

    void mouseDown (const juce::MouseEvent&) override { dragStart = value; att.beginGesture(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        att.setValueAsPartOfGesture (clampToRange (dragStart + (float) (-e.getDistanceFromDragY() / 8)));
    }
    void mouseUp (const juce::MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        att.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        att.setValueAsCompleteGesture (clampToRange (std::round (value) + (w.deltaY > 0 ? 1.f : -1.f)));
    }

private:
    float clampToRange (float v) const
    {
        const auto& r = param.getNormalisableRange();
        return std::clamp (std::round (v), r.start, r.end);
    }

    juce::RangedAudioParameter& param;
    juce::String prefix;
    bool showSign;
    juce::String caption;
    float value = 0.f, dragStart = 0.f;
    juce::ParameterAttachment att;
};

// ------------------------------------------------------------------ LED con texto (ToggleButton)
class LedToggle : public juce::ToggleButton
{
public:
    explicit LedToggle (const juce::String& text = {}, juce::Colour textColour = Theme::text)
        : label (text), textCol (textColour) { setClickingTogglesState (true); }

    void attach (APVTS& apvts, const juce::String& paramId)
    {
        att.reset();
        att = std::make_unique<ButtonAtt> (apvts, paramId, *this);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto b = getLocalBounds().toFloat();
        const float d = 10.f, cy = b.getCentreY();
        const juce::Rectangle<float> led (b.getX() + 2.f, cy - d * 0.5f, d, d);
        const bool on = getToggleState();
        if (on) { g.setColour (Theme::green.withAlpha (0.25f)); g.fillEllipse (led.expanded (3.f)); }
        g.setColour (on ? Theme::green : juce::Colour (0xff23303a));
        g.fillEllipse (led);
        g.setColour (juce::Colour (0x88000000));
        g.drawEllipse (led, 1.f);
        if (label.isNotEmpty())
        {
            g.setColour (over ? juce::Colours::white : textCol);
            g.setFont (Theme::font (10.f));
            g.drawText (label, b.withTrimmedLeft (d + 8.f), juce::Justification::centredLeft);
        }
    }

private:
    juce::String label;
    juce::Colour textCol;
    std::unique_ptr<ButtonAtt> att;
};

// ------------------------------------------------------------------ Selector de forma de onda del sub (2x2 iconos)
class WaveSelector : public juce::Component
{
public:
    explicit WaveSelector (juce::RangedAudioParameter& p)
        : param (p), att (p, [this] (float v) { selected = (int) std::lround (v); repaint(); })
    {
        att.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < 4; ++i)
        {
            auto c = cell (i).toFloat().reduced (2.f);
            const bool sel = i == selected;
            g.setColour (sel ? Theme::dispBot : Theme::darkPanel2);
            g.fillRoundedRectangle (c, 3.f);
            if (sel) { g.setColour (Theme::green); g.drawRoundedRectangle (c, 3.f, 1.2f); }

            juce::Path p;
            auto a = c.reduced (6.f, 12.f);
            const int n = 40;
            for (int k = 0; k <= n; ++k)
            {
                const float t = (float) k / (float) n;
                float y = 0.f;
                switch (i)
                {
                    case 0: y = std::sin (t * 6.2831853f); break;
                    case 1: y = t < 0.5f ? 1.f : -1.f; break;
                    case 2: y = 1.f - 4.f * std::abs (t - 0.5f); break;
                    default: y = 2.f * t - 1.f; break;
                }
                const float px = a.getX() + t * a.getWidth(), py = a.getCentreY() - y * a.getHeight() * 0.5f;
                if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            g.setColour (sel ? Theme::green : Theme::textDim);
            g.strokePath (p, juce::PathStrokeType (1.6f));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 4; ++i)
            if (cell (i).contains (e.getPosition())) { att.setValueAsCompleteGesture ((float) i); break; }
    }

private:
    juce::Rectangle<int> cell (int i) const
    {
        const int w = getWidth() / 2, h = getHeight() / 2;
        return { (i % 2) * w, (i / 2) * h, w, h };
    }
    juce::RangedAudioParameter& param;
    int selected = 0;
    juce::ParameterAttachment att;
};

// ------------------------------------------------------------------ Selector segmentado horizontal (SERIE | PARALELO)
class SegSelector : public juce::Component
{
public:
    SegSelector (juce::RangedAudioParameter& p, juce::StringArray names)
        : labels (std::move (names)), att (p, [this] (float v) { selected = (int) std::lround (v); repaint(); })
    {
        att.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        const int n = labels.size();
        for (int i = 0; i < n; ++i)
        {
            auto c = juce::Rectangle<float> ((float) getWidth() * (float) i / (float) n, 0.f,
                                             (float) getWidth() / (float) n, (float) getHeight()).reduced (1.f);
            const bool sel = i == selected;
            g.setColour (sel ? Theme::accent.withAlpha (0.35f) : Theme::darkPanel2);
            g.fillRoundedRectangle (c, 3.f);
            g.setColour (sel ? juce::Colours::white : Theme::textDim);
            g.setFont (Theme::font (9.5f));
            g.drawText (labels[i].toUpperCase(), c, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = labels.size();
        att.setValueAsCompleteGesture ((float) std::clamp (e.x * n / std::max (1, getWidth()), 0, n - 1));
    }

private:
    juce::StringArray labels;
    int selected = 0;
    juce::ParameterAttachment att;
};

// ------------------------------------------------------------------ Pestaña (ENV 1, LFO 3, VELO...)
class TabButton : public juce::Component
{
public:
    TabButton (const juce::String& t) : text (t) {}
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void setLedStyle (bool b) { ledStyle = b; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (selected ? Theme::darkPanel2.brighter (0.15f) : Theme::darkPanel);
        g.fillRect (b);
        g.setColour (Theme::bg);
        g.drawVerticalLine (getWidth() - 1, 0.f, (float) getHeight());
        if (selected && ! ledStyle) { g.setColour (Theme::accent); g.fillRect (b.removeFromBottom (2.f)); }
        g.setColour (selected ? juce::Colours::white : Theme::text);
        g.setFont (Theme::font (ledStyle ? 14.f : 12.f));
        auto textArea = getLocalBounds();
        if (ledStyle)
        {
            const float cx = b.getCentreX(), cy = 16.f;
            if (selected) { g.setColour (Theme::green.withAlpha (0.3f)); g.fillEllipse (cx - 6.f, cy - 6.f, 12.f, 12.f); }
            g.setColour (selected ? Theme::green : juce::Colour (0xff27333c));
            g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.f, 7.f);
            g.setColour (selected ? juce::Colours::white : Theme::text);
            textArea = textArea.withTrimmedTop (26);
        }
        g.drawText (text, textArea, juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;

private:
    juce::String text;
    bool selected = false, ledStyle = false;
};
}
