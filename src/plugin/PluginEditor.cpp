#include "PluginEditor.h"

static const char* kParamIds[13] = {
    "density","grain","position","spread","panspread","pitchsemi","pitchcent",
    "macro","attack","decay","sustain","release","master" };
static const char* kParamLabels[13] = {
    "Density","Grain","Position","Spread","Pan","Pitch","Fine",
    "Macro","Attack","Decay","Sustain","Release","Master" };

void MotesEditor::addKnob(const juce::String& paramId, const juce::String& text) {
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
    k->slider.setLookAndFeel(&lnf);
    addAndMakeVisible(k->slider);
    k->label.setText(text, juce::dontSendNotification);
    k->label.setJustificationType(juce::Justification::centred);
    k->label.setFont(juce::Font(11.0f));
    addAndMakeVisible(k->label);
    k->attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, paramId, k->slider);
    knobs.push_back(std::move(k));
}

MotesEditor::MotesEditor(MotesProcessor& p) : AudioProcessorEditor(p), proc(p) {
    for (int i=0;i<13;i++) addKnob(kParamIds[i], kParamLabels[i]);
    addAndMakeVisible(loadBtn);
    loadBtn.onClick = [this]{
        chooser = std::make_unique<juce::FileChooser>("Load WAV", juce::File(), "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){
                auto f = fc.getResult();
                if (f.existsAsFile()) { proc.loadWavFile(f); fileLabel.setText(f.getFileName(), juce::dontSendNotification); }
            });
    };
    fileLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7c8aa0));
    fileLabel.setText("(built-in tone)", juce::dontSendNotification);
    addAndMakeVisible(fileLabel);
    setSize(768, 512);
}

MotesEditor::~MotesEditor() { for (auto& k : knobs) k->slider.setLookAndFeel(nullptr); }

void MotesEditor::paint(juce::Graphics& g) {
    // procedural background gradient (Task 3 will draw the particle field into fieldArea)
    juce::ColourGradient grad(juce::Colour(0xff0a0d12), 0, 0,
                              juce::Colour(0xff0e1420), 0, (float)getHeight(), false);
    g.setGradientFill(grad); g.fillAll();
    // wordmark
    g.setColour(juce::Colour(0x66c6d4e6));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("motes", fieldArea.reduced(10), juce::Justification::topLeft);
    // group labels above the knob panel
    g.setColour(juce::Colour(0xff5a6678));
    g.setFont(juce::Font(10.0f));
    int panelY = fieldArea.getBottom() + 34;
    g.drawText("GRAIN",   16,  panelY-16, 180, 12, juce::Justification::left);
    g.drawText("PITCH / POSITION", 200, panelY-16, 180, 12, juce::Justification::left);
    g.drawText("AMP",     392, panelY-16, 180, 12, juce::Justification::left);
    g.drawText("MASTER",  660, panelY-16, 100, 12, juce::Justification::left);
}

void MotesEditor::resized() {
    auto r = getLocalBounds();
    fieldArea = r.removeFromTop(300);                 // hero field (Task 3)
    auto bar = r.removeFromTop(28);                   // sample bar
    loadBtn.setBounds(bar.removeFromLeft(110).reduced(3));
    fileLabel.setBounds(bar.reduced(4));
    // knob panel: 4 groups. Grain(4) Pitch/Pos(4) Amp(4) Master(1)
    auto panel = r.reduced(8, 6);
    const int kw = 88, kh = 96;
    auto place = [&](int idx, int col, int xoff){
        knobs[idx]->slider.setBounds(xoff + col*kw, panel.getY()+14, kw-6, kh-18);
        knobs[idx]->label.setBounds (xoff + col*kw, panel.getY()+kh-2, kw-6, 12);
    };
    // Grain group: density(0), grain(1), spread(3), panspread(4)
    place(0,0,8); place(1,1,8); place(3,2,8); place(4,3,8);
    // Pitch/Pos group: position(2), macro(7), pitchsemi(5), pitchcent(6)
    place(2,0,200); place(7,1,200); place(5,2,200); place(6,3,200);
    // Amp group: attack(8), decay(9), sustain(10), release(11)
    place(8,0,392); place(9,1,392); place(10,2,392); place(11,3,392);
    // Master(12)
    place(12,0,660);
}
