![ODIN](https://www.4players.io/images/odin/banner.jpg)

[![Releases](https://img.shields.io/github/release/4Players/odin-sdk)](https://github.com/4Players/odin-sdk/releases)
[![Documentation](https://img.shields.io/badge/docs-4Players.io-orange)](https://docs.4players.io/voice)
[![Twitter](https://img.shields.io/badge/twitter-@ODIN4Players-blue)](https://twitter.com/ODIN4Players)

# 4Players ODIN Voice SDK

ODIN is a versatile cross-platform Software Development Kit (SDK) engineered to seamlessly integrate real-time voice chat into multiplayer games, applications, and websites. Regardless of whether you're employing a native application or your preferred web browser, ODIN simplifies the process of maintaining connections with your significant contacts. Through its intuitive interface and robust functionality, ODIN enhances interactive experiences, fostering real-time engagement and collaboration across various digital platforms.

You can choose between a managed cloud and a self-hosted solution. Let [4Players GmbH](https://www.4players.io/) deal with the setup, administration and bandwidth costs or run our [server software](https://github.com/4Players/odin-server) on your own infrastructure allowing you complete control and customization of your deployment environment.

- [Supported Platforms](#supported-platforms)
- [Getting Started](#getting-started)
- [Usage](#usage)
   - [Quick Start](#quick-start)
   - [Authentication](#authentication)
   - [Connection Pooling](#connection-pooling)
      - [Voice Data Handling](#voice-data-handling)
      - [Event Handling](#event-handling)
      - [Event Flow](#event-flow)
   - [Audio Encoding and Decoding](#audio-encoding-and-decoding)
   - [Audio Pipelines & Effects](#audio-pipelines-and-effects)
   - [Audio Events For Talk Status Detection](#audio-events-for-talk-status-detection)
   - [User Data](#user-data)
   - [Proximity Chat](#proximity-chat)
      - [Channel Masks](#channel-masks)
      - [Updating Positions](#updating-positions)
      - [Background Updates](#background-updates)
   - [Messages](#messages)
   - [End-to-End Encryption](#end-to-end-encryption-cipher)
- [Testing](#testing)
- [Resources](#resources)
- [Troubleshooting](#troubleshooting)

## Supported Platforms

The current release of ODIN is shipped with native pre-compiled binaries for the following platforms:

| Platform | x86_64             | aarch64            | x86                | armv7a             |
|----------|--------------------|--------------------|--------------------|--------------------|
| Windows  | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| Linux    | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| macOS    | :white_check_mark: | :white_check_mark: |                    |                    |
| Android  | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |
| iOS      | :white_check_mark: | :white_check_mark: |                    |                    |

Support for gaming consoles is available upon request and covers Microsoft Xbox, Sony PlayStation 4/5 and Nintendo Switch.

If your project requires support for any additional platform, please [contact us](https://www.4players.io/company/contact_us/).

## Getting Started

To check out the SDK for development, clone the git repo into a working directory of your choice.

This repository uses [LFS](https://git-lfs.github.com) (large file storage) to manage pre-compiled binaries. Note that a standard clone of the repository might only retrieve the metadata about these files managed with LFS. In order to retrieve the actual data with LFS, please follow these steps:

1. Clone the repository:  
   `git clone https://github.com/4Players/odin-sdk.git`

2. Cache the actual LFS data on your local machine:  
   `git lfs fetch`

3. Replaces the metadata in the binary files with their actual contents:  
   `git lfs checkout`

## Usage

### Quick Start

The following code snippet illustrates how to join a designated room on a specified server using a room token acquired externally:

```c
#include <stdio.h>
#include "odin.h"

int main(int argc, const char *argv[])
{
   odin_initialize(ODIN_VERSION);

   OdinRoomEvents events = {
      .on_datagram = NULL,
      .on_rpc = NULL,
      .user_data = NULL
   };

   OdinRoom *room;
   odin_room_create("<SERVER_URL>", "<TOKEN>", &events, NULL, &room);

   getchar();

   odin_room_free(room);
   odin_shutdown();

   return 0;
}
```

### Authentication

To enter a room, an authentication token is requisite. ODIN employs the creation of signed JSON Web Tokens (JWT) to ensure a secure authentication pathway. These tokens encapsulate the details of the room(s) you wish to join alongside a customizable identifier for the user, which can be leveraged to reference an existing record within your specific service.

```cpp
OdinTokenGenerator *generator;
odin_token_generator_create("<ACCESS_KEY>", &generator);

char token[512];
odin_token_generator_sign(generator, payload, token, sizeof(token));

odin_token_generator_free(generator);
```

The minimal payload of an ODIN token looks like this:

```jsonc
{
   "rid": "foo",      // customer-speficied room ID
   "uid": "bar",      // customer-speficied user ID as reference
   "exp": 1669852860, // token expiration date
   "nbf": 1669852800  // token activation time  
}
```

As ODIN is fully user agnostic, [4Players GmbH](https://www.4players.io/) does not store any of this information on its servers.

Tokens are signed employing an Ed25519 key pair derived from your distinctive access key. Think of an access key as a singular, unique authentication credential, crucial for generating room tokens to access the ODIN server network. It essentially combines the roles of a username and password into a singular, unobtrusive string of characters, necessitating a comparable degree of protection. For bolstered security, it is strongly recommended to refrain from incorporating an access key in your client-side code. We've created a very basic [Node.js](https://docs.4players.io/voice/token-server/) server here, to showcase how to issue ODIN tokens to your client apps without exposing your access key.

### Connection Pooling

ODIN uses connection pooling under the hood to manage all network communication and event routing between your application and the ODIN server infrastructure. Multiple rooms automatically share the same underlying pool, but you no longer need to manage it manually - everything is handled transparently.

To get started, define your callbacks for incoming data and control messages:

```cpp
void on_datagram(OdinRoom* room, const OdinDatagramProperties* props, const uint8_t* bytes, uint32_t bytes_length, void* user_data);

void on_rpc(OdinRoom* room, const char* json, void* user_data);
```

Then set up the room events and create a room:

```cpp
OdinRoomEvents events = {
   .on_datagram = &on_datagram, // handles incoming voice data
   .on_rpc      = &on_rpc,      // handles signaling and server events
   .user_data   = &state        // user-defined context passed to callbacks
};

OdinRoom* room;
odin_room_create("<GATEWAY_URL>", "<TOKEN>", &events, nullptr, &room);
```

The `authentication` parameter can be provided in two forms: either as a raw JWT string or as a serialized JSON object containing additional fields such as the room ID, channel masks and peer user data. Using the JSON object form can be convenient if you want to include optional metadata or audio channel configuration alongside the token.

Once created, the room automatically connects and joins in the background. Any additional rooms you create will reuse the same underlying connection pool without extra configuration, so you can focus entirely on your application logic.

#### Voice Data Handling

Voice data is delivered through the `on_datagram` callback. Each incoming packet includes metadata such as the source peer ID and channel mask, which you can use to route the data to the correct ODIN decoder. A typical implementation looks like this:

```cpp
void on_datagram(OdinRoom* room, const OdinDatagramProperties* properties, const uint8_t* bytes, uint32_t bytes_length, void* user_data) {
   const auto state = reinterpret_cast<State*>(user_data);
   assert(state->room.get() == room);
    
   if (auto it = state->decoders.find(properties->peer_id); it != state->decoders.end()) {
      odin_decoder_push(it->second.ptr.get(), bytes, bytes_length);
   }
}
```

#### Event Handling

Control and event messages are delivered through the `on_rpc` callback. These messages are sent as plain UTF-8 encoded JSON strings. This makes it straightforward to inspect and parse events using your favorite JSON library, such as [nlohmann::json](https://github.com/nlohmann/json), without any additional decoding steps.

```cpp
#include <nlohmann/json.hpp>
using namespace nlohmann;

void on_rpc(OdinRoom* room, const char* json_text, void* user_data) {
   const auto state = reinterpret_cast<State*>(user_data);
   assert(state->room.get() == room);

   try {
      json rpc = json::parse(json_text);
      std::cout << "event: " << rpc.dump() << std::endl;
   } catch (const std::exception& err) {
      std::cerr << "error: " << err.what() << std::endl;
   }
}
```

Events follow a simple object format where the event name is the top-level key, for example:

```json
{ "PeerJoined": { "peer_id": 1234, "user_id": "Player A" } }
```

This structure makes it easy to pattern-match on event names and handle them accordingly.

#### Event Flow

Understanding the event flow is essential for building responsive applications. The following diagram illustrates a typical connection lifecycle:

```mermaid
sequenceDiagram
    participant A as Client A
    participant S as Server
    participant B as Client B

    A->>S: Connect + HelloRoom
    S-->>A: RoomStatusChanged [joining]
    S-->>A: RoomStatusChanged [joined]
    S-->>A: Joined

    B->>S: Connect + HelloRoom
    S-->>B: RoomStatusChanged [joining]
    S-->>B: RoomStatusChanged [joined]
    S-->>B: PeerJoined (A)
    S-->>B: Joined
    S-->>A: PeerJoined (B)

    A->>S: ChangeSelf
    S-->>B: PeerChanged (A)
    
    A->>S: Voice Datagrams
    S-->>B: Voice Datagrams

    A->>S: Disconnect
    S-->>A: RoomStatusChanged [closed]
    S-->>B: PeerLeft (A)
```

##### RoomStatusChanged

A synthetic event emitted by the client library when the underlying connection status of an ODIN room changes. This event is not actually sent by the server but generated locally to provide a unified event interface.

| Field     | Type     | Description                                                  |
| --------- | -------- | ------------------------------------------------------------ |
| `status`  | `string` | Connection status: `joining`, `joined`, or `closed`          |
| `message` | `string` | Optional human-readable message explaining the status change |

**Example:**
```json
{ "RoomStatusChanged": { "status": "joining", "message": "initial connect" } }
{ "RoomStatusChanged": { "status": "joined" } }
{ "RoomStatusChanged": { "status": "closed", "message": "room closed by user request" } }
```

##### Joined

Emitted after successfully joining a room. Any `PeerJoined` events received before this event represent peers that were already in the room when you connected.

| Field         | Type     | Description                                   |
| ------------- | -------- | --------------------------------------------- |
| `own_peer_id` | `u32`    | Your assigned peer ID for this room session   |
| `room_id`     | `string` | The identifier of the room you joined         |
| `customer`    | `string` | The customer/organization that owns this room |

**Example:**
```json
{ "Joined": { "own_peer_id": 42, "room_id": "lobby", "customer": "acme-corp" } }
```

##### Left

Emitted when you are removed from the room by the server.

| Field    | Type     | Description                                                              |
| -------- | -------- | ------------------------------------------------------------------------ |
| `reason` | `string` | Why you were removed: `room_closing`, `server_closing`, or `peer_kicked` |

**Example:**
```json
{ "Left": { "reason": "room_closing" } }
```

##### PeerJoined

Emitted when another peer joins the room. If received before the `Joined` event, it indicates a peer that was already present when you connected.

| Field        | Type       | Description                                               |
| ------------ | ---------- | --------------------------------------------------------- |
| `peer_id`    | `u32`      | Unique identifier for this peer in the room               |
| `user_id`    | `string`   | Application-defined user identifier from the peer's token |
| `user_data`  | `object`   | Optional custom metadata attached to the peer             |
| `tags`       | `string[]` | Optional list of tags assigned to the peer                |
| `parameters` | `object`   | Optional key-value parameters for the peer                |

**Example:**
```json
{ "PeerJoined": { "peer_id": 17, "user_id": "player-abc", "user_data": { "name": "Alice" } } }
```

##### PeerLeft

Emitted when another peer leaves the room.

| Field     | Type  | Description                       |
| --------- | ----- | --------------------------------- |
| `peer_id` | `u32` | The peer ID of the peer that left |

**Example:**
```json
{ "PeerLeft": { "peer_id": 17 } }
```

##### PeerChanged

Emitted when a peer updates their user data, tags, or parameters.

| Field        | Type       | Description                                       |
| ------------ | ---------- | ------------------------------------------------- |
| `peer_id`    | `u32`      | The peer ID of the peer that changed              |
| `user_data`  | `object`   | Updated custom metadata (present if changed)      |
| `tags`       | `string[]` | Updated list of tags (present if changed)         |
| `parameters` | `object`   | Updated key-value parameters (present if changed) |

**Example:**
```json
{ "PeerChanged": { "peer_id": 17, "user_data": { "name": "Alice", "status": "away" } } }
```

##### MessageReceived

Emitted when another peer sends you a direct message.

| Field            | Type            | Description                                       |
| ---------------- | --------------- | ------------------------------------------------- |
| `sender_peer_id` | `u32`           | The peer ID of the message sender                 |
| `message`        | `array\|object` | The message payload (binary array or JSON object) |

**Example:**
```json
{ "MessageReceived": { "sender_peer_id": 17, "message": [104, 101, 108, 108, 111] } }
{ "MessageReceived": { "sender_peer_id": 17, "message": { "type": "chat", "text": "Hello!" } } }
```

##### Error

Emitted when an error occurs on the server side.

| Field     | Type     | Description                      |
| --------- | -------- | -------------------------------- |
| `message` | `string` | Human-readable error description |

**Example:**
```json
{ "Error": { "message": "something went wrong" } }
```

### Audio Encoding and Decoding

ODIN separates audio transmission into two core components: **encoders** for outgoing streams and **decoders** for incoming ones. These components handle real-time audio conversion and transport using a shared audio pipeline interface. Internally, each encoder/decoder wraps an Opus codec for compression/decompression as well as an ingress or egress resampler for automatic sample rate conversion. This design provides high performance, low latency and flexible hooks for integrating features like Voice Activity Detection (VAD), audio enhancements (APM) or custom effects.

Unlike earlier versions, there are no separate media objects anymore. Instead, applications are expected to create encoders and decoders as needed - for example, one encoder for the local peer and one decoder per remote peer. It's also possible to transmit voice on multiple audio channels within the same room simultaneously. In theses cases, the `on_datagram` callback provides the corresponding channel mask so you can decide how to route or process incoming audio.

When creating encoders and decoders, simply specify the sample rate and channel count (mono or stereo) of your source or target audio material. ODIN handles all internal resampling automatically, so you can feed samples directly from your audio source without manual conversion. All audio data must be provided as 32-bit floats, interleaved, and normalized to `[-1.0, 1.0]`. Internally, ODIN processes audio in 20ms chunks.

#### Encoders (Outgoing Audio)

An encoder transforms raw PCM audio captured from a local source (e.g., microphone) into compressed ODIN datagrams for transmission.

```cpp
OdinEncoder* encoder;
odin_encoder_create(peer_id, sample_rate, stereo, &encoder);

const OdinPipeline* pipeline = odin_encoder_get_pipeline(encoder);
```

Push captured audio samples into the encoder. Samples must be interleaved 32-bit floats in the range `[-1.0, 1.0]`:

```cpp
odin_encoder_push(encoder, samples, sample_count);
```

Poll for encoded datagrams and send them to the room:

```cpp
for (;;) {
   uint8_t datagram[2048];
   uint32_t datagram_length = sizeof(datagram);
   switch (odin_encoder_pop(encoder, datagram, &datagram_length)) {
      case ODIN_ERROR_SUCCESS:
         CHECK(odin_room_send_datagram(room, datagram, datagram_length));
         break;
      case ODIN_ERROR_NO_DATA:
         return; // no more packets ready
      default:
         std::cerr << "failed to encode audio datagram" << std::endl;
   }
}
```

**Note:** You can optionally assign 3D positions per channel mask on the encoder to enable positional audio.

#### Decoders (Incoming Audio)

A decoder processes datagrams received from remote peers. Typically, you’ll create one decoder per peer, which you update in your `on_datagram` callback.

```cpp
OdinDecoder* decoder;
odin_decoder_create(sample_rate, stereo, &decoder);

const OdinPipeline* pipeline = odin_decoder_get_pipeline(decoder);
```

Push received datagrams into the decoder:

```cpp
odin_decoder_push(decoder, bytes, length);
```

Retrieve decoded audio for playback:

```cpp
float samples[FRAME_SIZE];
bool is_silent;
odin_decoder_pop(decoder, samples, FRAME_SIZE, &is_silent);
```

**Note:** The `on_datagram` callback provides the channel mask used for each packet, allowing you to handle multi-channel or spatialized setups efficiently.

#### Streaming an Audio Buffer

The following example demonstrates how to stream a pre-loaded audio buffer (e.g. from a file) into an ODIN room. The key is to feed samples in real-time intervals matching the audio duration. ODIN processes audio internally in 20ms chunks.

```cpp
// Assuming the audio_buffer contains 32-bit float samples at 48kHz stereo
const int sample_rate = 48000;
const int channel_count = 2;
const int frames_per_20ms = sample_rate * 20 / 1000;           // 960 frames at 48kHz
const int samples_per_chunk = frames_per_20ms * channel_count; // 1920 samples for stereo

OdinEncoder* encoder;
odin_encoder_create(peer_id, sample_rate, channel_count == 2, &encoder);

// Stream the buffer in 20ms chunks
auto next_time = std::chrono::steady_clock::now();
for (size_t offset = 0; offset < audio_buffer_size; offset += samples_per_chunk) {
    size_t chunk_size = std::min(samples_per_chunk, audio_buffer_size - offset);
    odin_encoder_push(encoder, &audio_buffer[offset], chunk_size);

    // Pop and send encoded datagrams
    for (;;) {
        uint8_t datagram[2048];
        uint32_t datagram_length = sizeof(datagram);
        if (odin_encoder_pop(encoder, datagram, &datagram_length) != ODIN_ERROR_SUCCESS)
            break;
        odin_room_send_datagram(room, datagram, datagram_length);
    }

    // Pace to real-time using wall clock (self-corrects for encoding overhead)
    next_time += std::chrono::milliseconds(20);
    std::this_thread::sleep_until(next_time);
}

odin_encoder_free(encoder);
```

### Audio Pipelines and Effects

Each encoder and decoder in ODIN exposes an internal audio pipeline - a flexible and extensible processing chain that allows real-time manipulation of audio streams. Effects are applied in sequence to all audio flowing through the encoder or decoder. The pipeline automatically manages effect ordering internally, ensuring a consistent and logical execution flow.

#### VAD (Voice Activity Detection) Effects

The VAD module helps determine when a user is actively speaking. It provides two independent mechanisms:

- **Voice Activity Detection:** \
   When enabled, ODIN will analyze the audio input signal using smart voice detection algorithm to determine the presence of speech. You can define both the probability required to start and stop transmitting.
- **Voice Volume Gate:** \
   When enabled, the volume gate will measure the volume of the input audio signal, thus deciding when a user is speaking loud enough to transmit voice data. You can define both the root mean square power (dBFS) for when the gate should engage and disengage.

```cpp
uint32_t vad_id;
odin_pipeline_insert_vad_effect(pipeline, index, &vad_id);

OdinVadConfig config = {
   .voice_activity = {
      .enabled = true,
      .attack_threshold = 0.9f,
      .release_threshold = 0.8f
   },
   .volume_gate = {
      .enabled = true,
      .attack_threshold = -30,
      .release_threshold = -40
   }
};
odin_pipeline_set_vad_config(pipeline, vad_id, &config);
```

#### APM (Audio Processing Module) Effects

The APM module uses a variety of smart enhancement algorithms for enhancing audio clarity in noisy environments. It provides the following features:

- **Acoustic Echo Cancellation (AEC):** \
   When enabled the echo canceller will try to subtract echoes, reverberation, and unwanted added sounds from the audio input signal. Note, that you need to process the reverse audio stream, also known as the loopback data to be used in the ODIN echo canceller.
- **Noise Suppression:** \
   When enbabled, the noise suppressor will remove distracting background noise from the input audio signal. You can control the aggressiveness of the suppression. Increasing the level will reduce the noise level at the expense of a higher speech distortion.
- **High-Pass Filter (HPF):** \
   When enabled, the high-pass filter will remove low-frequency content from the input audio signal, thus making it sound cleaner and more focused.
- **Transient Suppression:** \
   When enabled, the transient suppressor will try to detect and attenuate keyboard clicks.
- **Automatic Gain Control (AGC):** \
   When enabled, the gain controller will bring the input audio signal to an appropriate range when it's either too loud or too quiet.

```cpp
uint32_t apm_id;
odin_pipeline_insert_apm_effect(pipeline, index, playback_sample_rate, stereo, &apm_id);

OdinApmConfig config = {
    .echo_canceller = true,
    .high_pass_filter = true,
    .transient_suppressor = true,
    .noise_suppression_level = ODIN_NOISE_SUPPRESSION_LEVEL_MODERATE,
    .gain_controller_version = ODIN_GAIN_CONTROLLER_VERSION_V2
};
odin_pipeline_set_apm_config(pipeline, apm_id, &config);
```

You can also push loopback (reverse) audio into the pipeline to support echo cancellation:

```cpp
odin_pipeline_update_apm_playback(pipeline, apm_id, reverse_samples, sample_count, delay_ms);
```

#### Custom Effects

ODIN allows you to define and insert your own audio processing functions. These run inline on the sample stream:

```cpp
void my_effect_callback(float *samples, uint32_t sample_count, bool *is_silent, const void *user_data) {
   // modify samples or observe silence state
}

uint32_t effect_id;
odin_pipeline_insert_custom_effect(pipeline, index, my_effect_callback, user_data, &effect_id);
```

You can remove or reorder effects dynamically as needed. All modifications are thread-safe and take effect immediately.

### Audio Events For Talk Status Detection

ODIN provides built-in audio event callbacks that allow applications to conveniently detect when a peer starts or stops talking - without inserting custom audio effects into the pipeline. Both encoders and decoders support event callbacks through `odin_encoder_set_event_callback` and `odin_decoder_set_event_callback`. These callbacks emit event bitmasks from `OdinAudioEvents`, including `ODIN_AUDIO_EVENTS_IS_SILENT_CHANGED`.

#### Encoder Talk Status (Local Peer)

You can detect whether the local user is speaking by registering an encoder event callback:

```cpp
void on_encoder_event(OdinEncoder* encoder, OdinAudioEvents events, void* user_data) {
    if (events & ODIN_AUDIO_EVENTS_IS_SILENT_CHANGED) {
        bool silent = odin_encoder_is_silent(encoder);
        printf("You %s talking\n", silent ? "stopped" : "started");
    }
}

// register the callback after creating the encoder
odin_encoder_set_event_callback(encoder, ODIN_AUDIO_EVENTS_IS_SILENT_CHANGED, &on_encoder_event, user_data);
```

This event fires automatically whenever the encoder decides that the input signal contains speech or silence, based on your configured VAD/APM modules (if any).

#### Decoder Talk Status (Remote Peers)

You can also detect whether remote peers are speaking. Since each peer typically has one decoder, you can pass the peer ID directly as `user_data`.
The callback can then use it without searching:

```cpp
void on_decoder_event(OdinDecoder* decoder, OdinAudioEvents events, void* user_data) {
    if (events & ODIN_AUDIO_EVENTS_IS_SILENT_CHANGED) {
        auto peer_id = *reinterpret_cast<const uint32_t*>(user_data);
        bool silent = odin_decoder_is_silent(decoder);
        printf("peer %u %s talking\n", peer_id, silent ? "stopped" : "started");
    }
}

// register the callback after creating the encoder
odin_decoder_set_event_callback(decoder, ODIN_AUDIO_EVENTS_IS_SILENT_CHANGED, &on_decoder_event, &peer_id);
```

This lets you trigger UI updates (e.g. "speaking" indicators) or gameplay logic whenever any peer starts or stops talking.

### User Data

Each peer in an ODIN room can carry a custom, user-defined user data payload, represented as a JSON object. This data is automatically synchronized across all connected peers and provides a flexible way to share peer-specific metadata such as usernames, avatars, roles, team assignments, mute flags or gameplay state.

You can set the initial user data in the authentication JSON when creating or joining a room:

```cpp
nlohmann::json authentication = {
   {"token", "<JWT>"},
   {"user_data", {
      {"name", "Player A"},
      {"team", "red"}
   }}
};

odin_room_create("<GATEWAY_HOST>", authentication.dump().data(), &events, nullptr, &room);
```

Once connected, you can update the local peer’s user data dynamically at any time by sending a client command through RPC:

```cpp
nlohmann::json command = {
   {"ChangeSelf", {
      {"user_data", {
         {"name", "Player A"},
         {"team", "red"},
         {"status", "ready"}
      }}
   }}
};

odin_room_send_rpc(room, command.dump().data());
```

All peers in the room will receive the updated user data automatically via the `PeerChanged` event. This makes it simple to keep in-game state or UI elements synchronized without additional signaling layers.

### Proximity Chat

ODIN provides built-in support for proximity-based voice communication through a combination of channel masks and per-channel positions. Each room supports **up to sixty-four audio channels**, represented by a 64-bit channel mask. Peers can transmit on one or more channels at the same time and each receiver decides exactly which peers and channels to hear by adjusting its masks. Every channel can also carry its own 3D position, which the server uses to perform automatic distance culling within a unit sphere radius of `1.0`. By scaling your world coordinates into this unit sphere before sending, you can implement scalable, spatially-aware voice systems without manually filtering streams on the client.

#### Channel Masks

ODIN provides a powerful channel mask system to determine which peers and channels you receive audio from. Each peer maintains a global/server mask, identified by `PeerId::SERVER` (which corresponds to `0` in the public API) and a set of per-peer masks. When deciding whether to forward audio from a given peer and channel, the server takes the bitwise AND of the global mask and the specific peer’s mask. By default, both are set to `FULL`, meaning you will receive all audio from all peers.

Clearing the global mask switches the behavior into a more restrictive whitelist mode: since `0 & mask = 0`, no audio is received unless you explicitly add per-peer masks with non-zero bits. This gives you precise control over who is audible at any given time without starting or stopping individual media streams.
You can specify initial masks in the authentication JSON when joining a room. The following example clears the global mask and enables channels *1*, *3* and *5* for peer `1234`:

```cpp
nlohmann::json authentication = {
    {"token", "<JWT>"},
    {"channel_masks", {
        {0,    0x00}, // global/server mask
        {1234, 0x15}  // 0b00010101 → channels 1, 3, 5
    }}
};

odin_room_create("<GATEWAY_HOST>", authentication.dump().data(), &events, nullptr, &room);
```

Masks can also be updated dynamically at runtime by sending a JSON command. The `SetChannelMasks` command lets you modify the global and per-peer masks at any time. The `reset` field controls whether the new masks replace or extend the existing configuration. When `reset` is set to `true`, the server clears all previously stored masks for that peer before applying the new set. When `reset` is `false` (or omitted), the new entries are additive, which means that they are merged into the existing configuration, allowing you to incrementally enable or adjust individual peers and channels without affecting others.

```cpp
nlohmann::json command = {
   {"SetChannelMasks", {
      {"masks", {
         {0,    0x00},
         {1234, 0x06}  // 0b00000110 → channels 2, 3
      }},
      {"reset", true}  // replace all existing masks
   }}
};

odin_room_send_rpc(room, command.dump().data());
```

This makes it easy to switch between fully redefining the audio routing configuration and making small, targeted changes on the fly.

#### Updating Positions

Each encoder can publish **per-channel 3D positions**, which the server uses to apply distance culling and spatialization for receivers. You can update your position at any time by calling odin_encoder_set_position() with the appropriate channel mask:

```cpp
OdinPosition pos = {playerXScaled, playerYScaled, playerZScaled};
odin_encoder_set_position(encoder, 0x01 | 0x04, &pos);
```

By default, new ODIN encoders publish on channel 1 at the origin. This is represented internally by a single position entry for channel 1 (`0x01`). If you want to publish on multiple channels, call `odin_encoder_set_position()` with the corresponding bitmask. If you clear all positions, the encoder will stop publishing on those channels entirely.

Use `odin_encoder_clear_position()` to dynamically remove channel positions when a channel becomes inactive. Once cleared, the server will stop using positional data for those channels and no audio will be published on them until a new position is set.

```cpp
odin_encoder_clear_position(encoder, 0x01);
```

#### Background Updates

The position is automatically sent with every outgoing audio datagram while the encoder is transmitting. When the encoder is silent, it will send **background position updates periodically** if you configured an update interval when creating the encoder with `odin_encoder_create_ex()`. If no interval was set, positions are only transmitted while speaking. For example, to configure an encoder that sends background position updates twice per second when not talking:

```cpp
uint64_t update_position_interval_ms = 500;

OdinEncoder* encoder;
odin_encoder_create_ex(peer_id, sample_rate, stereo, application_voip, bitrate_kbps, expected_packet_loss_perc, position_update_interval_ms, &encoder);
```

Background updates allow the server to continue culling and updating spatial relationships even when the user is silent, which is particularly useful in open-world or large multiplayer environments where players may move frequently without constantly transmitting audio.

### Messages

ODIN allows you to send arbitrary, application-defined messages between peers in the same room. Messages are delivered **reliably** and **in order** over the signaling channel, making them ideal for gameplay events, chat messages, state synchronization or any other non-audio communication. The payload format is entirely up to you - it can be raw bytes, JSON, Protocol Buffers or anything else you choose.

To send a message, construct a JSON command and pass it to `odin_room_send_rpc()`. If you provide a list of `peer_ids`, the message is sent to those peers only. If you omit the list, the message is broadcast to all connected peers.

```cpp
nlohmann::json command = {
   {"SendMessage", {
      {"message", {104, 101, 108, 108, 111}},
      {"peer_ids", target_peer_ids} // optional
   }}
};

odin_room_send_rpc(room, command.dump().data());
```

Messages are received through the `on_rpc` callback, where they arrive as standard JSON objects containing the binary payload. Unlike voice data, messages are **not affected by proximity or channel masks**. Instead, they are always delivered to the intended recipients, regardless of their position or audio configuration.

### End-to-End Encryption (Cipher)

ODIN supports end-to-end encryption (E2EE) through the use of a pluggable `OdinCipher` module. This enables you to secure all datagrams, messages and peer user data with a shared room password - without relying on the server as a trust anchor.

```cpp
OdinCipher *cipher = odin_crypto_create(ODIN_CRYPTO_VERSION);
odin_crypto_set_password(cipher, (const uint8_t *)"secret", strlen("secret"));

OdinRoom *room;
odin_room_create("<GATEWAY_HOST>", "<JWT>", &events, cipher, &room);
```

The encryption system uses a master key derived from the password, then derives peer-specific session keys with random salts. These salts are exchanged in-room so that all participants can reconstruct each other's peer keys. The master key never leaves the client and there are no long-term keys stored or distributed. Keys are rotated automatically after every 1 million packets or 2 GiB of traffic. The system is designed to minimize passive and active compromise by external actors. If the password is kept secure, so is your data.

## Testing

In addition to the latest binaries and C header files, this repository also contains a simple test client in the `test` sub-directory. Please note, that the configure process will try to download, verify and extract dependencies (e.g. [miniaudio](https://miniaud.io)), which are specified in the `CMakeLists.txt` file. [miniaudio](https://miniaud.io) is used to provide basic audio capture and playback functionality in the test client.

### Configuring and Building

1. Create a build directory:  
   `mkdir -p build && cd build`

2. Generate build scripts for your preferred build system:

   - For _make_ ...  
     `cmake ../`
   - ... or for _ninja_ ...  
     `cmake -GNinja ../`

3. Build the test client:
   - Using _make_ ...  
     `make`
   - ... or _ninja_ ...  
     `ninja`

On Windows, calling `cmake` from the build directory will create a _Visual Studio_ solution, which can be built using the following command:

```bat
msbuild odin_client.sln
```

### Using the Test Client

The test client accepts several arguments to control its functions, but the following three options are particularly crucial for its intended purpose:

```text
odin_client -r <room_id> -k <access_key> -s <server_url>
```

The `-r` argument (or `--room-id`) is used to specify the name of the room to join. If no room name is provided, the client will automatically join a room called **default**.

The `-k` argument (or `--access-key`) is used to specify an access key for generating tokens. If no access key is provided, the test client will auto-generate a key and display it in the console. An access key is a unique authentication key used to generate room tokens for accessing the 4Players ODIN server network. It is important to use the same access key for all clients that wish to join the same ODIN room. For more information about access keys, please refer to our [documentation](https://docs.4players.io/voice/introduction/access-keys/).

The `-s` argument (or `--server-url`) allows you to specify an alternate ODIN server address. This address can be either the URL to an ODIN gateway or an ODIN server. You may need to specify an alternate server if you are hosting your own fleet of ODIN servers. If you do not specify an ODIN server URL, the test client will use the default gateway, which is located at **https://gateway.odin.4players.io**.

**Note:** You can use the `--help` argument to get a full list of options provided by the console client.

## Resources

- [Documentation](https://docs.4players.io/voice/)
- [Frequently Asked Questions](https://docs.4players.io/voice/faq/)
- [Logos and Branding](https://docs.4players.io/voice/branding/)
- [Pricing](https://odin.4players.io/pricing/)

## Troubleshooting

Contact us through the listed methods below to receive answers to your questions and learn more about ODIN.

### Discord

Join our [official Discord server](https://4np.de/discord) to chat with us directly and become a part of the 4Players ODIN community.

### Email

Don’t use Discord or X? Send us an [email](mailto:odin@4players.io) and we’ll get back to you as soon as possible.