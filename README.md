# Plugin Syncronizer

This is an audio plugin that hosts other audio plugins and keeps them synchronized across multiple instances. Also supports MIDI and sidechain inputs.

Download from the [releases page](https://github.com/luismrguimaraes/PluginSyncronizer/releases).

So far, **VST3** is the most tested format and VST3 (Plugin Syncronizer) hosting VST3 (other plugins) is the most stable combination.

This is **NOT** a vibe-coded plugin, a lot of care is being taken to develop and test the plugin.

### Demo

https://github.com/user-attachments/assets/551a12d8-c295-42ee-aa34-b3db620c8f2f

### How to make macOS trust the plugin

- open Terminal
- write (without running): `sudo xattr -rd com.apple.quarantine`
- drag and drop the plugin into the terminal
- press Enter, enter your password, then press Enter again

## Notes for developers

### Code style conventions

- member_variable\_
- a_regular_variable
- functionParameter
- kSomeConstantThatRemainsUnchangedDuringRuntime
- free_function()

### For template updates

Check [this link](https://melatonin.dev/manuals/pamplejuce/life-with-pamplejuce/how-to-update-my-project/).
