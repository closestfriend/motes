#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MotesField : public juce::Component, private juce::Timer {
public:
    explicit MotesField(MotesProcessor&);
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override;
    void spawnAmbient(int n);
    static constexpr int MAX_MOTES = 400;
    struct Mote { float x=0,y=0,vx=0,vy=0,age=0,life=1,size=1,hue=0; bool active=false; };
    Mote motes[MAX_MOTES];
    int cursor = 0;
    MotesProcessor& proc;
    juce::Random rng;
};
