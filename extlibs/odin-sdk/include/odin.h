/* Copyright (c) 4Players GmbH. All rights reserved. */

#pragma once

/** @file odin.h */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define ODIN_VERSION "2.1.4"

/**
 * Defines standard error codes returned by ODIN functions. Non-negative values indicate success
 * or non-error states while negative values indicate errors.
 */
typedef enum OdinError {
    /**
     * Operation completed successfully.
     */
    ODIN_ERROR_SUCCESS = 0,
    /**
     * No data available.
     */
    ODIN_ERROR_NO_DATA = 1,
    /**
     * The runtime initialization failed.
     */
    ODIN_ERROR_INITIALIZATION_FAILED = -1,
    /**
     * The specified API version is not supported.
     */
    ODIN_ERROR_UNSUPPORTED_VERSION = -2,
    /**
     * The object is in an unexpected state.
     */
    ODIN_ERROR_UNEXPECTED_STATE = -3,
    /**
     * The object is closed.
     */
    ODIN_ERROR_CLOSED = -4,
    /**
     * A mandatory argument is null.
     */
    ODIN_ERROR_ARGUMENT_NULL = -11,
    /**
     * A provided argument is too small.
     */
    ODIN_ERROR_ARGUMENT_TOO_SMALL = -12,
    /**
     * A provided argument is out of the expected bounds.
     */
    ODIN_ERROR_ARGUMENT_OUT_OF_BOUNDS = -13,
    /**
     * A provided string argument is not valid UTF-8.
     */
    ODIN_ERROR_ARGUMENT_INVALID_STRING = -14,
    /**
     * A provided handle argument is invalid.
     */
    ODIN_ERROR_ARGUMENT_INVALID_HANDLE = -15,
    /**
     * A provided identifier argument is invalid.
     */
    ODIN_ERROR_ARGUMENT_INVALID_ID = -16,
    /**
     * A provided JSON argument is invalid.
     */
    ODIN_ERROR_ARGUMENT_INVALID_JSON = -17,
    /**
     * The provided version is invalid.
     */
    ODIN_ERROR_INVALID_VERSION = -21,
    /**
     * The provided access key is invalid.
     */
    ODIN_ERROR_INVALID_ACCESS_KEY = -22,
    /**
     * The provided gateway/server address is invalid.
     */
    ODIN_ERROR_INVALID_URI = -23,
    /**
     * The provided token is invalid.
     */
    ODIN_ERROR_INVALID_TOKEN = -24,
    /**
     * The provided effect is not compatible with the expected effect type.
     */
    ODIN_ERROR_INVALID_EFFECT = -25,
    /**
     * The provided MessagePack encoded bytes are invalid.
     */
    ODIN_ERROR_INVALID_MSG_PACK = -26,
    /**
     * The provided JSON string is invalid.
     */
    ODIN_ERROR_INVALID_JSON = -27,
    /**
     * The provided token does not grant access to the requested room.
     */
    ODIN_ERROR_TOKEN_ROOM_REJECTED = -31,
    /**
     * The token is missing a customer identifier.
     */
    ODIN_ERROR_TOKEN_MISSING_CUSTOMER = -32,
    /**
     * The audio processing module reported an error.
     */
    ODIN_ERROR_AUDIO_PROCESSING_FAILED = -41,
    /**
     * The setup process of the Opus audio codec reported an error.
     */
    ODIN_ERROR_AUDIO_CODEC_CREATION_FAILED = -42,
    /**
     * Encoding of an audio packet failed.
     */
    ODIN_ERROR_AUDIO_ENCODING_FAILED = -43,
    /**
     * Decoding of an audio packet failed.
     */
    ODIN_ERROR_AUDIO_DECODING_FAILED = -44,
    /**
     * Position limit reached.
     */
    ODIN_ERROR_AUDIO_POSITION_LIMIT_REACHED = -45,
} OdinError;

/**
 * Audio-related events emitted by encoders and decoders. These values act as bitflags and
 * can be combined to filter or report multiple events.
 */
