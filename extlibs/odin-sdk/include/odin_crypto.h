/* Copyright (c) 4Players GmbH. All rights reserved. */

#pragma once

/** @file odin_crypto.h */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "odin.h"

#define ODIN_CRYPTO_VERSION "2.0.2"

/**
 * Represents the encryption status of a remote peer in an ODIN room.
 */
enum OdinCryptoPeerStatus
#ifdef __cplusplus
  : int32_t
#endif // __cplusplus
 {
    /**
     * Password does not match the local password, preventing decryption of their data.
     */
    ODIN_CRYPTO_PEER_STATUS_PASSWORD_MISSMATCH = -1,
    /**
     * Encryption status has not yet been determined.
     */
    ODIN_CRYPTO_PEER_STATUS_UNKNOWN = 0,
    /**
     * Remote peer is not using encryption; their data is transmitted in plaintext.
     */
    ODIN_CRYPTO_PEER_STATUS_UNENCRYPTED = 1,
    /**
     * The peer is using encryption and their data can be successfully decrypted.
     */
    ODIN_CRYPTO_PEER_STATUS_ENCRYPTED = 2,
};
#ifndef __cplusplus
typedef int32_t OdinCryptoPeerStatus;
#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Creates a new ODIN cipher instance for end-to-end encryption of room communications. The
 * cipher uses AES-256-GCM for authenticated encryption with PBKDF2-SHA256 for key derivation,
 * providing strong cryptographic protection for datagrams, messages and peer user data.
 *
 * The returned cipher can be attached to an ODIN room during creation via `odin_room_create_ex`
 * to enable transparent encryption and decryption of all room communications. The cipher will
 * automatically manage per-peer encryption keys and handle key rotation.
 *
 * Note: Use `ODIN_CRYPTO_VERSION` to supply the `version` argument.
 */
OdinCipher *odin_crypto_create(const char *version);

/**
 * Retrieves the encryption status of a remote peer in the room. This function examines the peer's
 * user data to determine whether they are using encryption and whether their password matches the
 * local cipher's password.
 */
OdinCryptoPeerStatus odin_crypto_get_peer_status(OdinCipher *cipher, uint32_t peer_id);

/**
 * Sets or clears the encryption password for the specified ODIN cipher. The password is used to
 * derive a master key via PBKDF2-SHA256 with 600,000 iterations, which is then used to generate
 * per-peer encryption keys. All peers in a room must use the same master password to communicate
 * securely.
 *
 * When called before the cipher is attached to a room, the password is stored and applied once
 * the room is joined. When called after joining a room, the cipher immediately re-encrypts the
 * local peer's user data with the new key and broadcasts it to other peers.
 *
 * To disable encryption, pass `NULL` for the password parameter. This will clear any existing
 * master key and cause subsequent communications to be sent unencrypted.
 */
int32_t odin_crypto_set_password(OdinCipher *cipher,
                                 const uint8_t *password,
                                 uint32_t password_length);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
