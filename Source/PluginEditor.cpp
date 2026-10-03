#include "PluginEditor.h"
#include <UIData.h>
#include <cstring>

static std::vector<std::byte> toBytes (const char* data, int size)
{
    std::vector<std::byte> v ((size_t) size);
    std::memcpy (v.data(), data, (size_t) size);
    return v;
}

juce::RangedAudioParameter* SHZEditor::findParam (const juce::String& id)
{
    return proc.apvts.getParameter (id);
}

void SHZEditor::setParamValue (const juce::String& id, float v)
{
    for (int i = 0; i < Id::Count; ++i)
    {
        if (id == kParams[i].id)
        {
            if (auto* p = findParam (id))
            {
                p->setValueNotifyingHost (p->convertTo0to1 (v));
                lastSent[(size_t) i] = rawPtrs[(size_t) i]->load();   // UI already knows this value, no echo needed
            }
            return;
        }
    }
}

SHZEditor::SHZEditor (SHZProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    for (int i = 0; i < Id::Count; ++i)
    {
        rawPtrs[(size_t) i] = proc.apvts.getRawParameterValue (kParams[i].id);
        lastSent[(size_t) i] = std::numeric_limits<float>::quiet_NaN();   // forces a first full sync
    }

    using Opt = juce::WebBrowserComponent::Options;
    using Done = juce::WebBrowserComponent::NativeFunctionCompletion;
    using Args = juce::Array<juce::var>;

    Opt opts = Opt{}
       #if JUCE_WINDOWS
        .withBackend (Opt::Backend::webview2)   // default on Windows is Internet Explorer -> blank page
       #endif
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withWinWebView2Options (Opt::WinWebView2{}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("SISHHIN_HZ_MACHINE_WebView2")))
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })

        // ---- parameters: UI -> plugin (plain values in real units, e.g. gain 0..10, gate -80..-20) ----
        .withNativeFunction ("setParam", [this] (const Args& a, Done done)
        {
            if (a.size() >= 2)
                setParamValue (a[0].toString(), (float) (double) a[1]);
            done (juce::var());
        })
        .withNativeFunction ("setParams", [this] (const Args& a, Done done)
        {
            if (a.size() >= 1)
            {
                const auto parsed = juce::JSON::parse (a[0].toString());
                if (auto* o = parsed.getDynamicObject())
                    for (const auto& nv : o->getProperties())
                        setParamValue (nv.name.toString(), (float) (double) nv.value);
            }
            done (juce::var());
        })
        .withNativeFunction ("gesture", [this] (const Args& a, Done done)
        {
            if (a.size() >= 2)
                if (auto* prm = findParam (a[0].toString()))
                {
                    if ((int) a[1] != 0) prm->beginChangeGesture();
                    else                 prm->endChangeGesture();
                }
            done (juce::var());
        })
        // ---- parameters: plugin -> UI (full snapshot) ----
        .withNativeFunction ("getParams", [this] (const Args&, Done done)
        {
            auto* o = new juce::DynamicObject();
            for (int i = 0; i < Id::Count; ++i)
                o->setProperty (juce::Identifier (kParams[i].id), (double) rawPtrs[(size_t) i]->load());
            done (juce::var (o));
        })

        // ---- impulse responses ----
        .withNativeFunction ("loadIR", [this] (const Args&, Done done)
        {
            chooser = std::make_unique<juce::FileChooser> ("Load impulse response", juce::File(),
                                                           "*.wav;*.aif;*.aiff;*.flac");
            juce::Component::SafePointer<SHZEditor> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe, done] (const juce::FileChooser& fc)
                {
                    if (safe == nullptr) { done (juce::var()); return; }
                    const auto f = fc.getResult();
                    if (f.existsAsFile())
                        safe->proc.loadIRFile (f);
                    done (safe->proc.getIRName());
                });
        })
        .withNativeFunction ("clearIR", [this] (const Args&, Done done)
        {
            proc.clearIR();
            done (juce::String());
        })
        .withNativeFunction ("getIR", [this] (const Args&, Done done)
        {
            done (proc.getIRName());
        });

    web = std::make_unique<juce::WebBrowserComponent> (opts);
    addAndMakeVisible (*web);

    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (720, 520, 1700, 1200);
    setSize (1080, 760);
    startTimerHz (30);
}

SHZEditor::~SHZEditor()
{
    stopTimer();
}

void SHZEditor::resized()
{
    if (web != nullptr)
        web->setBounds (getLocalBounds());
}

void SHZEditor::timerCallback()
{
    if (web == nullptr)
        return;

    auto* m = new juce::DynamicObject();
    m->setProperty ("in",  (double) proc.engine.inPeak.load());
    m->setProperty ("out", (double) proc.engine.outPeak.load());
    web->emitEventIfBrowserIsVisible ("meters", juce::var (m));

    // host automation / preset recall / project load -> UI
    auto* changed = new juce::DynamicObject();
    bool any = false;
    for (int i = 0; i < Id::Count; ++i)
    {
        const float v = rawPtrs[(size_t) i]->load();
        if (v != lastSent[(size_t) i])
        {
            lastSent[(size_t) i] = v;
            changed->setProperty (juce::Identifier (kParams[i].id), (double) v);
            any = true;
        }
    }
    juce::var cv (changed);
    if (any)
        web->emitEventIfBrowserIsVisible ("params", cv);
}

std::optional<juce::WebBrowserComponent::Resource> SHZEditor::getResource (const juce::String& url)
{
    const auto path = url.upToFirstOccurrenceOf ("?", false, false);

    if (path == "/" || path.isEmpty() || path == "/index.html")
        return juce::WebBrowserComponent::Resource { toBytes (UIData::index_html, UIData::index_htmlSize), "text/html" };

    if (path == "/js/juce/index.js")
        return juce::WebBrowserComponent::Resource { toBytes (UIData::index_js, UIData::index_jsSize), "text/javascript" };

    return std::nullopt;
}