typedef enum OdinAudioEvents {
    /**
     * Triggered when the silence state changes.
     */
    ODIN_AUDIO_EVENTS_IS_SILENT_CHANGED = 1,
    /**
     * Triggered when 3D channel positions are added, updated or removed.
     */
    ODIN_AUDIO_EVENTS_POSITIONS_CHANGED = 2,
    /**
     * Enables all available audio events.
     */
    ODIN_AUDIO_EVENTS_ALL = -1,
} OdinAudioEvents;

/**
 * Valid levels for aggressiveness of the noise suppression. A higher level will reduce the noise
 * level at the expense of a higher speech distortion.
 */
typedef enum OdinNoiseSuppressionLevel {
    /**
     * Disable noise suppression.
     */
    ODIN_NOISE_SUPPRESSION_LEVEL_NONE = 0,
    /**
     * Use low suppression (6 dB).
     */
    ODIN_NOISE_SUPPRESSION_LEVEL_LOW = 1,
    /**
     * Use moderate suppression (12 dB).
     */
    ODIN_NOISE_SUPPRESSION_LEVEL_MODERATE = 2,
    /**
     * Use high suppression (18 dB).
     */
    ODIN_NOISE_SUPPRESSION_LEVEL_HIGH = 3,
    /**
     * Use very high suppression (21 dB).
     */
    ODIN_NOISE_SUPPRESSION_LEVEL_VERY_HIGH = 4,
} OdinNoiseSuppressionLevel;

/**
 * Available versions of the automatic gain controller (AGC) to be used. This adjusts the audio
 * signal's amplitude to reach a target level, helping to maintain a consistent output.
 */
typedef enum OdinGainControllerVersion {
    /**
     * AGC is disabled. The signal is untouched.
     */
    ODIN_GAIN_CONTROLLER_VERSION_DISABLED = 0,
    /**
     * Legacy AGC with adaptive digital gain and limiter.
     */
    ODIN_GAIN_CONTROLLER_VERSION_V1 = 1,
    /**
     * Enhanced AGC with digital processing and input volume control.
     */
    ODIN_GAIN_CONTROLLER_VERSION_V2 = 2,
} OdinGainControllerVersion;

/**
 * Defines the types of audio pipeline effects available.
 */
typedef enum OdinEffectType {
    /**
     * Voice Activity Detection (VAD) for detecting speech segments in an audio stream.
     */
    ODIN_EFFECT_TYPE_VAD = 0,
    /**
     * Audio Processing Module (APM) for enhancements such as noise suppression or echo cancellation.
     */
    ODIN_EFFECT_TYPE_APM = 1,
    /**
     * Custom user-defined audio processing effect that can be injected into the audio pipeline.
     */
    ODIN_EFFECT_TYPE_CUSTOM = 2,
} OdinEffectType;

/**
 * Represents a decoder for media streams from remote voice chat clients, which encapsulates all
 * the components required to process incoming audio streams. It includes an egress resampler for
 * sample rate conversion, an Opus decoder for decompressing audio data and a customizable audio
 * pipeline that enables the application of effects to modify the raw audio samples.
 *
 * Encoder operations are thread-safe and the same encoder handle may be accessed concurrently
 * from multiple threads.
 */
typedef struct OdinDecoder OdinDecoder;

/**
 * Represents an encoder for local media streams, which encapsulates the components required to
 * process outgoing audio streams captured from local sources (e.g. a microphone). It includes
 * an ingress resampler for sample rate conversion, an Opus encoder for compressing the audio
 * data and a customizable audio pipeline that allows the application of effects to modify the
 * raw audio samples before transmission.
 *
 * Encoder operations are thread-safe and the same encoder handle may be accessed concurrently
 * from multiple threads.
 */
typedef struct OdinEncoder OdinEncoder;

/**
 * A highly dynamic audio processing chain that manages a thread-safe collection of filters like
 * voice activity detection, echo cancellation, noise suppression and even custom effects. This
 * allows sequential processing and real-time modification of audio streams through operations
 * like insertion, removal, reordering and configuration updates.
 */
typedef struct OdinPipeline OdinPipeline;

