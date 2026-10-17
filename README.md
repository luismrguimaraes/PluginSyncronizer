# Plugin Syncronizer

This is an audio plugin that hosts other audio plugins and keeps them syncronized across multiple instances.

Download from the Releases section.

So far, **VST3** is the most tested format and VST3 (Plugin Syncronizer) hosting VST3 (other plugins) is the most stable combination.

Download from the [releases page](https://github.com/luismrguimaraes/PluginSyncronizer/releases).

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
- kSomeConstant
- free_function()

### For template updates

Check [this link](https://melatonin.dev/manuals/pamplejuce/life-with-pamplejuce/how-to-update-my-project/).
