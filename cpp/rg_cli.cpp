#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <memory>

#include <nlohmann/json.hpp>

#include "encoder.hh"
#include "decoder.hh"

struct WavHeader {
    char riff[4];
    uint32_t size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
};

bool write_wav(const std::string &path, const std::vector<int16_t> &samples,
               int rate, int channels) {
    WavHeader h;
    std::memcpy(h.riff, "RIFF", 4);
    std::memcpy(h.wave, "WAVE", 4);
    std::memcpy(h.fmt, "fmt ", 4);
    std::memcpy(h.data, "data", 4);
    h.fmt_size = 16;
    h.audio_format = 1;
    h.num_channels = channels;
    h.sample_rate = rate;
    h.bits_per_sample = 16;
    h.block_align = h.num_channels * h.bits_per_sample / 8;
    h.byte_rate = h.sample_rate * h.block_align;
    h.data_size = samples.size() * sizeof(int16_t);
    h.size = 36 + h.data_size;

    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    out.write(reinterpret_cast<char*>(&h), sizeof(h));
    out.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));
    return out.good();
}

bool read_wav(const std::string &path, std::vector<int16_t> &samples,
              int &rate, int &channels) {
    WavHeader h;
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!in)
        return false;
    if (std::strncmp(h.riff, "RIFF", 4) || std::strncmp(h.wave, "WAVE", 4) ||
        h.audio_format != 1 || h.bits_per_sample != 16)
        return false;
    rate = h.sample_rate;
    channels = h.num_channels;
    samples.resize(h.data_size / sizeof(int16_t));
    in.read(reinterpret_cast<char*>(samples.data()), h.data_size);
    return in.good();
}

template<int RATE>
bool encode_rate(const std::string &message, const std::string &callsign,
                 int carrier, int noise, bool fancy, int channel,
                 const std::string &outfile) {
    constexpr int SYMBOL_LENGTH = (1280 * RATE) / 8000;
    constexpr int GUARD_LENGTH = SYMBOL_LENGTH / 8;
    constexpr int EXTENDED_LENGTH = SYMBOL_LENGTH + GUARD_LENGTH;
    Encoder<RATE> encoder;
    uint8_t payload[170]{};
    std::copy(message.begin(), message.end(), payload);
    int8_t call[10]{};
    std::copy(callsign.begin(), callsign.end(), call);
    encoder.configure(payload, call, carrier, noise, fancy);
    std::vector<int16_t> block((channel == 1 || channel == 2 || channel == 4)
                               ? 2 * EXTENDED_LENGTH : EXTENDED_LENGTH);
    std::vector<int16_t> all;
    while (encoder.produce(block.data(), channel))
        all.insert(all.end(), block.begin(), block.end());
    encoder.produce(block.data(), channel);
    all.insert(all.end(), block.begin(), block.end());
    if (all.empty())
        return false;
    int channels = (channel == 1 || channel == 2 || channel == 4) ? 2 : 1;
    return write_wav(outfile, all, RATE, channels);
}

template<int RATE>
bool encode_rate_samples(const std::string &message, const std::string &callsign,
                         int carrier, int noise, bool fancy, int channel,
                         std::vector<int16_t> &out) {
    constexpr int SYMBOL_LENGTH = (1280 * RATE) / 8000;
    constexpr int GUARD_LENGTH = SYMBOL_LENGTH / 8;
    constexpr int EXTENDED_LENGTH = SYMBOL_LENGTH + GUARD_LENGTH;
    Encoder<RATE> encoder;
    uint8_t payload[170]{};
    std::copy(message.begin(), message.end(), payload);
    int8_t call[10]{};
    std::copy(callsign.begin(), callsign.end(), call);
    encoder.configure(payload, call, carrier, noise, fancy);
    std::vector<int16_t> block((channel == 1 || channel == 2 || channel == 4)
                               ? 2 * EXTENDED_LENGTH : EXTENDED_LENGTH);
    while (encoder.produce(block.data(), channel))
        out.insert(out.end(), block.begin(), block.end());
    encoder.produce(block.data(), channel);
    out.insert(out.end(), block.begin(), block.end());
    return !out.empty();
}

bool encode_to_wav(const std::string &message, const std::string &callsign,
                   int carrier, int noise, bool fancy, int rate, int channel,
                   const std::string &outfile) {
    switch (rate) {
        case 8000: return encode_rate<8000>(message, callsign, carrier, noise, fancy, channel, outfile);
        case 16000: return encode_rate<16000>(message, callsign, carrier, noise, fancy, channel, outfile);
        case 32000: return encode_rate<32000>(message, callsign, carrier, noise, fancy, channel, outfile);
        case 44100: return encode_rate<44100>(message, callsign, carrier, noise, fancy, channel, outfile);
        case 48000: return encode_rate<48000>(message, callsign, carrier, noise, fancy, channel, outfile);
        default:
            std::cerr << "Unsupported sample rate\n";
            return false;
    }
}

