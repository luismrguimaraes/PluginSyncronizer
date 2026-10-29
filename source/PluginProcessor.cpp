#include "PluginProcessor.hpp"
#include "SharedData.hpp"
#include "farbot/include/farbot/RealtimeObject.hpp"
#include "gui/PluginEditor.hpp"
#include "juce_core/system/juce_PlatformDefs.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <locale>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace
{
    inline const juce::String kInnerStateTag = "inner_state";
    inline const juce::String kGroupTag = "group";

    constexpr int kAsyncCallProcessingMs { 5 };
    constexpr int kSupportedNumChannels { 2 };

    constexpr bool kInput { true };
    constexpr bool kOutput { false };
    constexpr int kMainBus { 0 };
    constexpr int kSidechainBus { 1 };

}

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
            ),
      apvts_ { *this, nullptr, "Params", {
#ifdef DEBUG
                                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "test_id", 1 }, "Test Param", juce::NormalisableRange<float>(), 0.0f)
#endif
                                         } },
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
        opt.folderName = JucePlugin_Manufacturer;
        return opt;
    }());

    juce::addDefaultFormatsToManager(plugin_format_manager_);

    if (auto savedPluginList = app_properties_.getUserSettings()->getXmlValue("pluginList_"))
        plugin_list_.recreateFromXml(*savedPluginList);

    juce::String logs_folder { "Logs" };
    logs_directory_ = app_properties_.getUserSettings()->getFile().getParentDirectory().getChildFile(logs_folder);
    logs_directory_.createDirectory();

    juce::MessageManagerLock lock; // why is this here?
    plugin_list_.addChangeListener(this);

    shared_data_manager_.addListener(this);
    this->addListener(this);

    async_call_processing_timer_.startTimer(kAsyncCallProcessingMs);

    auto shared_pd = shared_data_manager_.getPluginDescription();
    if (shared_pd.has_value())
    {
        setNewPlugin(shared_pd.value(), {});
    }
}

PluginProcessor::~PluginProcessor()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    {
        NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
        needs_editor_deletion_ = true;
        // make PluginEditor delete inner editor
        juce::NullCheckedInvocation::invoke(plugin_changed_);
        *inner = nullptr;
        needs_editor_deletion_ = false;
    }

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
    // VST3 in REAPER may call this in audio thread
    // E.g. when setting the sidechain buffer

    inner_prepared_.store(false);

    async_caller_.callAsync([sampleRate, samplesPerBlock, this]() {
        if (!isBusesLayoutSupported(getBusesLayout())) return;

        setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);

#ifdef DEBUG
        juce_debugger_.prepareToPlay(samplesPerBlock, getTotalNumInputChannels(), getTotalNumOutputChannels());
#endif

        NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime>
            inner(inner_);
        if ((*inner) != nullptr)
        {
            auto success = prepareInstance(inner->get(), sampleRate, samplesPerBlock);
            if (!success)
            {
                auto options = juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon,
                    "Could not prepare plugin. Removing it...",
                    "");
                message_box_ = juce::AlertWindow::showScopedAsync(options, nullptr);
                removeInner();
            }
            else
            {
                inner_prepared_.store(true);
            }
        }

        double_process_buffer_ = juce::AudioBuffer<float> { std::max(getTotalNumInputChannels(), getTotalNumOutputChannels()), samplesPerBlock };

        are_we_playing_.store(true);
    });
}

void PluginProcessor::releaseResources()
{
    // VST3 in REAPER may call this in audio thread
    // E.g. when setting the sidechain buffer

    are_we_playing_.store(false);

    async_caller_.callAsync([this]() {
        NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

        inner_prepared_.store(false);
        if (*inner != nullptr)
            (*inner)->releaseResources();
    });
}

void PluginProcessor::reset()
{
    // CLAP in REAPER and VST3 in Bitwig calls reset in audio thread
    async_caller_.callAsync([this]() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

        if (*inner != nullptr)
            (*inner)->reset();
    });
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& main_output = layouts.getMainOutputChannelSet();
    const auto& main_input = layouts.getMainInputChannelSet();

    if (layouts.getBuses(kInput).size() > 2) return false;

    auto main_buses_size_supported = main_output.size() == kSupportedNumChannels && main_input.size() == kSupportedNumChannels;

    if (auto has_sidechain_bus = layouts.getBuses(kInput).size() == 2)
    {
        const auto& sidechain_input = layouts.getChannelSet(kInput, kSidechainBus);
        auto sidechain_bus_size_supported = sidechain_input.size() <= kSupportedNumChannels;

        return sidechain_bus_size_supported && main_buses_size_supported;
    }

    return main_buses_size_supported;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    jassert(!isUsingDoublePrecision());

    if (!inner_prepared_.load() || !are_we_playing_.load()) return;