/**
 * An opaque type representing an ODIN room handle, which is managed by the underlying connection.
 * This abstraction provides a high-level interface for joining rooms, managing persistent state
 * and sending/receiving data, making it easier to integrate room-based interactions into your
 * application.
 */
typedef struct OdinRoom OdinRoom;

/**
 * A handle for generating ODIN tokens, employed for generating signed room tokens predicated on
 * an access key. Be aware that access keys serve as your unique authentication keys, requisite for
 * generating room tokens to access the ODIN server network. To ensure your security, it's strongly
 * recommended that you _NEVER_ embed an access key within your client code and instead generate
 * room tokens on a server.
 */
typedef struct OdinTokenGenerator OdinTokenGenerator;

/**
 * Information attached to an incoming voice datagram. Provides information about the source
 * peer, the logical channel(s) it was transmitted on and internal transport identifiers. This
 * allows applications to distinguish streams, handle per-channel positioning, or perform custom
 * routing and filtering.
 */
typedef struct OdinDatagramProperties {
    uint32_t peer_id;
    uint64_t channel_mask;
    uint32_t ssrc_id;
    uint32_t reserved[4];
} OdinDatagramProperties;

/**
 * Room-level callbacks for incoming data and events.
 */
typedef struct OdinRoomEvents {
    void (*on_datagram)(struct OdinRoom *room,
                        const struct OdinDatagramProperties *properties,
                        const uint8_t *bytes,
                        uint32_t bytes_length,
                        void *user_data);
    void (*on_rpc)(struct OdinRoom *room, const char *json, void *user_data);
    void *user_data;
} OdinRoomEvents;

/**
 * Optional, pluggable encryption module for room communications. A cipher can be attached to an
 * ODIN room handle on creation to enable customizable, end-to-end encryption (E2EE). When enabled,
 * it intercepts data right before transmission and immediately after reception, allowing custom
 * processing of datagrams, messages and custom peer user data. The structure provides a suite of
 * callback functions for initialization, cleanup, event handling and encryption/decryption tasks,
 * along with parameters to adjust for any additional capacity overhead.
 */
typedef struct OdinCipher {
    void (*init)(struct OdinCipher *cipher, struct OdinRoom *room);
    void (*free)(struct OdinCipher *cipher);
    void (*on_event)(struct OdinCipher *cipher, const unsigned char *bytes, uint32_t length);
    int32_t (*encrypt_datagram)(struct OdinCipher *cipher,
                                const unsigned char *plaintext,
                                uint32_t plaintext_length,
                                unsigned char *ciphertext,
                                uint32_t ciphertext_capacity);
    int32_t (*decrypt_datagram)(struct OdinCipher *cipher,
                                uint32_t peer_id,
                                const unsigned char *ciphertext,
                                uint32_t ciphertext_length,
                                unsigned char *plaintext,
                                uint32_t plaintext_capacity);
    int32_t (*encrypt_message)(struct OdinCipher *cipher,
                               const unsigned char *plaintext,
                               uint32_t plaintext_length,
                               unsigned char *ciphertext,
                               uint32_t ciphertext_capacity);
    int32_t (*decrypt_message)(struct OdinCipher *cipher,
                               uint32_t peer_id,
                               const unsigned char *ciphertext,
                               uint32_t ciphertext_length,
                               unsigned char *plaintext,
                               uint32_t plaintext_capacity);
    int32_t (*encrypt_user_data)(struct OdinCipher *cipher,
                                 const unsigned char *plaintext,
                                 uint32_t plaintext_length,
                                 unsigned char *ciphertext,
                                 uint32_t ciphertext_capacity);
    int32_t (*decrypt_user_data)(struct OdinCipher *cipher,
                                 uint32_t peer_id,
                                 const unsigned char *ciphertext,
                                 uint32_t ciphertext_length,
                                 unsigned char *plaintext,
                                 uint32_t plaintext_capacity);
    uint32_t additional_capacity_datagram;
    uint32_t additional_capacity_message;
    uint32_t additional_capacity_user_data;
} OdinCipher;

/**
 * Statistics for the underlying connection of a room.
 */
