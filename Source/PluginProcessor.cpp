#include "PluginProcessor.h"
#include "PluginEditor.h"

// ═══════════════════════════════════════════════════════════════════════════════
// LinearPredictor
// ═══════════════════════════════════════════════════════════════════════════════

void LinearPredictor::train(const float* data, int length)
{
    trained = false;
    if (length < ORDER + 1) return;

    // ── Autocorrelation lags 0..ORDER ──────────────────────────────────────────
    double r[ORDER + 1] = {};
    for (int lag = 0; lag <= ORDER; ++lag)
        for (int i = lag; i < length; ++i)
            r[lag] += (double)data[i] * (double)data[i - lag];

    if (r[0] < 1e-12) return;  // silence — don't update coefficients

    // ── Levinson-Durbin recursion ──────────────────────────────────────────────
    double a[ORDER]    = {};
    double aTmp[ORDER] = {};
    double e = r[0];

    for (int i = 0; i < ORDER; ++i)
    {
        double lambda = r[i + 1];
        for (int j = 0; j < i; ++j)
            lambda -= a[j] * r[i - j];
        lambda /= e;

        // Stability check
        if (std::abs(lambda) >= 1.0) lambda *= 0.99 / std::abs(lambda);

        for (int j = 0; j < i; ++j)
            aTmp[j] = a[j] - lambda * a[i - 1 - j];
        aTmp[i] = lambda;

        for (int j = 0; j <= i; ++j)
            a[j] = aTmp[j];

        e *= (1.0 - lambda * lambda);
        if (e < 1e-12) break;
    }

    for (int i = 0; i < ORDER; ++i)
        coeffs[i] = (float)a[i];

    trained = true;
}

void LinearPredictor::generateFuture(const float* history, int histLen,
                                      float* future, int futureLength) const
{
    if (!trained || histLen < ORDER)
    {
        std::fill(future, future + futureLength, 0.0f);
        return;
    }

    // Scratch buffer: [history tail | predicted future]
    std::vector<float> buf(history, history + histLen);

    for (int i = 0; i < futureLength; ++i)
    {
        const int sz = (int)buf.size();
        float pred = 0.0f;
        for (int j = 0; j < ORDER; ++j)
            pred += coeffs[j] * buf[sz - 1 - j];

        // Soft-clip to prevent explosion
        pred = juce::jlimit(-1.0f, 1.0f, pred);
        future[i] = pred;
        buf.push_back(pred);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// AlcubierreProcessor
// ═══════════════════════════════════════════════════════════════════════════════

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [](float v) { return juce::String(v, 2); };

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "temporal_displacement", 1 },
        "X — Temporal Displacement",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "warp_density", 1 },
        "Y — Warp Density",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "dimensional_scatter", 1 },
        "Z — Dimensional Scatter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "alpha_omega", 1 },
        "ALPHAOMEGA — Future Inference",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "grain_size_ms", 1 },
        "Grain Size (ms)",
        juce::NormalisableRange<float>(10.0f, 500.0f, 0.1f, 0.4f), 80.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " ms"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "feedback", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.99f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

    return layout;
}

AlcubierreProcessor::AlcubierreProcessor()
    : AudioProcessor(BusesProperties()
                       .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ALCUBIERRE_STATE", createLayout())
{
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        futureData[ch].resize(FUTURE_BUFFER_LENGTH, 0.0f);
        futurePtrs[ch] = futureData[ch].data();
        lpcTrainBuf[ch].resize(LPC_TRAIN_LENGTH, 0.0f);
    }
}

AlcubierreProcessor::~AlcubierreProcessor() = default;

// ─── Bus layout ───────────────────────────────────────────────────────────────
bool AlcubierreProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Accept mono or stereo in/out if they match.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

// ─── Prepare ─────────────────────────────────────────────────────────────────
void AlcubierreProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int ringLen = (int)(10.0 * sampleRate);
    ringBuffer.prepare(MAX_CHANNELS, ringLen);

    granularEngine.prepare(sampleRate, samplesPerBlock);
    glitchEngine.prepare(sampleRate);

    const int blockAlloc = juce::jmax(samplesPerBlock, 512);
    writeInputBuffer.setSize(MAX_CHANNELS, blockAlloc, false, true, false);
    wetBuffer       .setSize(MAX_CHANNELS, blockAlloc, false, true, false);
    feedbackBuffer  .setSize(MAX_CHANNELS, blockAlloc, false, true, false);
    feedbackBuffer.clear();
    writeInputBuffer.clear();
    wetBuffer.clear();

    samplesSinceLastFutureUpdate = 0;

    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        std::fill(futureData[ch].begin(), futureData[ch].end(), 0.0f);
        std::fill(lpcTrainBuf[ch].begin(), lpcTrainBuf[ch].end(), 0.0f);
    }
}

void AlcubierreProcessor::releaseResources() {}