bool encode_to_vec(const std::string &message, const std::string &callsign,
                   int carrier, int noise, bool fancy, int rate, int channel,
                   std::vector<int16_t> &out) {
    switch (rate) {
        case 8000: return encode_rate_samples<8000>(message, callsign, carrier, noise, fancy, channel, out);
        case 16000: return encode_rate_samples<16000>(message, callsign, carrier, noise, fancy, channel, out);
        case 32000: return encode_rate_samples<32000>(message, callsign, carrier, noise, fancy, channel, out);
        case 44100: return encode_rate_samples<44100>(message, callsign, carrier, noise, fancy, channel, out);
        case 48000: return encode_rate_samples<48000>(message, callsign, carrier, noise, fancy, channel, out);
        default:
            std::cerr << "Unsupported sample rate\n";
            return false;
    }
}

bool encode_from_json(const std::string &jsonfile, const std::string &outfile) {
    std::ifstream in(jsonfile);
    if (!in)
        return false;
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception &) {
        return false;
    }
    std::string message = j.value("message", "");
    std::string callsign = j.value("callsign", "N0CALL");
    int carrier = j.value("carrierFrequency", 1500);
    int noise = j.value("noiseSymbols", 0);
    bool fancy = j.value("fancyHeader", false);
    int rate = j.value("sampleRate", 48000);
    int channel = j.value("channel", 0);
    return encode_to_wav(message, callsign, carrier, noise, fancy, rate, channel, outfile);
}

template<int RATE>
bool decode_rate(const std::vector<int16_t> &samples, int channels,
                 int channel_select, nlohmann::json &out) {
    constexpr int SYMBOL_LENGTH = (1280 * RATE) / 8000;
    constexpr int GUARD_LENGTH = SYMBOL_LENGTH / 8;
    constexpr int EXTENDED_LENGTH = SYMBOL_LENGTH + GUARD_LENGTH;
    Decoder<RATE> decoder;
    uint8_t payload[170]{};
    int status = STATUS_OKAY;
    size_t off = 0;
    while (off < samples.size()) {
        int frames = std::min<int>(EXTENDED_LENGTH, (samples.size() - off) / channels);
        int chunk = frames * channels;
        bool ready = decoder.feed(samples.data() + off, frames, channel_select);
        off += chunk;
        if (ready) {
            status = decoder.process();
            if (status == STATUS_DONE || status == STATUS_FAIL ||
                status == STATUS_NOPE || status == STATUS_PING)
                break;
        }
    }
    float cfo = 0;
    int32_t mode = 0;
    uint8_t call[10]{};
    if (status == STATUS_DONE) {
        decoder.staged(&cfo, &mode, call);
        int bits_flipped = decoder.fetch(payload);
        if (bits_flipped < 0) {
            std::cerr << "Decoding failed during fetch\n";
            return false;
        }
        out["status"] = "done";
        out["message"] = reinterpret_cast<char*>(payload);
        out["bitFlips"] = bits_flipped;
    } else if (status == STATUS_NOPE || status == STATUS_PING) {
        decoder.staged(&cfo, &mode, call);
        out["status"] = (status == STATUS_NOPE) ? "nope" : "ping";
        out["message"] = "";
        out["bitFlips"] = 0;
    } else {
        std::cerr << "Decoding failed with status " << status << "\n";
        return false;
    }
    std::string call_str(reinterpret_cast<char*>(call), reinterpret_cast<char*>(call) + 10);
    size_t zero = call_str.find('\0');
    if (zero != std::string::npos)
        call_str.resize(zero);
    size_t first = call_str.find_first_not_of(' ');
    size_t last = call_str.find_last_not_of(' ');
    if (first == std::string::npos)
        call_str.clear();
    else
        call_str = call_str.substr(first, last - first + 1);
    out["callsign"] = call_str;
    out["mode"] = mode;
    out["cfo"] = cfo;
    out["sampleRate"] = RATE;
    out["channels"] = channels;
    out["channelSelect"] = channel_select;
    return true;
}

bool decode_wav(const std::string &infile, int channel_select, nlohmann::json &out) {
    std::vector<int16_t> samples;
    int rate = 0;
    int channels = 0;
    if (!read_wav(infile, samples, rate, channels)) {
        std::cerr << "Failed to read wav file\n";
        return false;
    }
    if (channel_select < 0)
        channel_select = (channels == 2) ? 3 : 0;
    switch (rate) {
        case 8000: return decode_rate<8000>(samples, channels, channel_select, out);
        case 16000: return decode_rate<16000>(samples, channels, channel_select, out);
        case 32000: return decode_rate<32000>(samples, channels, channel_select, out);
        case 44100: return decode_rate<44100>(samples, channels, channel_select, out);
        case 48000: return decode_rate<48000>(samples, channels, channel_select, out);
        default:
            std::cerr << "Unsupported sample rate\n";
            return false;
    }
}