#ifdef DEBUG
    juce_debugger_.readInput(buffer);
#endif

    jassert(inner_process_buffer_.getNumSamples() == buffer.getNumSamples());
    int num_samples = std::min(inner_process_buffer_.getNumSamples(), buffer.getNumSamples());
    for (auto ch = 0; ch < std::min(inner_process_buffer_.getNumChannels(), buffer.getNumChannels()); ch++)
    {
        auto channel_data = inner_process_buffer_.getWritePointer(ch);
        for (auto i = 0; i < num_samples; i++)
        {
            channel_data[i] = buffer.getReadPointer(ch)[i];
        }
    }

    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::realtime> inner(inner_);
    if (*inner != nullptr)
    {
        (*inner)->processBlock(inner_process_buffer_, midiMessages);
    }

    for (auto ch = 0; ch < std::min(inner_process_buffer_.getNumChannels(), buffer.getNumChannels()); ch++)
    {
        auto channel_data = buffer.getWritePointer(ch);
        for (auto i = 0; i < num_samples; i++)
        {
            channel_data[i] = inner_process_buffer_.getReadPointer(ch)[i];
        }
    }

#ifdef DEBUG
    juce_debugger_.readInput(buffer);
#endif
}

void PluginProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    // ToDo: test (function untested)
    jassert(isUsingDoublePrecision());
    jassert(buffer.getNumChannels() < double_process_buffer_.getNumChannels());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        std::ranges::transform(buffer.getReadPointer(ch), buffer.getReadPointer(ch) + buffer.getNumSamples(), double_process_buffer_.getWritePointer(ch), [](double sample) {
            return static_cast<float>(sample);
        });
    }

    this->processBlock(double_process_buffer_, midiMessages);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new gui::PluginEditor(*this, logs_directory_);
}

//==============================================================================
void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::XmlElement xml { utils::kMainStateTag };

    xml.setAttribute(kGroupTag, group_);

    xml.addChildElement([this] {
        juce::MemoryBlock state;
        editor_props_.getStateInformation(state, logs_directory_);

        auto stateNode = std::make_unique<juce::XmlElement>(utils::kEditorPropsTag);
        stateNode->addTextElement(state.toBase64Encoding());
        return stateNode.release();
    }());

    {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
        if (*inner != nullptr)
        {
            xml.addChildElement((*inner)->getPluginDescription().createXml().release());
            xml.addChildElement([&inner] {
                juce::MemoryBlock state;
                (*inner)->getStateInformation(state);

                auto stateNode = std::make_unique<juce::XmlElement>(kInnerStateTag);
                stateNode->addTextElement(state.toBase64Encoding());
                return stateNode.release();
            }());
        }
    }

    xml.addChildElement([this] {
        juce::MemoryBlock state;
        shared_data_manager_.getStateInformation(state, logs_directory_);

        auto stateNode = std::make_unique<juce::XmlElement>(utils::kSharedStateTag);
        stateNode->addTextElement(state.toBase64Encoding());
        return stateNode.release();
    }());

    xml.setAttribute("accept_changes", accept_changes_);
    xml.setAttribute("propagate_changes", propagate_changes_.load());

    xml.writeTo(logs_directory_.getChildFile("PluginState.xml"));

    const auto xlm_as_string = xml.toString();
    destData.replaceAll(xlm_as_string.toRawUTF8(), xlm_as_string.getNumBytesAsUTF8());
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto xml = juce::XmlDocument::parse(juce::String(juce::CharPointer_UTF8(static_cast<const char*>(data)), static_cast<size_t>(sizeInBytes)));
    require(xml != nullptr);
    xml->writeTo(logs_directory_.getChildFile("LoadedPluginState.xml"));

    group_ = xml->getIntAttribute(kGroupTag, -1);
    require(group_ != -1);

    if (auto* plugin_description_node = xml->getChildByName("PLUGIN"))
    {
        juce::PluginDescription pd;
        pd.loadFromXml(*plugin_description_node);

        juce::MemoryBlock inner_state;
        inner_state.fromBase64Encoding(xml->getChildElementAllSubText(kInnerStateTag, {}));
        require(!inner_state.isEmpty());

        setNewPlugin(pd, inner_state);
    }

    juce::MemoryBlock shared_data_man_state;
    shared_data_man_state.fromBase64Encoding(xml->getChildElementAllSubText(utils::kSharedStateTag, {}));
    shared_data_manager_.setStateInformation(shared_data_man_state.getData(), static_cast<int>(shared_data_man_state.getSize()), logs_directory_);

    juce::MemoryBlock editor_props_state;
    editor_props_state.fromBase64Encoding(xml->getChildElementAllSubText(utils::kEditorPropsTag, {}));
    editor_props_.setStateInformation(editor_props_state.getData(), static_cast<int>(editor_props_state.getSize()));

    accept_changes_ = xml->getBoolAttribute("accept_changes");
    propagate_changes_.store(xml->getBoolAttribute("propagate_changes"));
}

