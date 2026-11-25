// Krotos-Studio-style layout: 4 tabs x 4 layer engines, sound browser on a custom folder, drag & drop,
// XY perform pad, Quick FX, mixer, preset save/load.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include "../PluginProcessor.h"

namespace f1nz::ui
{
inline bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

// ------------------------------------------------------------------ one layer engine card
class SlotCard : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget
{
public:
    explicit SlotCard (F1nzProcessor& p) : proc (p)
    {
        for (auto* c : { &modeBox, &linkBox, &loadBox, &eventBox }) { styleCombo (*c); addAndMakeVisible (*c); }
        for (int m = 0; m < f1nz::kNumModes; ++m) modeBox.addItem (f1nz::modeName (m), m + 1);
        linkBox.addItemList ({ "Level: fixed", "Level: rpm", "Level: speed", "Level: throttle" }, 1);
        loadBox.addItemList ({ "Any load", "On-throttle", "Off-throttle" }, 1);
        eventBox.addItemList ({ "On: shift", "On: lift / BOV", "On: limiter", "On: ignition" }, 1);

        for (auto* t : { &muteB, &soloB, &loopB, &normB })
        {
            t->setClickingTogglesState (true);
            t->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0202a));
            t->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c34));
            addAndMakeVisible (*t);
        }
        clearB.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a2024)); addAndMakeVisible (clearB);

        setupSlider (levelS, -36.0, 12.0, 0.1, [] (double v) { return "Level " + juce::String (v, 1) + " dB"; });
        setupSlider (panS, -1.0, 1.0, 0.01, [] (double v) { return v < -0.01 ? "Pan L" + juce::String ((int) (-v * 100)) : (v > 0.01 ? "Pan R" + juce::String ((int) (v * 100)) : juce::String ("Pan C")); });
        setupSlider (pitchS, -24.0, 24.0, 0.1, [] (double v) { return "Pitch " + juce::String (v, 1) + " st"; });
        setupSlider (refS, 200.0, 12000.0, 10.0, [] (double v) { return "Ref " + juce::String ((int) v) + " rpm"; });
        setupSlider (loS, 0.0, 12000.0, 10.0, [] (double v) { return "Fade-in " + juce::String ((int) v); });
        setupSlider (hiS, 1000.0, 20000.0, 10.0, [] (double v) { return v > 19990 ? juce::String ("Fade-out off") : "Fade-out " + juce::String ((int) v); });
        setupSlider (depthS, 0.0, 1.5, 0.01, [] (double v) { return "Pitch<-rpm " + juce::String ((int) (v * 100)) + "%"; });

        modeBox.onChange = [this] { P().mode = modeBox.getSelectedId() - 1; updateVisibility(); };
        linkBox.onChange = [this] { P().link = linkBox.getSelectedId() - 1; };
        loadBox.onChange = [this] { P().load = loadBox.getSelectedId() - 1; };
        eventBox.onChange = [this] { P().eventMask = 1u << (eventBox.getSelectedId() - 1); };
        muteB.onClick = [this] { P().mute = muteB.getToggleState(); };
        soloB.onClick = [this] { P().solo = soloB.getToggleState(); if (onSoloChanged) onSoloChanged(); };
        loopB.onClick = [this] { P().loop = loopB.getToggleState(); };
        normB.onClick = [this] { P().normalize = normB.getToggleState(); };
        clearB.onClick = [this] { proc.clearSlot (slot); refresh(); if (onChanged) onChanged(); };
        levelS.onValueChange = [this] { if (! binding) P().levelDb = (float) levelS.getValue(); };
        panS.onValueChange = [this] { if (! binding) P().pan = (float) panS.getValue(); };
        pitchS.onValueChange = [this] { if (! binding) P().pitchSemis = (float) pitchS.getValue(); };
        refS.onValueChange = [this] { if (! binding) P().refRpm = (float) refS.getValue(); };
        loS.onValueChange = [this] { if (! binding) P().rpmLo = (float) loS.getValue(); };
        hiS.onValueChange = [this] { if (! binding) P().rpmHi = (float) hiS.getValue(); };
        depthS.onValueChange = [this] { if (! binding) P().pitchDepth = (float) depthS.getValue(); };
    }

    std::function<void()> onChanged, onSoloChanged;

    void bind (int slotIndex) { slot = slotIndex; refresh(); }
    int  getSlot() const { return slot; }

    void refresh()
    {
        binding = true;
        auto& p = P();
        modeBox.setSelectedId (p.mode.load() + 1, juce::dontSendNotification);
        linkBox.setSelectedId (p.link.load() + 1, juce::dontSendNotification);
        loadBox.setSelectedId (p.load.load() + 1, juce::dontSendNotification);
        const uint32_t em = p.eventMask.load(); int ei = 0; while (ei < 3 && ! ((em >> ei) & 1u)) ++ei;
        eventBox.setSelectedId (ei + 1, juce::dontSendNotification);
        muteB.setToggleState (p.mute.load(), juce::dontSendNotification);
        soloB.setToggleState (p.solo.load(), juce::dontSendNotification);
        loopB.setToggleState (p.loop.load(), juce::dontSendNotification);
        normB.setToggleState (p.normalize.load(), juce::dontSendNotification);
        levelS.setValue (p.levelDb.load(), juce::dontSendNotification);   panS.setValue (p.pan.load(), juce::dontSendNotification);
        pitchS.setValue (p.pitchSemis.load(), juce::dontSendNotification); refS.setValue (p.refRpm.load(), juce::dontSendNotification);
        loS.setValue (p.rpmLo.load(), juce::dontSendNotification);        hiS.setValue (p.rpmHi.load(), juce::dontSendNotification);
        depthS.setValue (p.pitchDepth.load(), juce::dontSendNotification);
        binding = false;
        buildWave();
        updateVisibility();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f);
        g.setColour (hover ? juce::Colour (0xff2c2630) : juce::Colour (0xff1c1c22)); g.fillRoundedRectangle (b, 8.f);
        g.setColour (hover ? juce::Colour (0xffe0202a) : juce::Colour (0xff3a3a46)); g.drawRoundedRectangle (b, 8.f, hover ? 2.f : 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.85f)); g.setFont (juce::Font (13.f, juce::Font::bold));
        const auto clip = proc.sampler.slots[(size_t) slot].getClip();
        g.drawText ("[" + juce::String (slot + 1) + "]  " + (clip ? juce::String (clip->name) : juce::String ("drop a sound here")),
                    8, 6, getWidth() - 150, 18, juce::Justification::left, true);
        const auto wf = waveArea();
        g.setColour (juce::Colour (0xff101014)); g.fillRoundedRectangle (wf.toFloat(), 4.f);
        if (! peaks.empty())
        {
            juce::Path path; const float mid = wf.getCentreY(), h = wf.getHeight() * 0.45f;
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float x = (float) wf.getX() + (float) i * (float) wf.getWidth() / (float) peaks.size();
                path.addLineSegment (juce::Line<float> (x, mid - peaks[i] * h, x, mid + peaks[i] * h), 0.6f);
            }
            g.setColour (juce::Colour (0xffe0202a).withAlpha (0.85f)); g.fillPath (path);
        }
        else { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.setFont (12.f); g.drawText ("drag from the browser or from Explorer", wf, juce::Justification::centred); }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto top = r.removeFromTop (22);
        clearB.setBounds (top.removeFromRight (24)); top.removeFromRight (2);
        normB.setBounds (top.removeFromRight (40)); loopB.setBounds (top.removeFromRight (40));
        soloB.setBounds (top.removeFromRight (26)); muteB.setBounds (top.removeFromRight (26));
        r.removeFromTop (2);
        waveRect = r.removeFromTop (58);
        r.removeFromTop (3);
        auto row = r.removeFromTop (20);
        modeBox.setBounds (row.removeFromLeft (row.getWidth() / 3).reduced (1));
        linkBox.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (1));
        loadBox.setBounds (row.reduced (1)); eventBox.setBounds (row.reduced (1));
        eventBox.setBounds (linkBox.getBounds().getUnion (loadBox.getBounds()));
        r.removeFromTop (3);
        auto r1 = r.removeFromTop (19), r2 = r.removeFromTop (19);
        const int w1 = r1.getWidth() / 3;
        levelS.setBounds (r1.removeFromLeft (w1).reduced (1)); panS.setBounds (r1.removeFromLeft (w1).reduced (1)); pitchS.setBounds (r1.reduced (1));
        refS.setBounds (r2.removeFromLeft (w1).reduced (1));   loS.setBounds (r2.removeFromLeft (w1).reduced (1)); hiS.setBounds (r2.reduced (1));
        depthS.setBounds (r.removeFromTop (19).reduced (1).withWidth (w1 * 3 / 2));
    }

    // ---- external files (Explorer) ----
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    { for (auto& f : files) if (isAudioFile (juce::File (f))) return true; return false; }
    void fileDragEnter (const juce::StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { hover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        hover = false; int k = 0;
        for (auto& f : files) if (isAudioFile (juce::File (f)) && slot + k < f1nz::SamplerEngine::kSlots) { proc.loadSlotFile (slot + k, juce::File (f)); ++k; }
        if (onChanged) onChanged();
    }
    // ---- drags from the browser list ----
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().isNotEmpty(); }
    void itemDragEnter (const SourceDetails&) override { hover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { hover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        hover = false;
        proc.loadSlotFile (slot, juce::File (d.description.toString()));
        if (onChanged) onChanged();
    }

private:
    f1nz::SlotParams& P() { return proc.sampler.slots[(size_t) slot].p; }
    juce::Rectangle<int> waveArea() const { return waveRect; }

    static void styleCombo (juce::ComboBox& c)
    {
        c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2c2c34));
        c.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        c.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a4a56));
        c.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
    }
    void setupSlider (juce::Slider& s, double lo, double hi, double step, std::function<juce::String (double)> txt)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, step);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 10, 10);
        s.setTextBoxIsEditable (false);
        s.textFromValueFunction = std::move (txt);
        s.setColour (juce::Slider::trackColourId, juce::Colour (0xff7a1018));
        s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2c2c34));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff4a4a56));
        addAndMakeVisible (s);
    }
    void updateVisibility()
    {
        const int m = P().mode.load();
        const bool rpmMode = (m == f1nz::ModeRpmLoop || m == f1nz::ModeGranular);
        eventBox.setVisible (m == f1nz::ModeEvent);
        linkBox.setVisible (m != f1nz::ModeEvent); loadBox.setVisible (m != f1nz::ModeEvent);
        refS.setVisible (rpmMode); loS.setVisible (rpmMode); hiS.setVisible (rpmMode); depthS.setVisible (rpmMode);
        loopB.setVisible (m != f1nz::ModeRpmLoop && m != f1nz::ModeGranular && m != f1nz::ModeEvent);
    }
    void buildWave()
    {
        peaks.clear();
        const auto clip = proc.sampler.slots[(size_t) slot].getClip();
        if (! clip) return;
        const int cols = 300; const size_t n = clip->l.size();
        peaks.assign ((size_t) cols, 0.f);
        for (int i = 0; i < cols; ++i)
        {
            const size_t a = n * (size_t) i / (size_t) cols, b = std::max (a + 1, n * (size_t) (i + 1) / (size_t) cols);
            float pk = 0.f; for (size_t k = a; k < b && k < n; ++k) pk = std::max (pk, std::fabs (clip->l[k]));
            peaks[(size_t) i] = std::min (1.f, pk * clip->normGain);
        }
    }

    F1nzProcessor& proc;
    int slot = 0; bool binding = false, hover = false;
    juce::Rectangle<int> waveRect;
    std::vector<float> peaks;
    juce::ComboBox modeBox, linkBox, loadBox, eventBox;
    juce::TextButton muteB { "M" }, soloB { "S" }, loopB { "Loop" }, normB { "Norm" }, clearB { "x" };
    juce::Slider levelS, panS, pitchS, refS, loS, hiS, depthS;
};

