#ifndef MIDI_TRANSPORT_H
#define MIDI_TRANSPORT_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include "config.h"
#include "midi_types.h"
#include "midi_parser.h"
#include "jitter_buffer.h"

// ============================================================================
// PlayMode — MIDI Transport Layer (Phase 3)
// ============================================================================
//
// Manages the three MIDI input sources:
//   - Serial MIDI (31250 baud via Serial2)
//   - UDP MIDI (raw bytes over WiFi)
//   - RTP-MIDI (AppleMIDI via lathoub/Arduino-AppleMIDI-Library)
//
// Each transport has its own MIDI parser.
// Parsed messages are inserted into the JitterBuffer.
//
// A complete SysEx frame is handed to the registered handler together with the
// transport it came from, so a reply goes back the way the request arrived.
// Automatic General-Midi-Boop discovery is only possible where that return path
// exists (docs/SYSEX_IDENTITY.md §8): DIN needs a MIDI OUT pin, UDP answers the
// datagram's sender, RTP-MIDI answers the session.
//

class MidiTransport {
public:
    MidiTransport(JitterBuffer& jitterBuffer);

    // Initializes the configured transports.
    // Must be called after WiFi connection (for network transports).
    bool begin(const MidiInputConfig& config);

    // Reads data from all active transports.
    // Call on each iteration of loop().
    void poll();

    // Stops all transports
    void stop();

    // Stats
    uint32_t getSerialByteCount() const;
    uint32_t getUdpPacketCount() const;
    uint32_t getRtpPacketCount() const;

    // Delivers a parsed message to the jitter buffer (public for AppleMIDI callbacks)
    void deliverMessage(MidiMessage& msg, MidiTransportSource source);

    // --- SysEx (control plane) ---

    // Invoked with one COMPLETE frame (F0 ... F7) and the transport it arrived
    // on. Runs on the loop task, never in a real-time context.
    typedef void (*SysExHandler)(const uint8_t* frame, uint16_t len,
                                 MidiTransportSource source, void* ctx);
    void setSysExHandler(SysExHandler handler, void* ctx);

    // Public for the AppleMIDI SysEx callback.
    void deliverSysEx(const uint8_t* frame, uint16_t len, MidiTransportSource source);

    // True when a SysEx reply can actually leave on that transport.
    bool canSendSysEx(MidiTransportSource source) const;
    // True when at least one transport can answer — i.e. automatic discovery
    // and push notifications are possible at all.
    bool anyBidirectional() const;

    // Send one complete SysEx frame (F0 ... F7 included).
    bool sendSysEx(MidiTransportSource source, const uint8_t* data, uint16_t len);
    // Fan a notification out to every transport that can carry it.
    uint8_t broadcastSysEx(const uint8_t* data, uint16_t len);

    uint32_t getSysExInCount() const  { return _sysexIn; }
    uint32_t getSysExOutCount() const { return _sysexOut; }

    // AUDIT FIX: handler invoked when an RTP-MIDI session disconnects, so held
    // notes from that source can be released (set to MidiDispatcher::allNotesOff
    // via a thin wrapper in setup).
    void setDisconnectHandler(void (*handler)());
    void notifyDisconnect();

private:
    JitterBuffer& _jitterBuffer;
    MidiInputConfig _config;

    // Serial MIDI
    MidiParser _serialParser;
    bool _serialActive;

    // UDP MIDI
    WiFiUDP _udp;
    MidiParser _udpParser;
    bool _udpActive;

    // RTP-MIDI (AppleMIDI)
    bool _rtpActive;
    void (*_disconnectHandler)();

    // SysEx plumbing
    SysExHandler _sysexHandler;
    void*        _sysexCtx;
    bool         _serialTxActive;      // a MIDI OUT pin is configured
    IPAddress    _udpPeerIP;           // last UDP sender (reply / notification)
    uint16_t     _udpPeerPort;
    bool         _udpPeerValid;
    uint32_t     _sysexIn;
    uint32_t     _sysexOut;

    // Drain a parser's pending SysEx frame, if any.
    void drainSysEx(MidiParser& parser, MidiTransportSource source);

    // Stats
    uint32_t _serialBytes;
    uint32_t _udpPackets;
    uint32_t _rtpPackets;

    // Per-transport individual poll
    void pollSerial();
    void pollUDP();
    void pollRTP();

    // Per-transport initialization
    bool initSerial();
    bool initUDP();
    bool initRTP();
};

#endif // MIDI_TRANSPORT_H
