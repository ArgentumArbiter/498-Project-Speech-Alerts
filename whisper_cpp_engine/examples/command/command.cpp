// Voice assistant example
//
// Speak short text commands to the microphone.
// This program will detect your voice command and convert them to text.
//
// ref: https://github.com/ggerganov/whisper.cpp/issues/171
//

#include "common.h"
#include "common-sdl.h"
#include "whisper.h"

#include <sstream>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <map>

#include <iostream>
// command-line parameters
struct whisper_params {
    int32_t n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t prompt_ms  = 5000;
    int32_t command_ms = 700;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;

    float vad_thold    = 0.6f;
    float freq_thold   = 100.0f;

    bool speed_up      = false;
    bool translate     = false;
    bool print_special = false;
    bool print_energy  = false;
    bool no_timestamps = true;

    std::string language  = "en";
    std::string model     = "ggml-base.en.bin";
    std::string fname_out;
    std::string commands;
    std::string prompt;
};

void whisper_print_usage(int argc, char ** argv, const whisper_params & params);

bool whisper_params_parse(int argc, char ** argv, whisper_params & params) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "-t"   || arg == "--threads")       { params.n_threads     = std::stoi(argv[++i]); }
        else if (arg == "-pms" || arg == "--prompt-ms")     { params.prompt_ms     = std::stoi(argv[++i]); }
        else if (arg == "-cms" || arg == "--command-ms")    { params.command_ms    = std::stoi(argv[++i]); }
        else if (arg == "-c"   || arg == "--capture")       { params.capture_id    = std::stoi(argv[++i]); }
        else if (arg == "-mt"  || arg == "--max-tokens")    { params.max_tokens    = std::stoi(argv[++i]); }
        else if (arg == "-ac"  || arg == "--audio-ctx")     { params.audio_ctx     = std::stoi(argv[++i]); }
        else if (arg == "-vth" || arg == "--vad-thold")     { params.vad_thold     = std::stof(argv[++i]); }
        else if (arg == "-fth" || arg == "--freq-thold")    { params.freq_thold    = std::stof(argv[++i]); }
        else if (arg == "-su"  || arg == "--speed-up")      { params.speed_up      = true; }
        else if (arg == "-tr"  || arg == "--translate")     { params.translate     = true; }
        else if (arg == "-ps"  || arg == "--print-special") { params.print_special = true; }
        else if (arg == "-pe"  || arg == "--print-energy")  { params.print_energy  = true; }
        else if (arg == "-l"   || arg == "--language")      { params.language      = argv[++i]; }
        else if (arg == "-m"   || arg == "--model")         { params.model         = argv[++i]; }
        else if (arg == "-f"   || arg == "--file")          { params.fname_out     = argv[++i]; }
        else if (arg == "-cmd" || arg == "--commands")      { params.commands      = argv[++i]; }
        else if (arg == "-p"   || arg == "--prompt")        { params.prompt        = argv[++i]; }
        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}

void whisper_print_usage(int /*argc*/, char ** argv, const whisper_params & params) {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [options]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h,         --help           [default] show this help message and exit\n");
    fprintf(stderr, "  -t N,       --threads N      [%-7d] number of threads to use during computation\n", params.n_threads);
    fprintf(stderr, "  -pms N,     --prompt-ms N    [%-7d] prompt duration in milliseconds\n",             params.prompt_ms);
    fprintf(stderr, "  -cms N,     --command-ms N   [%-7d] command duration in milliseconds\n",            params.command_ms);
    fprintf(stderr, "  -c ID,      --capture ID     [%-7d] capture device ID\n",                           params.capture_id);
    fprintf(stderr, "  -mt N,      --max-tokens N   [%-7d] maximum number of tokens per audio chunk\n",    params.max_tokens);
    fprintf(stderr, "  -ac N,      --audio-ctx N    [%-7d] audio context size (0 - all)\n",                params.audio_ctx);
    fprintf(stderr, "  -vth N,     --vad-thold N    [%-7.2f] voice activity detection threshold\n",        params.vad_thold);
    fprintf(stderr, "  -fth N,     --freq-thold N   [%-7.2f] high-pass frequency cutoff\n",                params.freq_thold);
    fprintf(stderr, "  -su,        --speed-up       [%-7s] speed up audio by x2 (reduced accuracy)\n",     params.speed_up ? "true" : "false");
    fprintf(stderr, "  -tr,        --translate      [%-7s] translate from source language to english\n",   params.translate ? "true" : "false");
    fprintf(stderr, "  -ps,        --print-special  [%-7s] print special tokens\n",                        params.print_special ? "true" : "false");
    fprintf(stderr, "  -pe,        --print-energy   [%-7s] print sound energy (for debugging)\n",          params.print_energy ? "true" : "false");
    fprintf(stderr, "  -l LANG,    --language LANG  [%-7s] spoken language\n",                             params.language.c_str());
    fprintf(stderr, "  -m FNAME,   --model FNAME    [%-7s] model path\n",                                  params.model.c_str());
    fprintf(stderr, "  -f FNAME,   --file FNAME     [%-7s] text output file name\n",                       params.fname_out.c_str());
    fprintf(stderr, "  -cmd FNAME, --commands FNAME [%-7s] text file with allowed commands\n",             params.commands.c_str());
    fprintf(stderr, "  -p,         --prompt         [%-7s] the required activation prompt\n",              params.prompt.c_str());
    fprintf(stderr, "\n");
}

