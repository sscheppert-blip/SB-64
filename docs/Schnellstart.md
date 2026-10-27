# SB-64 – Schnellstart

Ausführlich: [Handbuch](Handbuch.md).

SB-64 ist ein monophoner Acid-Bass-Synthesizer mit eingebautem 64-Step-Sequencer: ein Oszillator, Sub,
Rauschen, ein rotziges Zwei-Pol-Filter – alles auf einer Frontplatte, ohne Untermenüs zum Durchklicken.

![SB-64](images/panel.png)

---

## Die erste Acid-Line

1. SB-64 auf eine **Instrument-/MIDI-Spur** laden. Er startet wie die Hardware mit leeren Patterns.
2. Ein paar **Step-Tasten** klicken: Jeder Step geht an und blinkt. **BACK/NOTE** dreht seinen Ton,
   **SELECT/GATE** seine Länge. Noch ein Klick schaltet ihn aus.
   Schneller: **SHIFT** und dann **RND PATT** würfelt ein ganzes Pattern.
3. **▷ PLAY** drücken. Im Plugin folgt SB-64 der DAW: PLAY blinkt, bis die DAW läuft.
4. Am Acid-Kern drehen: **CUTOFF**, **RESONANCE**, **VCF ENV**, **ENV/CV LVL**.
5. **DRIVE** langsam aufziehen, dann **DETUNE**.

Werks-Sounds: **SELECT/GATE** drehen bis `PRS`, mit **BACK/NOTE** einen der 16 Sounds wählen.

## Die Frontplatte

| Regler | Was er tut |
|---|---|
| **DRIVE** | Überblendung von sauber zu Übersteuerung |
| **DETUNE** | Schwarm aus zwei modulierten Delay-Lines |
| **VCA ENV** | Ausklingzeit; ganz rechts **INF** = Drone |
| **VCF ENV** | Ausklingzeit der Filterhüllkurve |
| **RESONANCE / CUTOFF** | Filter, bis zur Selbstoszillation |
| **Wellen-Schalter** / **Filter-Schalter** | Rechteck/Säge/Dreieck · BP/LP |
| **FM LVL** | LFO auf die Tonhöhe (bis in den Audiobereich) |
| **SUB LVL / NOISE LVL** | Sub-Oszillator eine Oktave tiefer, Rauschen |
| **LFO RATE / LFO LVL** | LFO-Tempo und Stärke aufs Filter |
| **ENV/CV LVL** | Wie weit die Hüllkurve das Filter öffnet |

**Knöpfe:** ziehen, **Shift** = fein, **Doppelklick** = Grundwert. **SHIFT** auf der Frontplatte rastet
ein: einmal drücken, dann eine Step-Taste.

## Steps bearbeiten

| Aktion | So geht's |
|---|---|
| Ton | **BACK/NOTE** drehen, **±1 OCT** springt Oktaven |
| Länge | **SELECT/GATE** drehen (10–100 %; 100 % = legato) |
| Wahrscheinlichkeit / Feinstimmung | **SELECT/GATE** klicken: `GAT` → `PRB` → `CNT` |
| Accent / Slide | **SHIFT + ACC** (Taste 1) bzw. **SHIFT + SLIDE** (Taste 2), dann Steps antippen; **BACK** beendet |
| Takt 2–4 | **◂ BAR ▸** |
| Live transponieren | **SHIFT + TRANSP**, dann Taste 1–13 (Taste 16 = zurück) |

## Menü

Alles ohne eigenen Knopf liegt im Menü: **SELECT/GATE** drehen (ohne gewählten Step) blättert, **BACK/NOTE**
ändert den Wert. Wichtig: `PRS` Werks-Sounds, `BPM` Tempo, `CLK` Uhr (Host/intern), `SWG` Swing,
`SCL` Skala, `ARP` Arpeggiator.

## Ausprobieren

- **VCA ENV** ganz auf (**INF**) und am Filter schrauben – Drone.
- **LFO RATE** hoch, **FM LVL** auf – metallisch-kaputte Töne.
- **SHIFT + RND PATT** mehrmals drücken, bis eine Line hängen bleibt.