typedef struct OdinConnectionStats {
    /**
     * The amount of outgoing UDP datagrams observed
     */
    uint64_t udp_tx_datagrams;
    /**
     * The total amount of bytes which have been transferred inside outgoing UDP datagrams
     */
    uint64_t udp_tx_bytes;
    /**
     * The amount of incoming UDP datagrams observed
     */
    uint64_t udp_rx_datagrams;
    /**
     * The total amount of bytes which have been transferred inside incoming UDP datagrams
     */
    uint64_t udp_rx_bytes;
    /**
     * Current best estimate of the connection latency (round-trip time) in milliseconds
     */
    float rtt;
} OdinConnectionStats;

/**
 * A 3D position to calculate the peer proximity. The coordinates are expressed as floating-point
 * values (x, y, z).
 */
typedef struct OdinPosition {
    float x;
    float y;
    float z;
} OdinPosition;

/**
 * A callback invoked when the decoder reports one or more audio events.
 */
typedef void (*OdinDecoderEventCallback)(struct OdinDecoder *decoder,
                                         enum OdinAudioEvents events,
                                         void *user_data);

/**
 * A callback invoked when the encoder reports one or more audio events.
 */
typedef void (*OdinEncoderEventCallback)(struct OdinEncoder *encoder,
                                         enum OdinAudioEvents events,
                                         void *user_data);

/**
 * Sensitivity parameters for ODIN voice activity detection module configuration.
 */
typedef struct OdinSensitivityConfig {
    /**
     * Indicates whether the sensitivity configuration is enabled.
     */
    bool enabled;
    /**
     * The threshold at which the trigger should engage.
     */
    float attack_threshold;
    /**
     * The threshold at which the trigger should disengage.
     */
    float release_threshold;
} OdinSensitivityConfig;

/**
 * Pipeline configuration of the ODIN voice activity detection module, which offers advanced
 * algorithms to accurately determine when to start or stop transmitting audio data.
 */
typedef struct OdinVadConfig {
    /**
     * When enabled, ODIN will analyze the audio input signal using smart voice detection algorithms
     * to determine the presence of speech. You can define both the probability required to start
     * and stop transmitting.
     */
    struct OdinSensitivityConfig voice_activity;
    /**
     * When enabled, ODIN will measure the volume of the input audio signal, deciding when a user
     * speaks loudly enough to transmit voice data. You can define both the root-mean-square
     * power (dBFS) for when the gate should engage and disengage.
     */
    struct OdinSensitivityConfig volume_gate;
} OdinVadConfig;

/**
 * Pipeline configuration of the ODIN audio processing module which provides a variety of smart
 * enhancement algorithms.
 */
typedef struct OdinApmConfig {
    /**
     * When enabled the echo canceller will try to subtract echoes, reverberation and unwanted
     * added sounds from the audio input signal. Note that you need to process the reverse audio
     * stream, also known as the loopback data to be used in the ODIN echo canceller.
     */
    bool echo_canceller;
    /**
     * When enabled, the high-pass filter will remove low-frequency content from the input audio
     * signal, thus making it sound cleaner and more focused.
     */
    bool high_pass_filter;
    /**
     * When enabled, the transient suppressor will try to detect and attenuate keyboard clicks.
     */
    bool transient_suppressor;
    /**
     * When enabled, the noise suppressor will remove distracting background noise from the input
     * audio signal. You can control the aggressiveness of the suppression. Increasing the level
     * will reduce the noise level at the expense of a higher speech distortion.
     */
    enum OdinNoiseSuppressionLevel noise_suppression_level;
    /**
     * When enabled, the gain controller will bring the input audio signal to an appropriate range
     * when it's either too loud or too quiet.
     */
    enum OdinGainControllerVersion gain_controller_version;
} OdinApmConfig;

/**
 * Defines the signature for custom effect callbacks in an ODIN audio pipeline. The callback
 * receives a pointer to a buffer of audio samples, the number of samples in the buffer, a
 * pointer to a flag indicating whether the audio is silent. This allows for custom, in-place
 * processing of an audio stream.
 */