// ------------------------------------------------------------------ sound browser on a custom folder
class BrowserPanel : public juce::Component, private juce::ListBoxModel
{
public:
    explicit BrowserPanel (F1nzProcessor& p) : proc (p)
    {
        chooseB.setButtonText ("Choose folder..."); rescanB.setButtonText ("Rescan");
        addAndMakeVisible (chooseB); addAndMakeVisible (rescanB);
        folderLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f)); folderLabel.setFont (juce::Font (11.f));
        addAndMakeVisible (folderLabel);
        search.setTextToShowWhenEmpty ("search tags: e.g. v8 loop low", juce::Colours::grey);
        search.onTextChange = [this] { applyFilter(); };
        addAndMakeVisible (search);
        list.setModel (this); list.setRowHeight (34); list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff15151a));
        addAndMakeVisible (list);
        chooseB.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose your sound folder", proc.getSampleFolder());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc)
                                  { auto f = fc.getResult(); if (f.isDirectory()) { proc.setSampleFolder (f); rescan(); } });
        };
        rescanB.onClick = [this] { rescan(); };
        rescan();
    }
    std::function<void (const juce::File&)> onLoadRequested;       // double-click

    void rescan()
    {
        const auto folder = proc.getSampleFolder();
        folderLabel.setText (folder.getFullPathName(), juce::dontSendNotification);
        all.clear();
        if (folder.isDirectory()) folder.findChildFiles (all, juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        applyFilter();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        auto row = r.removeFromTop (24); chooseB.setBounds (row.removeFromLeft (row.getWidth() - 62)); rescanB.setBounds (row.reduced (2, 0).withTrimmedLeft (2));
        folderLabel.setBounds (r.removeFromTop (16)); search.setBounds (r.removeFromTop (24)); r.removeFromTop (4); list.setBounds (r);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff1c1c22)); g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.f), 8.f);
    }

