#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SharedData.hpp"
#include "juce_core/system/juce_PlatformDefs.h"
#include <algorithm>
#include <cstddef>
#include <locale>
#include <memory>
#include <tuple>

namespace
{
    static constexpr const char* kInnerStateTag = "inner_state";
    static constexpr const char* kGroupTag = "group";
    static constexpr const char* kEditorStyleTag = "editor_style";
}
//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
#if !JucePlugin_IsMidiEffect
    #if !JucePlugin_IsSynth
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
    #endif
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
            ),
      apvts_ (*this, nullptr)
{
    app_properties_.setStorageParameters ([&] {
        juce::PropertiesFile::Options opt;
        opt.applicationName = getName();
        opt.commonToAllUsers = false;
        opt.doNotSave = false;
        opt.filenameSuffix = ".props";
        opt.ignoreCaseOfKeyNames = false;
        opt.storageFormat = juce::PropertiesFile::StorageFormat::storeAsXML;
        opt.osxLibrarySubFolder = "Application Support";
        return opt;
    }());

    juce::addDefaultFormatsToManager (plugin_format_manager_);

    if (auto savedPluginList = app_properties_.getUserSettings()->getXmlValue ("pluginList_"))
        plugin_list_.recreateFromXml (*savedPluginList);

    juce::MessageManagerLock lock;
    plugin_list_.addChangeListener (this);

    shared_data_manager_.addListener (this);
}

