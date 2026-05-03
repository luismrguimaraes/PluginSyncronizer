#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SharedData.hpp"
#include "farbot/include/farbot/RealtimeObject.hpp"
#include "juce_core/system/juce_PlatformDefs.h"
#include <algorithm>
#include <cstddef>
#include <locale>
#include <memory>
#include <tuple>

namespace
{
    constexpr const char* kInnerStateTag = "inner_state";
    constexpr const char* kGroupTag = "group";
    constexpr const char* kEditorStyleTag = "editor_style";

    constexpr int kAsyncCallProcessingMs { 5 };
}
//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
            ),
      apvts_ { *this, nullptr, "Params", { std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "test_id", 1 }, "Test Param", juce::NormalisableRange<float>(), 0.0f) } },
      async_call_processing_timer_ { [this]() { async_caller_.process(); } }
{
    app_properties_.setStorageParameters([&] {
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

    juce::addDefaultFormatsToManager(plugin_format_manager_);

    if (auto savedPluginList = app_properties_.getUserSettings()->getXmlValue("pluginList_"))
        plugin_list_.recreateFromXml(*savedPluginList);

    juce::MessageManagerLock lock; // why is this here?
    plugin_list_.addChangeListener(this);

    shared_data_manager_.addListener(this);
    this->addListener(this);

    async_call_processing_timer_.startTimer(kAsyncCallProcessingMs);
}

PluginProcessor::~PluginProcessor()
{
    shared_data_manager_.removeListener(this);
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

void PluginProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String PluginProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void PluginProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    active_ = true;

    setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);

    if (*inner != nullptr)
    {
        (*inner)->setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);
        (*inner)->prepareToPlay(sampleRate, samplesPerBlock);
    }

    float_process_buffer_ = juce::AudioBuffer<float> { 2, samplesPerBlock };
    // // Use this method as the place to do any pre-playback
    // // initialisation that you need..
    // juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    active_ = false;

    if (*inner != nullptr)
        (*inner)->releaseResources();
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void PluginProcessor::reset()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    if (*inner != nullptr)
        (*inner)->reset();
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput = layouts.getMainInputChannelSet();

    if (mainInput != mainOutput)
        return false;

    return true;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    jassert(!isUsingDoublePrecision());

    // todo: add test to check if this ever allocates
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::realtime> inner(inner_);
    if (*inner != nullptr)
    {
        (*inner)->processBlock(buffer, midiMessages);
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<double>& buffer,
    juce::MidiBuffer& midiMessages)
{
    // ToDo: test (function untested)
    jassert(isUsingDoublePrecision());
    jassert(buffer.getNumChannels() < float_process_buffer_.getNumChannels());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        std::transform(buffer.getReadPointer(ch), buffer.getReadPointer(ch) + buffer.getNumSamples(), float_process_buffer_.getWritePointer(ch), [](auto sample) {
            return static_cast<float>(sample);
        });
    }

    this->processBlock(float_process_buffer_, midiMessages);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

//==============================================================================
void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::XmlElement xml("state");

    auto groupNode = std::make_unique<juce::XmlElement>(kGroupTag);
    groupNode->addTextElement(juce::String { group_ });
    xml.addChildElement(groupNode.release());

    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
    if (*inner != nullptr)
    {
        xml.setAttribute(kEditorStyleTag, (int) editor_style_);
        xml.addChildElement((*inner)->getPluginDescription().createXml().release());
        xml.addChildElement([&inner] {
            juce::MemoryBlock innerState;
            (*inner)->getStateInformation(innerState);

            auto stateNode = std::make_unique<juce::XmlElement>(kInnerStateTag);
            stateNode->addTextElement(innerState.toBase64Encoding());
            return stateNode.release();
        }());
    }

    xml.setAttribute("accept_changes", accept_changes_);
    xml.setAttribute("propagate_changes", propagate_changes_.load());

    xml.writeTo(juce::File { "~/Desktop/test.xml" });

    const auto text = xml.toString();
    destData.replaceAll(text.toRawUTF8(), text.getNumBytesAsUTF8());

    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto xml = juce::XmlDocument::parse(juce::String(juce::CharPointer_UTF8(static_cast<const char*>(data)), (size_t) sizeInBytes));

    if (auto* pluginNode = xml->getChildByName("PLUGIN"))
    {
        juce::PluginDescription pd;
        pd.loadFromXml(*pluginNode);

        {
            juce::MemoryBlock inner_state;
            inner_state.fromBase64Encoding(xml->getChildElementAllSubText(kInnerStateTag, {}));

            createSharedPluginInstance(pd,
                (EditorStyle) xml->getIntAttribute(kEditorStyleTag, 0),
                inner_state);
        }

        group_ = xml->getIntAttribute(kGroupTag, 0);

        accept_changes_ = xml->getBoolAttribute("accept_changes");
        propagate_changes_.store(xml->getBoolAttribute("propagate_changes"));
    }

    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void PluginProcessor::audioProcessorParameterChanged(AudioProcessor* processor,
    int parameterIndex,
    float newValue)
{
    // todo: add test to check if this allocates
    async_caller_.callAsync([this, processor, parameterIndex, newValue]() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        {
            NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
            if (processor != (*inner).get()) return;
        }

        notifyHost();
        if (propagate_changes_.load())
        {
            shared_data_manager_.updateParameterValue(this, parameterIndex, newValue);
        }
    });
}