typedef void (*OdinCustomEffectCallback)(float *samples,
                                         uint32_t samples_count,
                                         bool *is_silent,
                                         const void *user_data);

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Initializes the internal ODIN client runtime with a specified version number, ensuring the correct
 * header file is employed. The majority of the API functions hinge on an active ODIN runtime.
 *
 * Note: Use `ODIN_VERSION` to supply the `version` argument.
 */
enum OdinError odin_initialize(const char *version);

/**
 * Shuts down the internal ODIN runtime including all active connection pools. It is advisable to
 * invoke this function prior to terminating your application.
 */
void odin_shutdown(void);

/**
 * Returns the error message from the last occurred error, if available. If no error is present,
 * an empty string is returned.
 */
const char *odin_error_get_last_error(void);

/**
 * Resets the last error message by clearing the error buffer.
 */
void odin_error_reset_last_error(void);

/**
 * Creates a new ODIN room handle and starts the asynchronous connection process. This requires
 * the address of an ODIN gateway and an authentication string, which must either be a valid and
 * signed JSON Web Token (JWT) or a serialized JSON object containing the token, initial peer user
 * data, channel masks and other optional parameters. On success, a new room handle is stored in
 * `out_room`. If provided, the room events struct enables delivery of room-specific callbacks
 * to notify your application about incoming datagrams or RPCs. Additionally, an optional ODIN
 * cipher plugin can be provided to enable end-to-end encryption of room communications.
 */
enum OdinError odin_room_create(const char *gateway,
                                const char *authentication,
                                struct OdinRoomEvents *events,
                                struct OdinCipher *cipher,
                                struct OdinRoom **out_room);

/**
 * Closes the specified ODIN room handle, thus making our own peer leave the room on the server
 * and closing the connection if needed. To release resources, call `odin_room_free`` afterwards.
 */
void odin_room_close(struct OdinRoom *room);

/**
 * Retrieves the name from the specified room.
 */
enum OdinError odin_room_get_name(struct OdinRoom *room,
                                  char *out_value,
                                  uint32_t *out_value_length);

/**
 * Retrieves the underlying connection identifier associated with the room, or `0` if no valid
 * connection exists. The identifier is a 32-bit value.
 */
uint32_t odin_room_get_connection_id(struct OdinRoom *room);

/**
 * Retrieves detailed connection statistics for the specified room, filling the provided structure
 * with data such as the number of transmitted/received datagrams, bytes and round-trip time.
 */
enum OdinError odin_room_get_connection_stats(struct OdinRoom *room,
                                              struct OdinConnectionStats *out_stats);

/**
 * Flushes the local peer's user data by re-sending it to the server, ensuring that the latest
 * data is synchronized across all connected peers. This function does NOT need to be invoked
 * manually. It is typically used internally by an ODIN cipher after encryption key rotations
 * to update and maintain data consistency.
 */
enum OdinError odin_room_resend_user_data(struct OdinRoom *room);

/**
 * Sends a JSON-encoded RPC message to the server for the specified room.
 */
enum OdinError odin_room_send_rpc(struct OdinRoom *room, const char *json);

/**
 * Sends a JSON-encoded RPC message using a local loopback mechanism. It bypasses network
 * transmission by directly invoking the room’s RPC callback from `OdinRoomEvents`. This is
 * useful for emitting synthetic events for testing and internal processing without involving
 * the network layer.
 */
enum OdinError odin_room_send_loopback_rpc(struct OdinRoom *room, const char *json);

/**
 * Sends an encoded voice packet to the server for the specified room.
 */
enum OdinError odin_room_send_datagram(struct OdinRoom *room,
                                       const uint8_t *bytes,
                                       uint32_t bytes_length);

/**
 * Destroys the specified ODIN room handle and releases all underlying resources, even if the
 * room is still connecting.
 */
void odin_room_free(struct OdinRoom *room);

/**
 * Creates a new ODIN decoder with default settings for processing a remote media stream. All
 * decoded audio will be provided at the specified sample rate and channel count.
 */
enum OdinError odin_decoder_create(uint32_t sample_rate,
                                   bool stereo,
                                   struct OdinDecoder **out_decoder);

