/**
 * espnow_control.cpp — Decoupled ESP-NOW receive handler for main ESP32
 *
 * Architecture:
 *   ESP-NOW Callback (ISR context)
 *       ↓
 *   Validate sender MAC & packet length
 *       ↓
 *   Copy into fixed-size queue (zero dynamic allocation)
 *       ↓
 *   Return immediately
 *
 *   loop() → espnowUpdate()
 *       ↓
 *   Validate magic / version / sequence
 *       ↓
 *   Heartbeat → controlHeartbeat()
 *   Movement/Stop → controlSubmitCommand()
 */

#include "espnow_control.h"
#include "config.h"

#ifdef ENABLE_ESPNOW
#include "command.h"
#include "control_manager.h"
#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>

// ============================================================
// Fixed-size Queue for Incoming Packets
// ============================================================
constexpr size_t QUEUE_CAPACITY = 8;

struct QueuedPacket {
    RobotPacket packet;
    uint8_t     senderMac[6];
};

static QueuedPacket   _queue[QUEUE_CAPACITY];
static volatile uint8_t _qHead = 0;
static volatile uint8_t _qTail = 0;

// Sequence tracking for ESP-NOW commands
static uint32_t _lastSequence   = 0;
static bool     _hasSequence    = false;

// ============================================================
// Lightweight ESP-NOW Receive Callback (ISR Context)
// ============================================================
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const uint8_t *senderMac = info ? info->src_addr : nullptr;
#else
static void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    const uint8_t *senderMac = mac;
#endif
    if (!senderMac || !data) return;

    // 1. Basic length check
    if (len != sizeof(RobotPacket)) return;

    // 2. Validate sender MAC against configured peer
    if (STRICT_SENDER_MAC_CHECK && isCamMacConfigured()) {
        if (memcmp(senderMac, ESP32_CAM_MAC, 6) != 0) {
            return; // Reject packets from untrusted senders
        }
    }

    // 3. Enqueue into safe ring buffer (drop if full, no heap allocation)
    uint8_t nextHead = (_qHead + 1) % QUEUE_CAPACITY;
    if (nextHead != _qTail) {
        memcpy(&_queue[_qHead].packet, data, sizeof(RobotPacket));
        memcpy(_queue[_qHead].senderMac, senderMac, 6);
        __sync_synchronize();  // payload visible before head advances (other core reads it)
        _qHead = nextHead;
    }
}

// ============================================================
// Initialization
// ============================================================
void espnowInit() {
    _qHead = 0;
    _qTail = 0;
    _hasSequence = false;

    if (!isCamMacConfigured()) {
        Serial.println(F("************************************************************"));
        Serial.println(F("[ESPNOW] WARNING: ESP32_CAM_MAC is not configured in config.h!"));
        Serial.println(F("[ESPNOW] Operating in DEVELOPMENT MODE (accepting any sender)"));
        Serial.println(F("************************************************************"));
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println(F("[ESPNOW] ERROR: Init failed"));
        return;
    }

    esp_now_register_recv_cb(onDataRecv);

    // Register peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, ESP32_CAM_MAC, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println(F("[ESPNOW] Info: Added peer (or using broadcast)"));
    }

    Serial.println(F("[ESPNOW] Initialized with decoupled receive queue"));
}

// ============================================================
// Loop Processing: Parse, Validate Sequence & Dispatch
// ============================================================
void espnowUpdate() {
    while (_qTail != _qHead) {
        __sync_synchronize();  // pair with producer barrier: read head before payload
        QueuedPacket qp = _queue[_qTail];
        __sync_synchronize();  // finish copying before releasing the slot
        _qTail = (_qTail + 1) % QUEUE_CAPACITY;

        const RobotPacket& pkt = qp.packet;

        // 1. Validate magic and version
        if (pkt.magic != ESPNOW_MAGIC) {
            Serial.println(F("[ESPNOW] Rejected: invalid magic"));
            continue;
        }
        if (pkt.version != ESPNOW_VERSION) {
            Serial.println(F("[ESPNOW] Rejected: version mismatch"));
            continue;
        }

        // 2. Reject out-of-range command bytes before casting to the enum
        if (pkt.command > CMD_CLEAR_EMERGENCY) {
            Serial.print(F("[ESPNOW] Rejected: unknown command "));
            Serial.println(pkt.command);
            continue;
        }

        // 3. Separate heartbeat handling from command sequence
        if (pkt.command == CMD_HEARTBEAT) {
            controlHeartbeat(SRC_ESP32_CAM_ESPNOW);
            continue;
        }

        // 4. Sequence validation for commands (allow 32-bit unsigned wraparound)
        if (_hasSequence) {
            int32_t diff = (int32_t)(pkt.sequence - _lastSequence);
            if (diff <= 0) {
                Serial.print(F("[ESPNOW] Dropped stale/duplicate packet seq="));
                Serial.print(pkt.sequence);
                Serial.print(F(" last="));
                Serial.println(_lastSequence);
                continue;
            }
        }
        _lastSequence = pkt.sequence;
        _hasSequence  = true;

        // 5. Construct normalized RobotCommand and submit
        RobotCommand cmd;
        cmd.source    = SRC_ESP32_CAM_ESPNOW;
        cmd.action    = (CommandAction)pkt.command;
        cmd.speed     = pkt.speed;
        cmd.sequence  = pkt.sequence;
        cmd.timestamp = millis();

        controlSubmitCommand(cmd);
    }
}

#endif // ENABLE_ESPNOW
