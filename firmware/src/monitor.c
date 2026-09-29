// MIT License
//
// Copyright (c) 2026 Kevin Thomas
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Author:  Kevin Thomas
// Email:   kevin@mytechnotalent.com
// GitHub:  https://github.com/mytechnotalent/picokit-27-lora-store-forward
// File:    monitor.c
// Desc:    Implements the LoRa store and forward state machine that buffers
//          readings while the link is down and flushes them on recovery.
// Created: 2026

#include "picokit_27_lora_store_forward.h"
#include "monitor.h"
#include "radio.h"
#include "status_led.h"
#include "sensor.h"
#include "ccm.h"
#include "envelope.h"
#include "field_secrets.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Module-ready flag.
 *
 * Set to true by monitor_init() once the peripherals are configured.
 * monitor_step() returns false while this flag is clear.
 */
static bool g_ready;

/**
 * @brief True while the downlink is considered up.
 */
static bool g_link_up;

/**
 * @brief Number of readings held in the store and forward backlog.
 */
static uint16_t g_backlog;

/**
 * @brief Monotonic transmit sequence number.
 */
static uint16_t g_seq;

/**
 * @brief Absolute time in microseconds of the next DHT11 sample.
 */
static uint64_t g_next_read_us;

/**
 * @brief Absolute time in microseconds of the next authenticated transmit.
 */
static uint64_t g_next_tx_us;

/**
 * @brief Most recent decoded DHT11 reading.
 */
static dht_reading_t g_reading;

/**
 * @brief Inbound radio line accumulator.
 */
static char g_rx_line[RADIO_LINE_BUF_LEN];

/**
 * @brief Number of bytes currently held in the inbound line accumulator.
 */
static size_t g_rx_len;

/**
 * @brief AES-128 session key for telemetry.
 */
static uint8_t g_key[CCM_KEY_LEN];

/**
 * @brief True once the telemetry session key has been loaded.
 */
static bool g_key_ready;

/**
 * @brief Configure the onboard heartbeat LED as a dark output.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_state_init_io(void) {
    gpio_init(PICOKIT_27_LORA_STORE_FORWARD_LED_PIN);
    gpio_set_dir(PICOKIT_27_LORA_STORE_FORWARD_LED_PIN, GPIO_OUT);
    gpio_put(PICOKIT_27_LORA_STORE_FORWARD_LED_PIN, 0);
}

/**
 * @brief Reset the link, backlog, sequence, and transmit timing.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_state_init(void) {
    uint64_t now_us = time_us_64();
    memset(&g_reading, 0, sizeof(g_reading));
    g_link_up = true;
    g_backlog = 0u;
    g_seq = 0u;
    g_next_read_us = now_us;
    g_next_tx_us = now_us + (uint64_t)PICOKIT_27_LORA_STORE_FORWARD_TX_INTERVAL_MS * 1000u;
    g_ready = true;
}

/**
 * @brief Load the telemetry session key from the field secret.
 *
 * LAB-ONLY: production must provision the session key through OTP rather
 * than embedding a committed key.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_load_key(void) {
    static const uint8_t key[CCM_KEY_LEN] = FIELD_SECRET_KEY;
    memcpy(g_key, key, CCM_KEY_LEN);
    g_key_ready = true;
}

/**
 * @brief Print the boot banner for the store and forward lesson.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_banner(void) {
    printf("=== PICOKIT-27 LORA STORE FORWARD // BUFFER + FLUSH + AUTHENTICATED HEARTBEAT ===\n");
}

/**
 * @brief Derive the field key and announce a ready monitor.
 *
 * @param void No parameters.
 * @return bool true when the field key was derived and installed.
 */
static bool monitor_finish(void) {
    monitor_load_key();
    monitor_banner();
    return true;
}

/**
 * @brief Blink the onboard heartbeat LED exactly once.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_heartbeat(void) {
    gpio_put(PICOKIT_27_LORA_STORE_FORWARD_LED_PIN, 1);
    sleep_us(MONITOR_HEARTBEAT_BLINK_US);
    gpio_put(PICOKIT_27_LORA_STORE_FORWARD_LED_PIN, 0);
    sleep_us(MONITOR_HEARTBEAT_BLINK_US);
}

/**
 * @brief Print the most recent temperature and humidity reading.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_log_reading(void) {
    printf("DHT t=%d h=%u\n", (int)g_reading.temperature_tenths, (unsigned)g_reading.humidity_tenths);
}

/**
 * @brief Print a DHT11 read failure with its result code.
 *
 * @param rc Sensor result code returned by the one-wire state machine.
 * @return void
 */
static void monitor_log_error(sensor_result_t rc) {
    printf("READ ERR %d\n", (int)rc);
}

/**
 * @brief Sample the DHT11, drive the LEDs, and schedule the next read.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_read_tick(uint64_t now_us) {
    sensor_result_t rc = sensor_read(&g_reading);
    bool ok = (rc == SENSOR_RESULT_OK);
    if (ok) {
        monitor_log_reading();
    } else {
        monitor_log_error(rc);
    }
    status_led_show_step(ok ? STATUS_LED_STEP_GREEN : STATUS_LED_STEP_RED);
    g_next_read_us = now_us + (uint64_t)MONITOR_READ_INTERVAL_MS * 1000u;
}

/**
 * @brief Format the heartbeat JSON body for the current backlog depth.
 *
 * @param frame Pointer to the mutable frame output buffer.
 * @param frame_len Capacity of the frame output buffer in bytes.
 * @return size_t Number of JSON bytes written, or zero on overflow.
 */