void PluginProcessor::audioProcessorChanged([[maybe_unused]] AudioProcessor* processor, [[maybe_unused]] const juce::AudioProcessorListener::ChangeDetails& details)
{
}

void PluginProcessor::propagateState()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::MemoryBlock mb;
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
    (*inner)->getStateInformation(mb);
    shared_data_manager_.updateInnerState(this, mb);
}

bool PluginProcessor::getAcceptChanges()
{
    return accept_changes_;
}

void PluginProcessor::setAcceptChanges(bool accept)
{
    if (accept_changes_ == accept) return;

    accept_changes_ = accept;
    notifyHost();
}

bool PluginProcessor::getPropagateChanges()
{
    return propagate_changes_.load();
}

void PluginProcessor::setPropagateChanges(bool propagate)
{
    if (propagate_changes_.load() == propagate) return;

    propagate_changes_.store(propagate);
    notifyHost();
}

void PluginProcessor::callbackLoadInner(const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (inner_plugin_description_.has_value() && pluginDescription.isDuplicateOf(inner_plugin_description_.value()))
    {
        NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
        jassert(*inner != nullptr);
        return;
    }

    setNewPlugin(pluginDescription, stateMemoryBlock);
}

void PluginProcessor::callbackLoadInnerState(const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (!accept_changes_) return;

    if (!stateMemoryBlock.isEmpty())
    {
        if (!inner_plugin_description_.has_value())
        {
            setNewPlugin(pluginDescription, stateMemoryBlock);
        }
        else
        {
            jassert(pluginDescription.isDuplicateOf(inner_plugin_description_.value()));

            NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
            jassert(*inner != nullptr);
            (*inner)->setStateInformation(stateMemoryBlock.getData(), (int) stateMemoryBlock.getSize());
        }
    }
}

void PluginProcessor::callbackUpdateParameterValue(int parameterIndex, float newValue)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    jassert(*inner != nullptr);

    if (!accept_changes_) return;

    jassert(parameterIndex < (*inner)->getParameters().size());
    auto param = (*inner)->getHostedParameter(parameterIndex);
    param->setValue(newValue); // Don't notify to avoid infinite loop (listener -> shared -> here -> listener -> ...)

    async_caller_.callAsync([this]() {
        notifyHost();
    });
}