PluginProcessor::~PluginProcessor()
{
    shared_data_manager_.removeListener (this);
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
        // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl (inner_mutex_);

    active_ = true;

    setRateAndBufferSizeDetails (sampleRate, samplesPerBlock);
    if (inner_ != nullptr)
    {
        inner_->setRateAndBufferSizeDetails (sampleRate, samplesPerBlock);
        inner_->prepareToPlay (sampleRate, samplesPerBlock);
    }

    float_process_buffer_ = juce::AudioBuffer<float> { 2, samplesPerBlock };
    // // Use this method as the place to do any pre-playback
    // // initialisation that you need..
    // juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    const juce::ScopedLock sl (inner_mutex_);

    active_ = false;

    if (inner_ != nullptr)
        inner_->releaseResources();
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void PluginProcessor::reset()
{
    const juce::ScopedLock sl (inner_mutex_);

    if (inner_ != nullptr)
        inner_->reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput = layouts.getMainInputChannelSet();

    if (mainInput != mainOutput)
        return false;

    return true;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    jassert (!isUsingDoublePrecision());

    if (inner_ != nullptr)
    {
        inner_->processBlock (buffer, midiMessages);
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<double>& buffer,
    juce::MidiBuffer& midiMessages)
{
    // ToDo: test (function untested)
    jassert (isUsingDoublePrecision());
    jassert (buffer.getNumChannels() < float_process_buffer_.getNumChannels());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        std::transform (buffer.getReadPointer (ch), buffer.getReadPointer (ch) + buffer.getNumSamples(), float_process_buffer_.getWritePointer (ch), [] (auto sample) {
            return static_cast<float> (sample);
        });
    }

    this->processBlock (float_process_buffer_, midiMessages);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock sl (inner_mutex_);

    juce::XmlElement xml ("state");

    auto groupNode = std::make_unique<juce::XmlElement> (kGroupTag);
    groupNode->addTextElement (juce::String { group_ });
    xml.addChildElement (groupNode.release());

    if (inner_ != nullptr)
    {
        xml.setAttribute (kEditorStyleTag, (int) editor_style_);
        xml.addChildElement (inner_->getPluginDescription().createXml().release());
        xml.addChildElement ([this] {
            juce::MemoryBlock innerState;
            inner_->getStateInformation (innerState);

            auto stateNode = std::make_unique<juce::XmlElement> (kInnerStateTag);
            stateNode->addTextElement (innerState.toBase64Encoding());
            return stateNode.release();
        }());
    }

    xml.writeTo (juce::File { "~/Desktop/test.xml" });

    const auto text = xml.toString();
    destData.replaceAll (text.toRawUTF8(), text.getNumBytesAsUTF8());

    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const juce::ScopedLock sl (inner_mutex_);

    auto xml = juce::XmlDocument::parse (juce::String (juce::CharPointer_UTF8 (static_cast<const char*> (data)), (size_t) sizeInBytes));

    if (auto* pluginNode = xml->getChildByName ("PLUGIN"))
    {
        juce::PluginDescription pd;
        pd.loadFromXml (*pluginNode);

        juce::MemoryBlock innerState;
        innerState.fromBase64Encoding (xml->getChildElementAllSubText (kInnerStateTag, {}));

        createSharedPluginInstance (pd,
            (EditorStyle) xml->getIntAttribute (kEditorStyleTag, 0),
            innerState);

        group_ = xml->getIntAttribute (kGroupTag, 0);
    }

    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void PluginProcessor::audioProcessorParameterChanged (AudioProcessor* processor,
    int parameterIndex,
    float newValue)
{
    if (processor != inner_.get())
        return;

    if (syncing_params_)
    {
        shared_data_manager_.updateParameterValue (this, parameterIndex, newValue);
    }
}

void PluginProcessor::audioProcessorChanged ([[maybe_unused]] AudioProcessor* processor, [[maybe_unused]] const juce::AudioProcessorListener::ChangeDetails& details)
{
}

void PluginProcessor::syncInners()
{
    juce::MemoryBlock mb;
    inner_->getStateInformation (mb);
    shared_data_manager_.updateInnerState (this, mb);
}

void PluginProcessor::callbackLoadInner (const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock)
{
    if (inner_plugin_description_.has_value() && pluginDescription.isDuplicateOf (inner_plugin_description_.value()))
    {
        jassert (inner_ != nullptr);
        return;
    }

    setNewPlugin (pluginDescription, stateMemoryBlock);
}

void PluginProcessor::callbackLoadInnerState (const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock)
{
    if (!stateMemoryBlock.isEmpty())
    {
        if (!inner_plugin_description_.has_value())
        {
            setNewPlugin (pluginDescription, stateMemoryBlock);
        }
        else
        {
            jassert (pluginDescription.isDuplicateOf (inner_plugin_description_.value()));
            jassert (inner_ != nullptr);
            inner_->setStateInformation (stateMemoryBlock.getData(), (int) stateMemoryBlock.getSize());
        }
    }
}

void PluginProcessor::callbackUpdateParameterValue (int parameterIndex, float newValue)
{
    if (syncing_params_ && inner_ != nullptr)
    {
        jassert (parameterIndex < inner_->getParameters().size());
        auto param = inner_->getHostedParameter (parameterIndex);
        param->setValue (newValue);

        auto change_details = AudioProcessorListener::ChangeDetails {};
        updateHostDisplay (change_details.withNonParameterStateChanged (true));
    }
}

void PluginProcessor::setNewPlugin (const juce::PluginDescription& pd, const juce::MemoryBlock& mb)
{
    const juce::ScopedLock sl (inner_mutex_);

    const auto callback = [this, mb, pd] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) {
        if (error.isNotEmpty())
        {
            return;
        }

        editor_style_ = EditorStyle::newWindow;
        inner_plugin_description_ = pd;

        jassert (instance != nullptr);
        if (!mb.isEmpty())
            instance->setStateInformation (mb.getData(), (int) mb.getSize());

        // In a 'real' plugin, we'd also need to set the bus configuration of the instance plugin.
        // One possibility would be to match the bus configuration of the wrapper plugin, but
        // the instance plugin isn't guaranteed to support the same layout. Alternatively, we
        // could try to apply a reasonably similar layout, and maintain a mapping between the
        // instance/outer channel layouts.
        //
        // In any case, it is essential that the instance plugin is told about the bus
        // configuration that will be used. The AudioBuffer passed to the instance plugin must also
        // exactly match this layout.

        if (active_)
        {
            instance->setRateAndBufferSizeDetails (getSampleRate(), getBlockSize());
            // if (!instance->setBusesLayout (getBusesLayout()))
            // {
            //     std::cout << "Layout not supported\n";
            // }
            instance->prepareToPlay (getSampleRate(), getBlockSize());
        }

        instance->addListener (this);

        inner_ = std::move (instance);

        juce::NullCheckedInvocation::invoke (plugin_changed_);
    };

    plugin_format_manager_.createPluginInstanceAsync (pd, getSampleRate(), getBlockSize(), callback);
}

void PluginProcessor::createSharedPluginInstance (const juce::PluginDescription& pd, [[maybe_unused]] EditorStyle where, const juce::MemoryBlock& mb)
{
    const juce::ScopedLock sl (inner_mutex_);

    const auto callback_for_shared_instance = [this, mb, pd] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) {
        if (error.isNotEmpty())
        {
            auto options = juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon,
                "Plugin Load Failed",
                error);
            message_box_ = juce::AlertWindow::showScopedAsync (options, nullptr);
            return;
        }

        if (instance != nullptr && !mb.isEmpty())
            instance->setStateInformation (mb.getData(), (int) mb.getSize());

        if (active_)
        {
            instance->setRateAndBufferSizeDetails (getSampleRate(), getBlockSize());
            // if (!instance->setBusesLayout (getBusesLayout()))
            // {
            //     std::cout << "Layout not supported\n";
            // }
            instance->prepareToPlay (getSampleRate(), getBlockSize());
        }

        instance->addListener (this);
        shared_data_manager_.updateInner (this, std::move (instance), pd);
    };

    plugin_format_manager_.createPluginInstanceAsync (pd, getSampleRate(), getBlockSize(), callback_for_shared_instance);
}

void PluginProcessor::callbackRemoveInner()
{
    const juce::ScopedLock sl (inner_mutex_);

    if (inner_ == nullptr)
        return;

    inner_ = nullptr; // editor will be deleted when pluginChanged_ is called
    inner_plugin_description_.reset();
    shared_data_manager_.removeInner();
    juce::NullCheckedInvocation::invoke (plugin_changed_);
}

bool PluginProcessor::isPluginLoaded() const
{
    const juce::ScopedLock sl (inner_mutex_);
    return inner_ != nullptr;
}

std::unique_ptr<juce::AudioProcessorEditor> PluginProcessor::createInnerEditor() const
{
    const juce::ScopedLock sl (inner_mutex_);
    return rawToUniquePtr (inner_->hasEditor() ? inner_->createEditorIfNeeded() : nullptr);
}

EditorStyle PluginProcessor::getEditorStyle() const noexcept
{
    return editor_style_;
}

void PluginProcessor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source != &plugin_list_)
        return;

    auto savedPluginList = plugin_list_.createXml();
    if (savedPluginList)
    {
        app_properties_.getUserSettings()->setValue ("pluginList_", savedPluginList.get());
        app_properties_.saveIfNeeded();
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
