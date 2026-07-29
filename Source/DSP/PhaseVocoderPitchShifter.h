#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

namespace dpwim {

class PhaseVocoderPitchShifter {
public:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kOversampling = 4;
    static constexpr int kHopSize = kFftSize / kOversampling;
    static constexpr int kLatencySamples = kFftSize - kHopSize;

    PhaseVocoderPitchShifter();

    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* const* channels, int channelCount,
                 int frames, double pitchRatio) noexcept;

private:
    struct Channel {
        int rover = kLatencySamples;
        std::vector<float> inputFifo;
        std::vector<float> outputFifo;
        std::vector<float> outputAccum;
        std::vector<float> lastPhase;
        std::vector<float> sumPhase;
        std::vector<float> analysisMagnitude;
        std::vector<float> analysisBin;
        std::vector<float> synthesisMagnitude;
        std::vector<float> synthesisBin;
        std::vector<juce::dsp::Complex<float>> fftInput;
        std::vector<juce::dsp::Complex<float>> fftOutput;
        std::vector<juce::dsp::Complex<float>> fftInverse;
    };

    float processSample(Channel& channel, float input,
                        double pitchRatio) noexcept;
    void processFrame(Channel& channel,
                      double pitchRatio) noexcept;
    static void prepareChannel(Channel& channel);

    juce::dsp::FFT fft_{kFftOrder};
    std::array<Channel, 2> channels_;
    std::vector<float> window_;
};

} // namespace dpwim