/**
 * Returns a pointer to the internal ODIN audio pipeline instance used by the given decoder.
 */
const struct OdinPipeline *odin_decoder_get_pipeline(struct OdinDecoder *decoder);

/**
 * Pushes an incoming datagram to the specified decoder for processing.
 */
enum OdinError odin_decoder_push(struct OdinDecoder *decoder,
                                 const uint8_t *datagram,
                                 uint32_t datagram_length);

/**
 * Retrieves a block of processed audio samples from the decoder's buffer. The samples are
 * interleaved floating-point values in the range [-1, 1] and are written into the provided
 * output buffer. A flag is also set to indicate if the output is silent.
 */
enum OdinError odin_decoder_pop(struct OdinDecoder *decoder,
                                float *out_samples,
                                uint32_t out_samples_count,
                                bool *out_is_silent);

/**
 * Returns a bitmask indicating the channels on which the most recently pushed voice packet
 * was transmitted. If no packet has been processed yet, the value is `0`.
 */
uint64_t odin_decoder_get_active_channels(struct OdinDecoder *decoder);

/**
 * Retrieves the list of 3D positions associated with the specified channel mask from the most
 * recently pushed voice packet.
 */
enum OdinError odin_decoder_get_positions(struct OdinDecoder *decoder,
                                          uint64_t channel_mask,
                                          struct OdinPosition *out_positions,
                                          uint32_t *out_positions_length);

/**
 * Returns whether the decoder is currently processing silence. This reflects the internal
 * silence detection state of the decoder, which updates as audio is processed. If the
 * provided handle is invalid, the function returns `true` for safety.
 */
bool odin_decoder_is_silent(struct OdinDecoder *decoder);

/**
 * Registers a callback to receive decoder audio events. The `filter` determines which event
 * types will trigger the callback, allowing selective handling. Any previously registered
 * callback is replaced. The callback is invoked with the decoder handle, the combined event
 * bitmask and the supplied `user_data`.
 */
enum OdinError odin_decoder_set_event_callback(struct OdinDecoder *decoder,
                                               enum OdinAudioEvents filter,
                                               OdinDecoderEventCallback callback,
                                               void *user_data);

/**
 * Frees the resources associated with the specified decoder.
 */
void odin_decoder_free(struct OdinDecoder *decoder);

/**
 * Creates a new ODIN encoder instance with default settings used to encode audio captured from
 * local sources, such as a microphone. The encoder encapsulates an ingress resampler using the
 * given sample rate and channel layout.
 */
enum OdinError odin_encoder_create(uint32_t peer_id,
                                   uint32_t sample_rate,
                                   bool stereo,
                                   struct OdinEncoder **out_encoder);

/**
 * Creates a new ODIN encoder instance for local media streams with extended codec configuration
 * parameters. In addition to the sample rate and stereo configuration, it allows specification
 * of whether the application is intended for VoIP, a target bitrate and the encoder's expected
 * packet loss percentage.
 */
enum OdinError odin_encoder_create_ex(uint32_t peer_id,
                                      uint32_t sample_rate,
                                      bool stereo,
                                      bool application_voip,
                                      uint32_t bitrate_kbps,
                                      uint32_t packet_loss_perc,
                                      uint64_t update_position_interval_ms,
                                      struct OdinEncoder **out_encoder);

/**
 * Returns a pointer to the internal ODIN audio pipeline instance used by the given encoder.
 */
const struct OdinPipeline *odin_encoder_get_pipeline(struct OdinEncoder *encoder);

/**
 * Pushes raw audio samples to the encoder for processing. The provided audio samples, which
 * must be interleaved floating-point values in the range [-1, 1], are processed through the
 * encoder's pipeline, allowing any configured effects to be applied prior to encoding.
 */
enum OdinError odin_encoder_push(struct OdinEncoder *encoder,
                                 const float *samples,
                                 uint32_t samples_count);

/**
 * Retrieves an encoded datagram from the encoder's buffer.
 */
enum OdinError odin_encoder_pop(struct OdinEncoder *encoder,
                                uint8_t *out_datagram,
                                uint32_t *out_datagram_length);

