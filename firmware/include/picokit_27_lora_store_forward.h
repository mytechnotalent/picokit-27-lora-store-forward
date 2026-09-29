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
// File:    picokit_27_lora_store_forward.h
// Desc:    Declares the platform pin mapping, radio handle, DHT11 data
//          pin, and heartbeat boundaries for the Picokit LoRa node.
// Created: 2026

#ifndef PICOKIT_27_LORA_STORE_FORWARD_H
#define PICOKIT_27_LORA_STORE_FORWARD_H

#include "hardware/uart.h"
#include "packet_artifact.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Onboard heartbeat LED GPIO pin number.
 *
 * The RP2350 Pico 2 onboard LED is connected to GPIO 25. It blinks once per
 * pattern step as a visible sign of life.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_LED_PIN 25u

/**
 * @brief UART peripheral used by the RYLR998 transceiver.
 *
 * The RYLR998 connects to UART1 through the Pico 2 header. All AT command
 * traffic flows over this byte stream.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_UART uart1

/**
 * @brief UART TX GPIO pin number to the RYLR998 RX input.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_UART_TX 8u

/**
 * @brief UART RX GPIO pin number from the RYLR998 TX output.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_UART_RX 9u

/**
 * @brief UART baud rate negotiated with the RYLR998.
 *
 * The RYLR998 ships with a 115200 baud default and must be matched on both
 * the edge device and the instructor gateway.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_UART_BAUD 115200u

/**
 * @brief RYLR998 network identifier shared by all classroom radios.
 *
 * Every edge device and the instructor hub must program the same network
 * identifier or no frames are delivered over the air.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_NETWORK_ID 18u

/**
 * @brief Fixed telemetry frame size in bytes.
 *
 * The JSON payload produced by the edge device is sealed into a hex envelope
 * before transmission over LoRa.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_FRAME_SIZE PACKET_FRAME_SIZE

/**
 * @brief Minimum spacing between consecutive LoRa transmissions.
 *
 * Deliberately larger than the radio on-air time so the instructor hub sees
 * one authenticated heartbeat per edge device per slot.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_TX_INTERVAL_MS PACKET_TX_INTERVAL_MS

/**
 * @brief Red chase LED GPIO pin number.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_RED_LED_PIN 16u

/**
 * @brief Yellow chase LED GPIO pin number.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_YELLOW_LED_PIN 18u

/**
 * @brief Green chase LED GPIO pin number.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_GREEN_LED_PIN 17u


/**
 *  DHT11 one-wire data GPIO pin number.
 *
 * The DHT11 single data line is driven low for the host-start pulse and
 * then read for the 40-bit response frame.
 */
#define PICOKIT_27_LORA_STORE_FORWARD_DHT_PIN 4u
#endif // PICOKIT_27_LORA_STORE_FORWARD_H