private:
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) shown.size()) return;
        g.fillAll (selected ? juce::Colour (0xff5a1218) : (row % 2 ? juce::Colour (0xff1a1a20) : juce::Colour (0xff17171c)));
        const auto& f = shown[(size_t) row];
        g.setColour (juce::Colours::white); g.setFont (13.f); g.drawText (f.getFileNameWithoutExtension(), 6, 2, w - 12, 16, juce::Justification::left, true);
        g.setColour (juce::Colours::white.withAlpha (0.45f)); g.setFont (10.5f);
        g.drawText (f.getRelativePathFrom (proc.getSampleFolder()).replaceCharacter ('\\', '/'), 6, 17, w - 12, h - 18, juce::Justification::left, true);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    { if (row >= 0 && row < (int) shown.size()) proc.previewFile (shown[(size_t) row]); }          // audition
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    { if (row >= 0 && row < (int) shown.size() && onLoadRequested) onLoadRequested (shown[(size_t) row]); }
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    { if (rows.size() > 0 && rows[0] < (int) shown.size()) return shown[(size_t) rows[0]].getFullPathName(); return {}; }

    void applyFilter()
    {
        shown.clear();
        const auto tokens = juce::StringArray::fromTokens (search.getText().toLowerCase(), " ", "");
        for (auto& f : all)
        {
            const auto hay = f.getRelativePathFrom (proc.getSampleFolder()).toLowerCase();       // folder names double as tags
            bool ok = true; for (auto& t : tokens) if (t.isNotEmpty() && ! hay.contains (t)) ok = false;
            if (ok) shown.push_back (f);
        }
        std::sort (shown.begin(), shown.end(), [] (const juce::File& a, const juce::File& b) { return a.getFullPathName().compareIgnoreCase (b.getFullPathName()) < 0; });
        list.updateContent(); list.repaint();
    }

    F1nzProcessor& proc;
    juce::TextButton chooseB, rescanB; juce::Label folderLabel; juce::TextEditor search; juce::ListBox list;
    juce::Array<juce::File> all; std::vector<juce::File> shown;
    std::unique_ptr<juce::FileChooser> chooser;
};