/**
 * Updates the 3D position for the specified channel mask. To assign different positions to
 * multiple masks, call this function once per mask.
 */
enum OdinError odin_encoder_set_position(struct OdinEncoder *encoder,
                                         uint64_t channel_mask,
                                         const struct OdinPosition *position);

/**
 * Clears the 3D position associated with the specified channel mask.
 */
enum OdinError odin_encoder_clear_position(struct OdinEncoder *encoder, uint64_t channel_mask);

/**
 * Returns whether the encoder is currently processing silence. This reflects the internal
 * silence detection state of the encoder, which updates as audio is processed. If the
 * provided handle is invalid, the function returns `true` for safety.
 */
bool odin_encoder_is_silent(struct OdinEncoder *encoder);

/**
 * Registers a callback to receive encoder audio events. The `filter` determines which event
 * types will trigger the callback, allowing selective handling. Any previously registered
 * callback is replaced. The callback is invoked with the encoder handle, the combined event
 * bitmask and the supplied `user_data`.
 */
enum OdinError odin_encoder_set_event_callback(struct OdinEncoder *encoder,
                                               enum OdinAudioEvents filter,
                                               OdinEncoderEventCallback callback,
                                               void *user_data);

/**
 * Frees the resources associated with the specified encoder.
 */
void odin_encoder_free(struct OdinEncoder *encoder);

/**
 * Inserts a Voice Activity Detection (VAD) effect into the audio pipeline at the specified
 * index, shifting subsequent effects. An index of `0` inserts at the beginning of the pipeline.
 * On success, a unique effect identifier is returned.
 */
enum OdinError odin_pipeline_insert_vad_effect(const struct OdinPipeline *pipeline,
                                               uint32_t index,
                                               uint32_t *out_effect_id);

/**
 * Retrieves the configuration for a VAD effect identified by `effect_id` from the specified
 * audio pipeline.
 */
enum OdinError odin_pipeline_get_vad_config(const struct OdinPipeline *pipeline,
                                            uint32_t effect_id,
                                            struct OdinVadConfig *out_config);

/**
 * Updates the configuration settings of the VAD effect identified by `effect_id` in the specified
 * audio pipeline.
 */
enum OdinError odin_pipeline_set_vad_config(const struct OdinPipeline *pipeline,
                                            uint32_t effect_id,
                                            const struct OdinVadConfig *config);

/**
 * Inserts an Audio Processing Module (APM) effect into the audio pipeline at the specified
 * index, shifting subsequent effects. An index of `0` inserts at the beginning of the pipeline.
 * On success, a unique effect identifier is returned.
 */
enum OdinError odin_pipeline_insert_apm_effect(const struct OdinPipeline *pipeline,
                                               uint32_t index,
                                               uint32_t playback_sample_rate,
                                               bool playback_stereo,
                                               uint32_t *out_effect_id);

/**
 * Retrieves the configuration for an APM effect identified by `effect_id` from the specified
 * audio pipeline.
 */
enum OdinError odin_pipeline_get_apm_config(const struct OdinPipeline *pipeline,
                                            uint32_t effect_id,
                                            struct OdinApmConfig *out_config);

/**
 * Updates the configuration settings of the APM effect identified by `effect_id` in the specified
 * audio pipeline.
 */
enum OdinError odin_pipeline_set_apm_config(const struct OdinPipeline *pipeline,
                                            uint32_t effect_id,
                                            const struct OdinApmConfig *config);

/**
 * Updates the specified APM effect's sample buffer for processing the reverse (playback) audio
 * stream. The provided samples must be interleaved float values in the range [-1, 1]. The delay
 * parameter is used to align the reverse stream processing with the forward (capture) stream.
 * The delay can be expressed as:
 *
 *  `delay = (t_render - t_analyze) + (t_process - t_capture)`
 *
 * where:
 *  - `t_render` is the time the first sample of the same frame is rendered by the audio hardware.
 *  - `t_analyze` is the time the frame is processed in the reverse stream.
 *  - `t_capture` is the time the first sample of a frame is captured by the audio hardware.
 *  - `t_process` is the time the frame is processed in the forward stream.
 */