// ─── Future buffer update ─────────────────────────────────────────────────────
void AlcubierreProcessor::updateFutureBuffer(int numChannels)
{
    const int ringLen   = ringBuffer.getSize();
    const int trainLen  = juce::jmin(LPC_TRAIN_LENGTH, ringLen);
    const int wp        = ringBuffer.getWritePosition();

    for (int ch = 0; ch < juce::jmin(numChannels, MAX_CHANNELS); ++ch)
    {
        // Collect training samples from ring buffer (most recent `trainLen` samples)
        for (int i = 0; i < trainLen; ++i)
        {
            const int pos = ((wp - trainLen + i) + ringLen * 2) % ringLen;
            lpcTrainBuf[ch][i] = (float)ringBuffer.readAtAbsolutePosition(ch, (double)pos);
        }

        predictors[ch].train(lpcTrainBuf[ch].data(), trainLen);

        predictors[ch].generateFuture(lpcTrainBuf[ch].data(), trainLen,
                                       futureData[ch].data(), FUTURE_BUFFER_LENGTH);
    }

    // Mirror channel 0 to channel 1 if mono
    if (numChannels == 1)
    {
        std::copy(futureData[0].begin(), futureData[0].end(), futureData[1].begin());
    }
}

// ─── processBlock ─────────────────────────────────────────────────────────────
void AlcubierreProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn   = getTotalNumInputChannels();
    const int totalOut  = getTotalNumOutputChannels();
    const int numSamples= buffer.getNumSamples();
    const int numCh     = juce::jmin({ totalIn, totalOut, MAX_CHANNELS });

    // Clear any extra output channels we won't fill
    for (int ch = numCh; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    // ── Read parameters ────────────────────────────────────────────────────────
    const float x          = apvts.getRawParameterValue("temporal_displacement")->load();
    const float y          = apvts.getRawParameterValue("warp_density")->load();
    const float z          = apvts.getRawParameterValue("dimensional_scatter")->load();
    const float ao         = apvts.getRawParameterValue("alpha_omega")->load();
    const float grainSizeMs= apvts.getRawParameterValue("grain_size_ms")->load();
    const float feedback   = apvts.getRawParameterValue("feedback")->load();
    const float mix        = apvts.getRawParameterValue("mix")->load();

    // ── Build ring-buffer write signal: dry input + feedback ──────────────────
    for (int ch = 0; ch < numCh; ++ch)
    {
        writeInputBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        const float* fb = feedbackBuffer.getReadPointer(ch);
        float*       wi = writeInputBuffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            wi[i] = juce::jlimit(-1.0f, 1.0f, wi[i] + fb[i] * feedback);
    }

    // ── Write to ring buffer ───────────────────────────────────────────────────
    {
        const float* ptrs[MAX_CHANNELS];
        for (int ch = 0; ch < numCh; ++ch)
            ptrs[ch] = writeInputBuffer.getReadPointer(ch);
        // Fill any unused channels by mirroring ch 0
        for (int ch = numCh; ch < MAX_CHANNELS; ++ch)
            ptrs[ch] = ptrs[0];
        ringBuffer.write(ptrs, MAX_CHANNELS, numSamples);
    }

    // ── Periodically update LPC future buffer ─────────────────────────────────
    samplesSinceLastFutureUpdate += numSamples;
    if (samplesSinceLastFutureUpdate >= FUTURE_UPDATE_INTERVAL)
    {
        updateFutureBuffer(numCh);
        samplesSinceLastFutureUpdate = 0;
    }

    // ── Granular synthesis ────────────────────────────────────────────────────
    // Resize wet buffer if needed (no alloc — setSize with keepExisting = false)
    if (wetBuffer.getNumSamples() < numSamples)
        wetBuffer.setSize(MAX_CHANNELS, numSamples, false, true, true);

    {
        // Create a sub-buffer view of the right number of channels and samples
        juce::AudioBuffer<float> wetOut(wetBuffer.getArrayOfWritePointers(),
                                         numCh, numSamples);
        granularEngine.processBlock(wetOut, ringBuffer,
                                     futurePtrs.data(), FUTURE_BUFFER_LENGTH,
                                     x, y, z, ao, grainSizeMs);

        // ── Glitch engine ──────────────────────────────────────────────────────
        glitchEngine.process(wetOut, z);

        // ── Save feedback ──────────────────────────────────────────────────────
        if (feedbackBuffer.getNumSamples() < numSamples)
            feedbackBuffer.setSize(MAX_CHANNELS, numSamples, false, true, true);

        for (int ch = 0; ch < numCh; ++ch)
            feedbackBuffer.copyFrom(ch, 0, wetOut, ch, 0, numSamples);

        // ── Dry/Wet mix to output ──────────────────────────────────────────────
        for (int ch = 0; ch < numCh; ++ch)
        {
            float*       out = buffer.getWritePointer(ch);
            const float* wet = wetOut.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                out[i] = out[i] * (1.0f - mix) + wet[i] * mix;
        }
    }
}

// ─── State ────────────────────────────────────────────────────────────────────
void AlcubierreProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void AlcubierreProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// ─── Factory ──────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AlcubierreProcessor();
}