void PluginProcessor::audioProcessorParameterChanged(AudioProcessor* processor,
    int parameterIndex,
    float newValue)
{
    if (processor == this) return;

    // assume processor is inner, we cannot acquire a realtime mutatable here because process block runs concurrently

    async_caller_.callAsync([this, parameterIndex, newValue]() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        {
            NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
            if (*inner == nullptr) return;
        }

        if (propagate_changes_.load())
        {
            shared_data_manager_.updateParameterValue(this, parameterIndex, newValue);
        }

        notifyHost();
    });
}

void PluginProcessor::audioProcessorChanged(AudioProcessor* processor, const juce::AudioProcessorListener::ChangeDetails& details)
{
    if (processor == this) return;

    // assume processor is inner, we cannot acquire a realtime mutatable here because process block runs concurrently

    async_caller_.callAsync([this, details]() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
        if (*inner == nullptr) return;

        if (details.latencyChanged && getLatencySamples() != (*inner)->getLatencySamples())
        {
            setLatencySamples((*inner)->getLatencySamples());
        }

        notifyHost();
    });
}

void PluginProcessor::propagateState()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::MemoryBlock mb;
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
    (*inner)->getStateInformation(mb);
    shared_data_manager_.updateInnerState(this, mb);
}

bool PluginProcessor::innerSupportsSidechain() const
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    if ((*inner) == nullptr) return false;
    return (*inner)->getBusCount(kInput) > 1;
}

bool PluginProcessor::getAcceptChanges() const
{
    return accept_changes_;
}

void PluginProcessor::setAcceptChanges(bool accept)
{
    if (accept_changes_ == accept) return;

    accept_changes_ = accept;
    notifyHost();
}

bool PluginProcessor::getPropagateChanges() const
{
    return propagate_changes_.load();
}

void PluginProcessor::setPropagateChanges(bool propagate)
{
    if (propagate_changes_.load() == propagate) return;

    propagate_changes_.store(propagate);
    notifyHost();
}

void PluginProcessor::callbackLoadInner(const juce::PluginDescription& pluginDescription)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (inner_plugin_description_.has_value() && utils::areDuplicates(pluginDescription, inner_plugin_description_.value()))
        return;

    setNewPlugin(pluginDescription, {});
}

void PluginProcessor::callbackLoadInnerState([[maybe_unused]] const juce::PluginDescription& pluginDescription, const juce::MemoryBlock& stateMemoryBlock)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (!accept_changes_) return;

    if (!stateMemoryBlock.isEmpty())
    {
        jassert(inner_plugin_description_.has_value());
        jassert(utils::areDuplicates(pluginDescription,inner_plugin_description_.value()));

        NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
        jassert(*inner != nullptr);
        (*inner)->setStateInformation(stateMemoryBlock.getData(), (int) stateMemoryBlock.getSize());
    }
}

void PluginProcessor::callbackUpdateParameterValue(int parameterIndex, float newValue)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    if (*inner == nullptr)
    {
        jassert(inner_plugin_description_.has_value()); // inner creation is async but its description assignment is not
        return;
    }

    if (!accept_changes_) return;

    require(parameterIndex < (*inner)->getParameters().size());
    auto param = (*inner)->getHostedParameter(parameterIndex);
    param->setValue(newValue); // Don't notify to avoid infinite loop (listener -> shared -> here -> listener -> ...)

    notifyHost();
}

float PluginProcessor::getUserScale()
{
    return editor_props_.user_scale;
}

void PluginProcessor::setUserScale(float user_scale)
{
    editor_props_.user_scale = user_scale;
}

juce::KnownPluginList& PluginProcessor::getPluginList() noexcept
{
    return plugin_list_;
}

