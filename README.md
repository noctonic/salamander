# Salamander

This is a fork [rattlegram](https://github.com/aicodix/rattlegram) v1.17 that is designed to work in browser using WASM, it also comes with a commandline tool that uses json.

You can use it now [HERE](https://noctonic.github.io/salamander/)

## System dependencies

Building the command line tools requires a C++17 compiler and`make`

Ubuntu:
```
sudo apt-get install build-essential
```

To produce the WebAssembly version, install Emscripten 

```
sudo apt install emscripten
```

## Command-line interface


### Encode

Supply a json file, `message` is required.

```json
{"message": "Hello"}
```

| Key | Description | Default |
|-----|-------------|---------|
| `message` | text to transmit | (required) |
| `callsign` | up to 9 characters | `N0CALL` |
| `carrierFrequency` | carrier offset in Hz | `1500` |
| `noiseSymbols` | number of noise symbols to prepend | `0` |
| `fancyHeader` | enable the fancy header | `false` |
| `sampleRate` | one of 8000, 16000, 32000, 44100 or 48000 | `48000` |
| `channel` | 0 for mono, 1 left, 2 right, 4 I/Q | `0` |

Example with all fields:

```json
{
  "message": "Hello",
  "callsign": "N0CALL",
  "carrierFrequency": 1500,
  "noiseSymbols": 0,
  "fancyHeader": false,
  "sampleRate": 48000,
  "channel": 0
}
```

Run the encoder with:

```
rgcli encode output.wav payload.json
```

### Decode

Decoding a WAV file outputs the json

```
rgcli decode input.wav [channel]
```

## WebAssembly build

Running `make wasm` generates
`web/rgcli.mjs` and `web/rgcli.wasm`

If you just want the web app running locally and you don't have time for all of the above, go to the releases section and download the latest release, unzip it, cd to folder, and launch a local webserver with python.
```
python3 -m http.server
```
then browse to 

```
http://localhost:8000
```
If you are on windows and don't have python you can [use this powershell script](https://gist.github.com/noctonic/0cadb91a2c127ecf3ef93c11a813dd5d)