// command-list mode
// guide the transcription to match the most likely command from a provided list
int process_command_list(struct whisper_context * ctx, audio_async &audio, const whisper_params &params) {
    fprintf(stderr, "\n");
    fprintf(stderr, "%s: guided mode\n", __func__);

	std::vector<std::string> allowed_commands = {"hello"};
	
	
	// Initializes allowed_commands from command.txt
	std::string temp;
	std::ifstream command_file("command.txt");
	
	if (!command_file.is_open()) {
		std::ofstream open_file("command.txt");
		open_file.close();
	}
	
	command_file >> temp;
	std::cout << temp << "\n";
	command_file.close();
	
	if (!temp.empty())
		allowed_commands[0] = temp;
	
    int max_len = 0;

    std::vector<std::vector<whisper_token>> allowed_tokens;

    for (const auto & cmd : allowed_commands) {
        whisper_token tokens[1024];
        allowed_tokens.emplace_back();

        for (int l = 0; l < (int) cmd.size(); ++l) {
            // NOTE: very important to add the whitespace !
            //       the reason is that the first decoded token starts with a whitespace too!
            std::string ss = std::string(" ") + cmd.substr(0, l + 1);

            const int n = whisper_tokenize(ctx, ss.c_str(), tokens, 1024);
            if (n < 0) {
                fprintf(stderr, "%s: error: failed to tokenize command '%s'\n", __func__, cmd.c_str());
                return 3;
            }

            if (n == 1) {
                allowed_tokens.back().push_back(tokens[0]);
            }
        }

        max_len = std::max(max_len, (int) cmd.size());
    }

    fprintf(stderr, "%s: allowed commands [ tokens ]:\n", __func__);
    fprintf(stderr, "\n");
    for (int i = 0; i < (int) allowed_commands.size(); ++i) {
        fprintf(stderr, "  - \033[1m%-*s\033[0m = [", max_len, allowed_commands[i].c_str());
        for (const auto & token : allowed_tokens[i]) {
            fprintf(stderr, " %5d", token);
        }
        fprintf(stderr, " ]\n");
    }

    std::string  k_prompt = "select one from the available words: ";
    for (int i = 0; i < (int) allowed_commands.size(); ++i) {
        if (i > 0) {
            k_prompt += ", ";
        }
        k_prompt += allowed_commands[i];
    }
    k_prompt += ". selected word: ";

    // tokenize prompt
    std::vector<whisper_token> k_tokens;
    {
        k_tokens.resize(1024);
        const int n = whisper_tokenize(ctx, k_prompt.c_str(), k_tokens.data(), 1024);
        if (n < 0) {
            fprintf(stderr, "%s: error: failed to tokenize prompt '%s'\n", __func__, k_prompt.c_str());
            return 4;
        }
        k_tokens.resize(n);
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "%s: prompt: '%s'\n", __func__, k_prompt.c_str());
    fprintf(stderr, "%s: tokens: [", __func__);
    for (const auto & token : k_tokens) {
        fprintf(stderr, " %d", token);
    }
    fprintf(stderr, " ]\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "%s: listening for a command ...\n", __func__);
    fprintf(stderr, "\n");

    bool is_running  = true;

    std::vector<float> pcmf32_cur;
    std::vector<float> pcmf32_prompt;

    // main loop
    while (is_running) {
        // handle Ctrl + C
        is_running = sdl_poll_events();

        // delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        audio.get(2000, pcmf32_cur);
		
		float probab;

        if (::vad_simple(pcmf32_cur, WHISPER_SAMPLE_RATE, 1000, params.vad_thold, params.freq_thold, params.print_energy)) {
            fprintf(stdout, "%s: Speech detected! Processing ...\n", __func__);

            const auto t_start = std::chrono::high_resolution_clock::now();

            whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

            wparams.print_progress   = false;
            wparams.print_special    = params.print_special;
            wparams.print_realtime   = false;
            wparams.print_timestamps = !params.no_timestamps;
            wparams.translate        = params.translate;
            wparams.no_context       = true;
            wparams.single_segment   = true;
            wparams.max_tokens       = 1;
            wparams.language         = params.language.c_str();
            wparams.n_threads        = params.n_threads;

            wparams.audio_ctx        = params.audio_ctx;
            wparams.speed_up         = params.speed_up;

            wparams.prompt_tokens    = k_tokens.data();
            wparams.prompt_n_tokens  = k_tokens.size();

            // run the transformer and a single decoding pass
            if (whisper_full(ctx, wparams, pcmf32_cur.data(), pcmf32_cur.size()) != 0) {
                fprintf(stderr, "%s: ERROR: whisper_full() failed\n", __func__);
                break;
            }

            // estimate command probability
            // NOTE: not optimal
            {
                const auto * logits = whisper_get_logits(ctx);

                std::vector<float> probs(whisper_n_vocab(ctx), 0.0f);

                // compute probs from logits via softmax
                {
                    float max = std::max((float) -1e9, logits[0]);
                    //for (int i = 0; i < (int) probs.size(); ++i) {
					//	max = std::max(max, logits[i]);
                    //}

                    float sum = 0.0f;
                    for (int i = 0; i < (int) probs.size(); ++i) {
                        probs[i] = expf(logits[i] - max);
                        sum += probs[i];
                    }

                    for (int i = 0; i < (int) probs.size(); ++i) {
                        probs[i] /= sum;
                    }
                }
				
				
                std::vector<std::pair<float, int>> probs_id;

                double psum = 0.0;
                for (int i = 0; i < (int) allowed_commands.size(); ++i) {
                    probs_id.emplace_back(probs[allowed_tokens[i][0]], i);
                    for (int j = 1; j < (int) allowed_tokens[i].size(); ++j) {
                        probs_id.back().first += probs[allowed_tokens[i][j]];
                    }
                    probs_id.back().first /= allowed_tokens[i].size();
                    psum += probs_id.back().first;
                }

                // normalize
                for (auto & p : probs_id) {
                    p.first /= psum;
                }
				
				float probab =
					probs[allowed_tokens[0][(int) allowed_tokens[0].size() - 1]];
				std::ofstream outfile("output.txt");
				fprintf(stdout, "%f PROBAB\n", probab);
				outfile << probab << "\n";
				outfile.close();
            }
			
            audio.clear();
        }
    }
	

    return 0;
}

int main(int argc, char ** argv) {
    whisper_params params;

    if (whisper_params_parse(argc, argv, params) == false) {
        return 1;
    }

    if (whisper_lang_id(params.language.c_str()) == -1) {
        fprintf(stderr, "error: unknown language '%s'\n", params.language.c_str());
        whisper_print_usage(argc, argv, params);
        exit(0);
    }

    // whisper init

    struct whisper_context * ctx = whisper_init_from_file(params.model.c_str());

    // print some info about the processing
    {
        fprintf(stderr, "\n");
        if (!whisper_is_multilingual(ctx)) {
            if (params.language != "en" || params.translate) {
                params.language = "en";
                params.translate = false;
                fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
            }
        }
        /*fprintf(stderr, "%s: processing, %d threads, lang = %s, task = %s, timestamps = %d ...\n",
                __func__,
                params.n_threads,
                params.language.c_str(),
                params.translate ? "translate" : "transcribe",
                params.no_timestamps ? 0 : 1);*/

        //fprintf(stderr, "\n");
    }

    // init audio

    audio_async audio(30*1000);
    if (!audio.init(params.capture_id, WHISPER_SAMPLE_RATE)) {
        fprintf(stderr, "%s: audio.init() failed!\n", __func__);
        return 1;
    }

    audio.resume();

    // wait for 1 second to avoid any buffered noise
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    audio.clear();

	int ret_val = process_command_list(ctx, audio, params);
	

    audio.pause();

    whisper_print_timings(ctx);
    whisper_free(ctx);

    return ret_val;
}