void PluginProcessor::setInnerParameter(int parameterIndex, float newValue)
{
#ifndef TESTS_TARGET
    require(false); // only call on Tests
#endif

    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    if (*inner == nullptr)
    {
        // inner creation is async but its description assignment is not
        jassert(inner_plugin_description_.has_value());
        return;
    }

    jassert(parameterIndex < (*inner)->getParameters().size());
    auto param = (*inner)->getHostedParameter(parameterIndex);
    param->setValue(newValue);

    auto shared_ptr = *inner;
    auto audio_processor_cast = static_cast<juce::AudioProcessor*>(shared_ptr.get());
    jassert(audio_processor_cast != nullptr);
    audioProcessorParameterChanged(audio_processor_cast, parameterIndex, newValue);
}

float PluginProcessor::getInnerParameter(int parameterIndex) const
{
#ifndef TESTS_TARGET
    require(false); // only call on Tests
#endif

    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    if (*inner == nullptr)
    {
        std::terminate();
    }

    jassert(parameterIndex < (*inner)->getParameters().size());
    auto param = (*inner)->getHostedParameter(parameterIndex);
    return param->getValue();
}

juce::AudioProcessor* PluginProcessor::getInner()
{
#ifndef TESTS_TARGET
    require(false); // only call on Tests
#endif

    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    return static_cast<juce::AudioProcessor*>(inner->get());
}

void PluginProcessor::setNewPlugin(const juce::PluginDescription& pd, const juce::MemoryBlock& mb, std::optional<std::function<void()>> onSuccessCallback)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    inner_plugin_description_ = pd;

    const auto callback = [this, mb, pd, onSuccessCallback = std::move(onSuccessCallback)](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

        // In case inner_plugin_description_ was modified meanwhile for some reason
        if (!inner_plugin_description_.has_value() || !utils::areDuplicates(pd, inner_plugin_description_.value())) return;
#ifdef DEBUG
        jassert(utils::areDuplicates(std::move(instance->getPluginDescription()), pd));
#endif
        auto pd_to_log = instance->getPluginDescription().createXml();
        pd_to_log->writeTo(logs_directory_.getChildFile("LastPluginDescription.xml"));

        if (error.isNotEmpty())
        {
            auto options = juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon,
                "Plugin Instantiation Failed",
                error);
            message_box_ = juce::AlertWindow::showScopedAsync(options, nullptr);
            inner_plugin_description_.reset();
            return;
        }

        jassert(instance != nullptr);
        if (!mb.isEmpty())
            instance->setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));

        bool inner_prepared { false };
        if (are_we_playing_.load())
        {
            auto prepare_success = prepareInstance(instance.get(), getSampleRate(), getBlockSize());
            if (!prepare_success)
            {
                auto options = juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon,
                    "Could not prepare plugin. Cancelling load...",
                    "");
                message_box_ = juce::AlertWindow::showScopedAsync(options, nullptr);
                inner_plugin_description_.reset();
                return;
            }
            inner_prepared = true;
        }

        instance->addListener(this);

        setLatencySamples(instance->getLatencySamples());

        bool editor_was_open { false };
        {
            NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
            if ((*inner) != nullptr && (*inner)->getActiveEditor() != nullptr)
            {
                editor_was_open = true;
                // this can happen on Undo
                needs_editor_deletion_ = true;
                juce::NullCheckedInvocation::invoke(plugin_changed_);
                needs_editor_deletion_ = false;
            }
            *inner = std::move(instance);
            inner_prepared_.store(inner_prepared);
            is_an_inner_loaded_ = true;
        }
        wants_inner_editor_ = editor_was_open;
        juce::NullCheckedInvocation::invoke(plugin_changed_);
        wants_inner_editor_ = false;

        if (onSuccessCallback.has_value())
            (*onSuccessCallback)();
    };

    plugin_format_manager_.createPluginInstanceAsync(pd, getSampleRate(), getBlockSize(), callback);
}

void PluginProcessor::createSharedPluginInstance(const juce::PluginDescription& pd, [[maybe_unused]] EditorStyle where)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    setNewPlugin(pd, {}, [this]() {
        jassert(inner_plugin_description_.has_value());
        shared_data_manager_.updateInnerPd(this, inner_plugin_description_.value());
        notifyHost();
    });
}

std::optional<juce::PluginDescription> PluginProcessor::getPluginDescription() const
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    return inner_plugin_description_;
}