// ------------------------------------------------------------------ XY perform pad (x = rpm, y = throttle)
class XYPad : public juce::Component
{
public:
    std::function<void (float x01, float y01)> onMove;
    float x = 0.f, y = 0.f;                       // shown position
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f);
        g.setColour (juce::Colour (0xff101014)); g.fillRoundedRectangle (b, 8.f);
        g.setColour (juce::Colour (0xff2c2c36));
        for (int i = 1; i < 4; ++i) { g.drawLine (b.getX() + b.getWidth() * i / 4, b.getY(), b.getX() + b.getWidth() * i / 4, b.getBottom()); g.drawLine (b.getX(), b.getY() + b.getHeight() * i / 4, b.getRight(), b.getY() + b.getHeight() * i / 4); }
        const float px = b.getX() + x * b.getWidth(), py = b.getBottom() - y * b.getHeight();
        g.setColour (juce::Colour (0xffe0202a).withAlpha (0.5f)); g.drawLine (px, b.getY(), px, b.getBottom()); g.drawLine (b.getX(), py, b.getRight(), py);
        g.setColour (juce::Colour (0xffe0202a)); g.fillEllipse (px - 7, py - 7, 14, 14);
        g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (11.f);
        g.drawText ("RPM  >", b.toNearestInt().removeFromBottom (14), juce::Justification::centredRight);
        g.drawText ("THROTTLE", b.toNearestInt().removeFromTop (14), juce::Justification::centredLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override { move (e); }
    void mouseDrag (const juce::MouseEvent& e) override { move (e); }
private:
    void move (const juce::MouseEvent& e)
    {
        if (onMove) onMove (juce::jlimit (0.f, 1.f, e.position.x / (float) getWidth()), juce::jlimit (0.f, 1.f, 1.f - e.position.y / (float) getHeight()));
    }
};
} // namespace f1nz::ui
