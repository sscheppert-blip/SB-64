// The Standalone app: JUCE's own, but in a window with the system's title bar. JUCE draws its own title
// bar to make room for its "Options" button; here the options live in the menu bar instead (macOS: the
// app's menu bar at the top of the screen; Windows/Linux: a menu bar inside the window).
// Enabled by JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP (CMakeLists.txt).

#if defined (JucePlugin_Build_Standalone) && JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

// the same headers as JUCE's own Standalone wrapper (juce_audio_plugin_client_Standalone.cpp)
#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
    using namespace juce;

    class OptionsMenu final : public MenuBarModel
    {
    public:
        explicit OptionsMenu (StandaloneFilterWindow& w) : window (w) {}

        StringArray getMenuBarNames() override { return { "Options" }; }

        PopupMenu getMenuForIndex (int, const String&) override
        {
            // the same items (and ids) as JUCE's Options button, see StandaloneFilterWindow::handleMenuResult
            PopupMenu m;
            m.addItem (1, "Audio/MIDI Settings...");
            m.addSeparator();
            m.addItem (2, "Save current state...");
            m.addItem (3, "Load a saved state...");
            m.addSeparator();
            m.addItem (4, "Reset to default state");
            return m;
        }

        void menuItemSelected (int itemId, int) override { window.handleMenuResult (itemId); }

    private:
        StandaloneFilterWindow& window;
    };

    class SB64StandaloneApp final : public JUCEApplication
    {
    public:
        SB64StandaloneApp()
        {
            PropertiesFile::Options options;
            options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
            options.filenameSuffix      = ".settings";
            options.osxLibrarySubFolder = "Application Support";
           #if JUCE_LINUX || JUCE_BSD
            options.folderName          = "~/.config";
           #endif
            appProperties.setStorageParameters (options);
        }

        const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
        const String getApplicationVersion() override           { return JucePlugin_VersionString; }
        bool moreThanOneInstanceAllowed() override              { return true; }
        void anotherInstanceStarted (const String&) override    {}

        void initialise (const String&) override
        {
            if (Desktop::getInstance().getDisplays().displays.isEmpty())
                return;

            mainWindow = std::make_unique<StandaloneFilterWindow> (
                getApplicationName(),
                LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(), false));

            // JUCE's Options button sits in its own title bar, which the native one replaces
            for (auto* child : mainWindow->getChildren())
                if (auto* button = dynamic_cast<TextButton*> (child); button != nullptr && button->getButtonText() == "Options")
                    button->setVisible (false);

            mainWindow->setUsingNativeTitleBar (true);
            menu = std::make_unique<OptionsMenu> (*mainWindow);

           #if JUCE_MAC
            MenuBarModel::setMacMainMenu (menu.get());
           #else
            mainWindow->setMenuBar (menu.get());
           #endif

            mainWindow->setVisible (true);
        }

        void shutdown() override
        {
           #if JUCE_MAC
            MenuBarModel::setMacMainMenu (nullptr);
           #else
            if (mainWindow != nullptr)
                mainWindow->setMenuBar (nullptr);
           #endif

            mainWindow = nullptr;
            menu = nullptr;
            appProperties.saveIfNeeded();
        }

        void systemRequestedQuit() override
        {
            if (mainWindow != nullptr)
                mainWindow->pluginHolder->savePluginState();

            if (ModalComponentManager::getInstance()->cancelAllModalComponents())
            {
                Timer::callAfterDelay (100, []
                {
                    if (auto* app = JUCEApplicationBase::getInstance())
                        app->systemRequestedQuit();
                });
            }
            else
            {
                quit();
            }
        }

    private:
        ApplicationProperties appProperties;
        std::unique_ptr<StandaloneFilterWindow> mainWindow;
        std::unique_ptr<OptionsMenu> menu;
    };
}

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new SB64StandaloneApp(); }

#endif