bool decode_from_vec(const int16_t *samples, size_t count, int rate, int channels,
                     int channel_select, nlohmann::json &out) {
    std::vector<int16_t> buf(samples, samples + count);
    if (channel_select < 0)
        channel_select = (channels == 2) ? 3 : 0;
    switch (rate) {
        case 8000: return decode_rate<8000>(buf, channels, channel_select, out);
        case 16000: return decode_rate<16000>(buf, channels, channel_select, out);
        case 32000: return decode_rate<32000>(buf, channels, channel_select, out);
        case 44100: return decode_rate<44100>(buf, channels, channel_select, out);
        case 48000: return decode_rate<48000>(buf, channels, channel_select, out);
        default:
            std::cerr << "Unsupported sample rate\n";
            return false;
    }
}

static std::unique_ptr<DecoderInterface> make_decoder(int rate) {
    switch (rate) {
        case 8000: return std::unique_ptr<DecoderInterface>(new Decoder<8000>());
        case 16000: return std::unique_ptr<DecoderInterface>(new Decoder<16000>());
        case 32000: return std::unique_ptr<DecoderInterface>(new Decoder<32000>());
        case 44100: return std::unique_ptr<DecoderInterface>(new Decoder<44100>());
        case 48000: return std::unique_ptr<DecoderInterface>(new Decoder<48000>());
        default: return nullptr;
    }
}

extern "C" {
int wasm_encode(const char *message, const char *callsign, int carrier, int noise,
                int fancy, int rate, int channel, int16_t *out, int max_samples) {
    std::vector<int16_t> samples;
    if (!encode_to_vec(message, callsign, carrier, noise, fancy, rate, channel, samples))
        return 0;
    int n = std::min<int>(samples.size(), max_samples);
    std::copy(samples.begin(), samples.begin() + n, out);
    return n;
}

int wasm_decode(const int16_t *samples, int count, int rate, int channels,
                int channel_select, char *out_message, int max_len,
                char *out_call, int call_len) {
    static std::unique_ptr<DecoderInterface> decoder;
    static int current_rate = 0;
    if (rate != current_rate || !decoder) {
        decoder = make_decoder(rate);
        current_rate = rate;
        if (!decoder) {
            std::cerr << "Unsupported sample rate\n";
            return 0;
        }
    }
    if (channel_select < 0)
        channel_select = (channels == 2) ? 3 : 0;
    int frames = count / channels;
    if (decoder->feed(samples, frames, channel_select)) {
        int status = decoder->process();
        if (status == STATUS_DONE) {
            float cfo = 0;
            int32_t mode = 0;
            uint8_t call[10]{};
            decoder->staged(&cfo, &mode, call);
            uint8_t payload[170]{};
            int bits_flipped = decoder->fetch(payload);
            if (bits_flipped >= 0) {
                std::string msg(reinterpret_cast<char*>(payload));
                int n = std::min<int>(msg.size(), max_len - 1);
                std::memcpy(out_message, msg.data(), n);
                out_message[n] = '\0';
                int c = std::min<int>(sizeof(call), call_len - 1);
                std::memcpy(out_call, call, c);
                out_call[c] = '\0';
                decoder = make_decoder(rate);
                current_rate = rate;
                return n;
            }
            decoder = make_decoder(rate);
            current_rate = rate;
            return -STATUS_FAIL;
        } else if (status == STATUS_SYNC) {
            return -STATUS_SYNC;
        } else if (status == STATUS_NOPE || status == STATUS_PING ||
                   status == STATUS_FAIL || status == STATUS_HEAP) {
            decoder = make_decoder(rate);
            current_rate = rate;
            return -status;
        }
    }
    return 0;
}
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " encode <output.wav> <input.json>\n";
        std::cerr << "       " << argv[0] << " decode <input.wav> [channel]\n";
        return 1;
    }
    std::string mode = argv[1];
    if (mode == "encode") {
        if (argc < 4) {
            std::cerr << "encode requires output file and json\n";
            return 1;
        }
        std::string outfile = argv[2];
        std::string jsonfile = argv[3];
        if (!encode_from_json(jsonfile, outfile)) {
            std::cerr << "Encoding failed\n";
            return 1;
        }
        std::cout << "Wrote " << outfile << "\n";
        return 0;
    } else if (mode == "decode") {
        if (argc < 3) {
            std::cerr << "decode requires input file\n";
            return 1;
        }
        int channel = -1;
        if (argc > 3)
            channel = std::stoi(argv[3]);
        nlohmann::json j;
        if (!decode_wav(argv[2], channel, j))
            return 1;
        std::cout << j.dump() << "\n";
        std::string status = j.value("status", "");
        std::string call = j.value("callsign", "");
        int mode = j.value("mode", 0);
        double cfo = j.value("cfo", 0.0);
        if (!call.empty())
            std::cerr << "from " << call << ", mode " << mode
                      << ", cfo " << cfo << "\n";
        if (status == "done") {
            int flips = j.value("bitFlips", 0);
            std::cerr << flips << " bit" << (flips == 1 ? " flip" : " flips")
                      << " corrected\n";
        } else if (status == "ping") {
            std::cerr << "preamble ping\n";
        } else if (status == "nope") {
            std::cerr << "preamble nope\n";
        }
        return 0;
    } else {
        std::cerr << "Unknown mode\n";
        return 1;
    }
}

