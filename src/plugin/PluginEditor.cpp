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
    k->label.setColour(juce::Label::textColourId, juce::Colour(0xff7c8aa0));
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
    field = std::make_unique<MotesField>(proc); addAndMakeVisible(*field);
    setSize(768, 512);
}

MotesEditor::~MotesEditor() { for (auto& k : knobs) k->slider.setLookAndFeel(nullptr); }

void MotesEditor::paint(juce::Graphics& g) {
    // procedural background gradient below the field (the field paints itself)
    juce::ColourGradient grad(juce::Colour(0xff0a0d12), 0, (float)fieldArea.getBottom(),
                              juce::Colour(0xff0e1420), 0, (float)getHeight(), false);
    g.setGradientFill(grad);
    g.fillRect(0, fieldArea.getBottom(), getWidth(), getHeight()-fieldArea.getBottom());
    // group labels in the strip between the sample bar and the knob row
    g.setColour(juce::Colour(0xff5a6678));
    g.setFont(juce::Font(10.0f));
    int labelY = fieldArea.getBottom() + 28 + 2;
    const char* groupNames[4] = { "GRAIN", "PITCH / POSITION", "AMP", "MASTER" };
    for (int gr = 0; gr < 4; ++gr)
        g.drawText(groupNames[gr], groupX[gr], labelY, 180, 12, juce::Justification::left);
}

void MotesEditor::resized() {
    auto r = getLocalBounds();
    fieldArea = r.removeFromTop(300);
    field->setBounds(fieldArea);
    auto bar = r.removeFromTop(28);
    loadBtn.setBounds(bar.removeFromLeft(110).reduced(3));
    fileLabel.setBounds(bar.reduced(4));

    // 13 knobs, one row, four groups (Grain 4 / Pitch+Pos 4 / Amp 4 / Master 1) with gaps.
    static const int kOrder[13]       = { 0,1,3,4,  2,7,5,6,  8,9,10,11,  12 };
    static const int knobsPerGroup[4] = { 4,4,4,1 };
    const int margin = 12, groupGap = 16, knobH = 78;
    const int usable = getWidth() - 2*margin - 3*groupGap;   // width for the 13 knobs
    const int knobW  = usable / 13;
    const int rowY   = fieldArea.getBottom() + 28 + 16;      // below sample bar, leaving a strip for group labels
    int o = 0, x = margin;
    for (int gr = 0; gr < 4; ++gr) {
        groupX[gr] = x;
        for (int c = 0; c < knobsPerGroup[gr]; ++c) {
            int idx = kOrder[o++];
            knobs[idx]->slider.setBounds(x, rowY, knobW-4, knobH);
            knobs[idx]->label.setBounds (x, rowY + knobH, knobW-4, 12);
            x += knobW;
        }
        x += groupGap;
    }
}