void PluginProcessor::setNewPlugin(const juce::PluginDescription& pd, const juce::MemoryBlock& mb)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto callback = [this, mb, pd](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) {
        if (error.isNotEmpty())
        {
            auto options = juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon,
                "Plugin Instantiation Failed",
                error);
            message_box_ = juce::AlertWindow::showScopedAsync(options, nullptr);
            return;
        }

        editor_style_ = EditorStyle::newWindow;
        inner_plugin_description_ = pd;

        jassert(instance != nullptr);
        if (!mb.isEmpty())
            instance->setStateInformation(mb.getData(), (int) mb.getSize());

        // In a 'real' plugin, we'd also need to set the bus configuration of the instance plugin.
        // One possibility would be to match the bus configuration of the wrapper plugin, but
        // the instance plugin isn't guaranteed to support the same layout. Alternatively, we
        // could try to apply a reasonably similar layout, and maintain a mapping between the
        // instance/outer channel layouts.
        //
        // In any case, it is essential that the instance plugin is told about the bus
        // configuration that will be used. The AudioBuffer passed to the instance plugin must also
        // exactly match this layout.
        //
        // ToDo: this ^

        if (active_)
        {
            instance->setRateAndBufferSizeDetails(getSampleRate(), getBlockSize());
            // if (!instance->setBusesLayout (getBusesLayout()))
            // {
            //     std::cout << "Layout not supported\n";
            // }
            instance->prepareToPlay(getSampleRate(), getBlockSize());
        }

        instance->addListener(this);

        {
            NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
            *inner = std::move(instance);
            is_a_inner_loaded_.store(true);
        }

        juce::NullCheckedInvocation::invoke(plugin_changed_);
    };

    plugin_format_manager_.createPluginInstanceAsync(pd, getSampleRate(), getBlockSize(), callback);

    notifyHost();
}

void PluginProcessor::createSharedPluginInstance(const juce::PluginDescription& pd, [[maybe_unused]] EditorStyle where, const juce::MemoryBlock& mb)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto callback_for_shared_instance = [this, mb, pd](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) {
        if (error.isNotEmpty())
        {
            auto options = juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon,
                "Plugin Load Failed",
                error);
            message_box_ = juce::AlertWindow::showScopedAsync(options, nullptr);
            return;
        }

        if (instance != nullptr && !mb.isEmpty())
            instance->setStateInformation(mb.getData(), (int) mb.getSize());

        if (active_)
        {
            instance->setRateAndBufferSizeDetails(getSampleRate(), getBlockSize());
            // if (!instance->setBusesLayout (getBusesLayout()))
            // {
            //     std::cout << "Layout not supported\n";
            // }
            instance->prepareToPlay(getSampleRate(), getBlockSize());
        }

        shared_data_manager_.updateInner(this, std::move(instance), pd);
    };

    plugin_format_manager_.createPluginInstanceAsync(pd, getSampleRate(), getBlockSize(), callback_for_shared_instance);
}

void PluginProcessor::callbackRemoveInner()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    if (*inner == nullptr)
        return;

    editor_needs_deletion_.store(true);
    juce::NullCheckedInvocation::invoke(plugin_changed_); // make PluginEditor delete inner editor
    *inner = nullptr;
    is_a_inner_loaded_.store(false);
    inner_plugin_description_.reset();
    shared_data_manager_.removeInner();
    editor_needs_deletion_.store(false);

    notifyHost();
}

void PluginProcessor::notifyHost()
{
    auto change_details = AudioProcessorListener::ChangeDetails {};
    updateHostDisplay(change_details.withParameterInfoChanged(true).withNonParameterStateChanged(true));
}

void PluginProcessor::removeInner()
{
    shared_data_manager_.removeInner();
}

bool PluginProcessor::editorNeedsDeletion()
{
    return editor_needs_deletion_.load();
}

bool PluginProcessor::isPluginLoaded() const
{
    return is_a_inner_loaded_.load();
}

std::unique_ptr<juce::AudioProcessorEditor> PluginProcessor::createInnerEditor()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
    return rawToUniquePtr((*inner)->hasEditor() ? (*inner)->createEditorIfNeeded() : nullptr);
}

EditorStyle PluginProcessor::getEditorStyle() const noexcept
{
    return editor_style_;
}

void PluginProcessor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &plugin_list_)
        return;

    auto savedPluginList = plugin_list_.createXml();
    if (savedPluginList)
    {
        app_properties_.getUserSettings()->setValue("pluginList_", savedPluginList.get());
        app_properties_.saveIfNeeded();
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