void PluginProcessor::callbackRemoveInner()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);

    if (*inner == nullptr)
        return;

    inner_prepared_.store(false);
    needs_editor_deletion_ = true;
    // make PluginEditor delete inner editor
    juce::NullCheckedInvocation::invoke(plugin_changed_);
    *inner = nullptr;

    is_an_inner_loaded_ = false;
    inner_plugin_description_.reset();
    shared_data_manager_.removeInner();
    needs_editor_deletion_ = false;

    setLatencySamples(0);

    notifyHost();
}

void PluginProcessor::notifyHost()
{
    // todo: add a dummy parameter to change here. updateHostDisplay does not work on a AU build in REAPER
    auto change_details = AudioProcessorListener::ChangeDetails {};
    updateHostDisplay(change_details.withNonParameterStateChanged(true));
}

void PluginProcessor::numChannelsChanged()
{
}

void PluginProcessor::numBusesChanged()
{
    jassertfalse;
}

void PluginProcessor::processorLayoutsChanged()
{
    // VST3 in REAPER may call this in audio thread
    // E.g. when setting the sidechain buffer

    async_caller_.callAsync([this]() {
        if (!isBusesLayoutSupported(getBusesLayout())) return;

        prepareToPlay(getSampleRate(), getBlockSize());

        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        {
            NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
            if ((*inner) != nullptr)
            {
                (*inner)->processorLayoutsChanged();
            }
        }
    });
}

void PluginProcessor::removeInner()
{
    shared_data_manager_.removeInner();
}

bool PluginProcessor::needsEditorDeletion() const
{
    return needs_editor_deletion_;
}

bool PluginProcessor::isAnInnerLoaded() const
{
    return is_an_inner_loaded_;
}

bool PluginProcessor::wantsInnerEditor() const
{
    return wants_inner_editor_;
}

std::unique_ptr<juce::AudioProcessorEditor> PluginProcessor::createInnerEditor() const
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    NRTMAudioPluginInstance::ScopedAccess<farbot::ThreadType::nonRealtime> inner(inner_);
    return rawToUniquePtr((*inner)->hasEditor() ? (*inner)->createEditorIfNeeded() : nullptr);
}

EditorStyle PluginProcessor::getEditorStyle() const noexcept
{
    // This is not good, this EditorStyle should probably go away,
    // but it may be insteresting to try the other style it in the future
    return EditorStyle::newWindow;
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

bool PluginProcessor::prepareInstance(juce::AudioPluginInstance* instance, double sampleRate, int samplesPerBlock)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    jassert(instance != nullptr);

    auto new_bus_layout = getBusesLayout();

    const std::int64_t kIterationCeil { 100 };

    auto bus_matcher = [&instance, &new_bus_layout](bool is_input) {
        auto our_size = new_bus_layout.getBuses(is_input).size();

        std::int64_t iter_count { 0 };
        for (; iter_count < kIterationCeil; ++iter_count)
        {
            auto instance_size = instance->getBusesLayout().getBuses(is_input).size();
            if (our_size == instance_size) break;

            if (our_size > instance_size)
            {
                if (!instance->canAddBus(is_input)) break;
                instance->addBus(is_input);
            }
            else
            {
                if (!instance->canRemoveBus(is_input)) break;
                instance->removeBus(is_input);
            }
        }
        jassert(iter_count < kIterationCeil);
    };

    bus_matcher(kInput);
    bus_matcher(kOutput);

    auto num_channels_matcher = [this, &instance](bool is_input, int bus_index) -> bool {
        auto instance_bus = instance->getBus(is_input, bus_index);

        auto our_num_channels = getBus(is_input, bus_index)->getNumberOfChannels();
        auto success = instance_bus->setNumberOfChannels(our_num_channels);

        return success;
    };

    if (auto failed = instance->getBusCount(kInput) > 0 && !num_channels_matcher(kInput, kMainBus))
    {
        return false;
    }
    if (auto failed = instance->getBusCount(kOutput) > 0 && !num_channels_matcher(kOutput, kMainBus))
    {
        return false;
    }
    if (instance->getBusCount(kInput) > 1)
    {
        // Sidechain bus
        if (auto failed = !num_channels_matcher(kInput, kSidechainBus))
        {
            return false;
        }
    }

    instance->setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);
    instance->prepareToPlay(sampleRate, samplesPerBlock);

    inner_process_buffer_ = juce::AudioBuffer<float> {
        std::max(instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels()), samplesPerBlock
    };
    fill_with_silence(inner_process_buffer_);

    return true;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