static size_t monitor_build_frame(char *frame, size_t frame_len) {
    int written = snprintf(frame, frame_len, "{\"n\":%u,\"s\":%u,\"b\":%u}", (unsigned)PACKET_NODE_ID, (unsigned)g_seq, (unsigned)g_backlog);
    return (written > 0 && (size_t)written < frame_len) ? (size_t)written : 0u;
}

/**
 * @brief Seal the current heartbeat body into a hex envelope.
 *
 * @param hex Pointer to the NUL-terminated hex output buffer.
 * @param hex_len Capacity of the hex output buffer in bytes.
 * @return bool true when the heartbeat was sealed and encoded.
 */
static bool monitor_seal_frame(char *hex, size_t hex_len) {
    char frame[PICOKIT_27_LORA_STORE_FORWARD_FRAME_SIZE];
    uint8_t nonce[ENVELOPE_NONCE_LEN];
    uint8_t ad = (uint8_t)PACKET_NODE_ID;
    size_t frame_len = monitor_build_frame(frame, sizeof(frame));
    envelope_fill_nonce(nonce);
    return envelope_seal_hex(g_key, nonce, &ad, 1u, (const uint8_t *)frame, frame_len, hex, hex_len);
}

/**
 * @brief Build and transmit the authenticated heartbeat frame.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_transmit(void) {
    char hex[ENVELOPE_MAX_HEX_LEN];
    if (!g_key_ready) {
        return;
    }
    if (monitor_seal_frame(hex, sizeof(hex))) {
        radio_send_frame(PICOKIT_27_LORA_STORE_FORWARD_UART, (const uint8_t *)hex, strlen(hex));
        g_seq += 1u;
    }
}

/**
 * @brief Parse a link state token from a downlink payload.
 *
 * @param payload Pointer to the NUL-terminated inbound payload.
 * @param up Pointer to mutable decoded link state.
 * @return bool true when the payload carried a link state.
 */
static bool monitor_parse_link(const char *payload, uint8_t *up) {
    if (strncmp(payload, "LINK ", 5u) != 0) {
        return false;
    }
    *up = (payload[5] == '1') ? 1u : 0u;
    return true;
}

/**
 * @brief Transmit every buffered reading and drain the backlog.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_flush_backlog(void) {
    while (g_backlog > 0u) {
        g_backlog -= 1u;
        monitor_transmit();
    }
    printf("FLUSH b=%u\n", (unsigned)g_backlog);
}

/**
 * @brief Apply one downlink link state command and flush on recovery.
 *
 * @param payload Pointer to the NUL-terminated inbound payload.
 * @return void
 */
static void monitor_handle_link(const char *payload) {
    uint8_t up;
    if (!monitor_parse_link(payload, &up)) {
        return;
    }
    g_link_up = (up != 0u);
    printf("LINK %s\n", g_link_up ? "UP" : "DOWN");
    if (g_link_up) {
        monitor_flush_backlog();
    }
}

/**
 * @brief Buffer one reading while the link is down.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_store(void) {
    if (g_backlog < MONITOR_BACKLOG_MAX) {
        g_backlog += 1u;
    }
    printf("STORE b=%u\n", (unsigned)g_backlog);
}

/**
 * @brief Transmit one heartbeat and schedule the next transmit.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_tx_tick(uint64_t now_us) {
    monitor_heartbeat();
    if (g_link_up) {
        monitor_transmit();
    } else {
        monitor_store();
    }
    g_next_tx_us = now_us + (uint64_t)PICOKIT_27_LORA_STORE_FORWARD_TX_INTERVAL_MS * 1000u;
}

/**
 * @brief Drain inbound radio lines and apply every link state command.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_rx_tick(void) {
    radio_rcv_t rcv;
    while (radio_line_pump(PICOKIT_27_LORA_STORE_FORWARD_UART, g_rx_line, &g_rx_len)) {
        if (radio_parse_rcv(g_rx_line, &rcv) == RADIO_RESULT_OK) {
            printf("RX from 0x%04X, %u bytes\n", (unsigned)rcv.sender, (unsigned)rcv.len);
            monitor_handle_link(rcv.payload);
        }
    }
}

/**
 * @brief Service the DHT11 sample and heartbeat transmit timers.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_service_timers(uint64_t now_us) {
    if (now_us >= g_next_read_us) {
        monitor_read_tick(now_us);
    }
    if (now_us >= g_next_tx_us) {
        monitor_tx_tick(now_us);
    }
}

bool monitor_init(void) {
    bool ok;
    ok = status_led_init() && radio_init(PICOKIT_27_LORA_STORE_FORWARD_UART) && sensor_init();
    monitor_state_init_io();
    monitor_state_init();
    return ok && monitor_finish();
}

void monitor_deinit(void) {
    g_ready = false;
}

bool monitor_step(void) {
    uint64_t now_us;
    if (!g_ready) {
        return false;
    }
    now_us = time_us_64();
    monitor_service_timers(now_us);
    monitor_rx_tick();
    return true;
}
