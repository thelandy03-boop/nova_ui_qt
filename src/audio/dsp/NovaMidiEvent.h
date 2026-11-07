#pragma once

#include <cstdint>
#include <vector>

/**
 * Representa un evento o mensaje MIDI en tiempo real en NOVA Studio.
 * Diseñado para compatibilidad directa con protocolos MIDI 1.0, streams de
 * bloques DSP (desplazamiento temporal en fotogramas dentro del bloque) y buses.
 */
struct NovaMidiEvent {
    uint32_t bus = 0;          // Bus MIDI (0 por defecto)
    uint32_t offset = 0;       // Fotograma/muestra dentro del bloque actual
    std::vector<uint8_t> data; // Bytes raw del mensaje (ej: 0x90, nota, vel)

    NovaMidiEvent() = default;

    NovaMidiEvent(uint32_t off, const std::vector<uint8_t> &d, uint32_t b = 0)
        : bus(b), offset(off), data(d) {}

    NovaMidiEvent(uint32_t off, const uint8_t *raw, uint32_t size, uint32_t b = 0)
        : bus(b), offset(off), data(raw, raw + size) {}

    NovaMidiEvent(uint32_t off, uint8_t b0, uint8_t b1, uint8_t b2, uint32_t b = 0)
        : bus(b), offset(off), data{b0, b1, b2} {}

    NovaMidiEvent(uint32_t off, uint8_t b0, uint8_t b1, uint32_t b = 0)
        : bus(b), offset(off), data{b0, b1} {}

    // Creadores de mensajes MIDI estándar
    static NovaMidiEvent noteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t offset = 0, uint32_t bus = 0)
    {
        return NovaMidiEvent(offset, static_cast<uint8_t>(0x90 | (channel & 0x0F)), static_cast<uint8_t>(note & 0x7F), static_cast<uint8_t>(velocity & 0x7F), bus);
    }

    static NovaMidiEvent noteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0, uint32_t offset = 0, uint32_t bus = 0)
    {
        return NovaMidiEvent(offset, static_cast<uint8_t>(0x80 | (channel & 0x0F)), static_cast<uint8_t>(note & 0x7F), static_cast<uint8_t>(velocity & 0x7F), bus);
    }

    static NovaMidiEvent controlChange(uint8_t channel, uint8_t controller, uint8_t value, uint32_t offset = 0, uint32_t bus = 0)
    {
        return NovaMidiEvent(offset, static_cast<uint8_t>(0xB0 | (channel & 0x0F)), static_cast<uint8_t>(controller & 0x7F), static_cast<uint8_t>(value & 0x7F), bus);
    }

    static NovaMidiEvent pitchBend(uint8_t channel, uint16_t value, uint32_t offset = 0, uint32_t bus = 0)
    {
        uint8_t lsb = static_cast<uint8_t>(value & 0x7F);
        uint8_t msb = static_cast<uint8_t>((value >> 7) & 0x7F);
        return NovaMidiEvent(offset, static_cast<uint8_t>(0xE0 | (channel & 0x0F)), lsb, msb, bus);
    }
};