enum OdinError odin_pipeline_update_apm_playback(const struct OdinPipeline *pipeline,
                                                 uint32_t effect_id,
                                                 const float *samples,
                                                 uint32_t samples_count,
                                                 uint64_t delay_ms);

/**
 * Inserts a user-defined custom effect at the specified index in the audio pipeline. The effect
 * is implemented via a callback function and associated user data. A unique effect identifier
 * is returned.
 */
enum OdinError odin_pipeline_insert_custom_effect(const struct OdinPipeline *pipeline,
                                                  uint32_t index,
                                                  OdinCustomEffectCallback callback,
                                                  const void *user_data,
                                                  uint32_t *out_effect_id);

/**
 * Returns the unique effect identifier from an audio pipeline corresponding to the effect located
 * at the specified index.
 */
enum OdinError odin_pipeline_get_effect_id(const struct OdinPipeline *pipeline,
                                           uint32_t index,
                                           uint32_t *out_effect_id);

/**
 * Searches the specified audio pipeline for the effect with the specified `effect_id` and returns
 * its current index.
 */
enum OdinError odin_pipeline_get_effect_index(const struct OdinPipeline *pipeline,
                                              uint32_t effect_id,
                                              uint32_t *out_index);

/**
 * Obtains the effect type (VAD, APM, or Custom) for the effect identified by `effect_id`.
 */
enum OdinError odin_pipeline_get_effect_type(const struct OdinPipeline *pipeline,
                                             uint32_t effect_id,
                                             enum OdinEffectType *out_effect_type);

/**
 * Retrieves the total number of effects currently in the audio pipeline.
 */
uint32_t odin_pipeline_get_effect_count(const struct OdinPipeline *pipeline);

/**
 * Reorders the audio pipeline by moving the effect with the specified `effect_id` to a new index.
 */
enum OdinError odin_pipeline_move_effect(const struct OdinPipeline *pipeline,
                                         uint32_t effect_id,
                                         size_t new_index);

/**
 * Deletes the effect identified by `effect_id` from the specified audio pipeline.
 */
enum OdinError odin_pipeline_remove_effect(const struct OdinPipeline *pipeline, uint32_t effect_id);

/**
 * Creates a new token generator using the specified ODIN access key. If no access key is provided,
 * a new one will be generated.
 */
enum OdinError odin_token_generator_create(const char *access_key,
                                           struct OdinTokenGenerator **out_token_generator);

/**
 * Frees the specified token generator and releases the resources associated with it.
 */
void odin_token_generator_free(struct OdinTokenGenerator *token_generator);

/**
 * Retrieves the ODIN access key used by the specified token generator. An ODIN access key is a 44
 * character long Base64 string that consists of an internal version number, a set of random bytes
 * and a checksum.
 */
enum OdinError odin_token_generator_get_access_key(struct OdinTokenGenerator *token_generator,
                                                   char *out_access_key,
                                                   uint32_t *out_access_key_length);

/**
 * Extracts the key ID from the access key used by the specified token generator. The key ID is
 * embedded in room tokens, enabling the identification of the corresponding public key required
 * for verification.
 */
enum OdinError odin_token_generator_get_key_id(struct OdinTokenGenerator *token_generator,
                                               char *out_key_id,
                                               uint32_t *out_key_id_length);

/**
 * Extracts the public key from the access key used by the specified token generator. The public
 * key, derived from the Ed25519 curve, must be shared with _4Players_ to enable verification of
 * a generated room token.
 */
enum OdinError odin_token_generator_get_public_key(struct OdinTokenGenerator *token_generator,
                                                   char *out_public_key,
                                                   uint32_t *out_public_key_length);

/**
 * Signs the provided body using the key ID and access key stored in the token generator, producing
 * a JSON Web Token (JWT). The EdDSA (Ed25519) algorithm is used for the digital signature.
 */
enum OdinError odin_token_generator_sign(struct OdinTokenGenerator *token_generator,
                                         const char *body,
                                         char *out_token,
                                         uint32_t *out_token_length);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
