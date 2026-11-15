#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Panels.h"
#include "UI/Oscilloscope.h"

// Interfaz de XUM 1.1 (Tapelab). Se diseña sobre un lienzo base de 1190 x 740 y se
// escala proporcionalmente al redimensionar la ventana.
class XumEditor : public juce::AudioProcessorEditor
{
public:
    explicit XumEditor (XumAudioProcessor&);
    ~XumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Canvas : public juce::Component { void paint (juce::Graphics&) override; };

    void showPage (int page);
    void selectEnv (int i);
    void selectLfo (int i);
    void selectMap (int i);

    XumAudioProcessor& proc;
    xum::XumLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    Canvas content;

    xum::Knob mainKnob;
    xum::SubPanel sub;
    xum::OscPanel oscA, oscB, oscC;
    xum::NoisePanel noise;
    xum::FilterPanel filters;
    xum::MacroPanel macros;
    xum::EnvSection envSec;
    xum::LfoSection lfoSec;
    xum::MapDisplay mapDisp;
    xum::MatrixPage matrix;
    xum::Oscilloscope scope;
    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<xum::TabButton>> pageTabs, envTabs, lfoTabs, mapTabs;
    int page = 0, envSel = 0, lfoSel = 0, mapSel = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XumEditor)
};
