#pragma once

// Told about the notes the sequencer and the arpeggiator play (for MIDI out). JUCE-free.
namespace sb64::seq
{
    struct NoteListener
    {
        virtual ~NoteListener() = default;
        // a new note; a note still sounding ends right after (so slides and ties overlap)
        virtual void noteStarted (int note, bool accent) noexcept = 0;
        virtual void noteEnded() noexcept = 0;
    };
}
