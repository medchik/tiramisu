#include <cstring>
#include <chrono>
#include <iostream>
#include <math.h>
#include <numeric>
#include <fstream>
#include <time.h>
#include <string>
#include <iomanip>
#include "mkldnn.hpp"
#include "configure.h"

using namespace mkldnn;
using namespace std;

void lstm()
{
        auto cpu_engine = engine(engine::cpu, 0);
        auto null_memory_ = null_memory(cpu_engine);

        std::vector<primitive> lstm_net;
        std::vector<float> net_src(SEQ_LENGTH * BATCH_SIZE * FEATURE_SIZE);

        /* Initializing non-zero values for src */
        srand(1);
        for (size_t i = 0; i < net_src.size(); ++i)
                net_src[i] = rand() % 10;

        /* Encoder */

        memory::dims src_layer_tz = {SEQ_LENGTH, BATCH_SIZE, FEATURE_SIZE};
        memory::dims weights_layer_tz = {1, 1, FEATURE_SIZE, NUM_LAYERS, FEATURE_SIZE};
        memory::dims weights_iter_tz = {1, 1, FEATURE_SIZE, NUM_LAYERS, FEATURE_SIZE};
        memory::dims bias_tz = {1, 1, NUM_LAYERS, FEATURE_SIZE};
        memory::dims dst_layer_tz = {SEQ_LENGTH, BATCH_SIZE, FEATURE_SIZE};

        /* GNMT encoder: 1 bidirectional layer and 7 unidirectional layers  */

        std::vector<float> user_wei_layer(1 * 1 * 2 * FEATURE_SIZE * NUM_LAYERS * FEATURE_SIZE);
        std::vector<float> user_wei_iter(1 * 1 * FEATURE_SIZE * NUM_LAYERS * FEATURE_SIZE);
        std::vector<float> user_bias(1 * 1 * NUM_LAYERS * FEATURE_SIZE);

        /* Initializing non-zero values for weights and bias */
        srand(3);
        for (int i = 0; i < (int)user_wei_layer.size(); ++i)
                user_wei_layer[i] = 3;
        for (int i = 0; i < (int)user_wei_iter.size(); ++i)
                user_wei_iter[i] = 3;
        for (size_t i = 0; i < user_bias.size(); ++i)
                user_bias[i] = rand() % 5;

        // We create the memory descriptors used by the user
        auto user_src_layer_md = mkldnn::memory::desc(
            {src_layer_tz}, mkldnn::memory::data_type::f32,
            mkldnn::memory::format::tnc);

        auto user_wei_layer_md = mkldnn::memory::desc(
            {weights_layer_tz}, mkldnn::memory::data_type::f32,
            mkldnn::memory::format::ldigo);

        auto user_wei_iter_md = mkldnn::memory::desc(
            {weights_iter_tz}, mkldnn::memory::data_type::f32,
            mkldnn::memory::format::ldigo);

        auto user_bias_md = mkldnn::memory::desc({bias_tz},
                                                 mkldnn::memory::data_type::f32, mkldnn::memory::format::ldgo);

        auto dst_layer_md = mkldnn::memory::desc(
            {dst_layer_tz}, mkldnn::memory::data_type::f32,
            mkldnn::memory::format::tnc);

        /* We create memories */
        auto user_src_layer_memory = mkldnn::memory({user_src_layer_md, cpu_engine},
                                                    net_src.data());
        auto user_wei_layer_memory = mkldnn::memory({user_wei_layer_md, cpu_engine},
                                                    user_wei_layer.data());
        auto user_wei_iter_memory = mkldnn::memory({user_wei_iter_md, cpu_engine},
                                                   user_wei_iter.data());
        auto user_bias_memory = mkldnn::memory({user_bias_md, cpu_engine},
                                               user_bias.data());

        /// @todo fix this once cell desc is merged with rnn_desc
        rnn_cell::desc lstm_cell(algorithm::vanilla_lstm);
        rnn_forward::desc layer_desc(prop_kind::forward_inference, lstm_cell,
                                     rnn_direction::unidirectional_left2right, user_src_layer_md,
                                     zero_md(), user_wei_layer_md, user_wei_iter_md,
                                     user_bias_md, dst_layer_md, zero_md());

        auto prim_desc = mkldnn::rnn_forward::primitive_desc(layer_desc, cpu_engine);

        // there are currently no reorders
        /// @todo add a reorder when they will be available

        auto dst_layer_memory = mkldnn::memory(prim_desc.dst_layer_primitive_desc());

        lstm_net.push_back(
            rnn_forward(prim_desc, user_src_layer_memory,
                        null_memory_, user_wei_layer_memory,
                        user_wei_iter_memory, user_bias_memory,
                        dst_layer_memory, null_memory_, null_memory_));

        std::vector<std::chrono::duration<double, std::milli>> duration_vector;
        for (int i = 0; i < NB_TESTS; i++)
        {
                auto start1 = std::chrono::high_resolution_clock::now();
                stream(stream::kind::eager).submit(lstm_net).wait();
                auto end1 = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> duration = end1 - start1;
                duration_vector.push_back(duration);
        }
        std::cout << "\t\tMKL-DNN LSTM duration"
                  << ": " << median(duration_vector) << "; " << std::endl;

        printf("writing result in file\n");
        ofstream resultfile;
        resultfile.open("mkldnn_result.txt");

        float *output = (float *)dst_layer_memory.get_data_handle();
        for (size_t i = 0; i < SEQ_LENGTH; ++i)
                for (size_t j = 0; j < BATCH_SIZE; ++j)
                        for (size_t k = 0; k < FEATURE_SIZE; ++k)
                        {
                                resultfile << output[i * BATCH_SIZE * FEATURE_SIZE + j * FEATURE_SIZE + k];
                                //resultfile << fixed << setprecision(2) << (float)((int)(output[i * 64 * N * N + j * N * N + k * N + l] * 1000) / 1000.0);
                                resultfile << "\n";
                        }
        resultfile.close();
}

int main(int argc, char **argv)
{
        try
        {
                lstm();
                std::cout << "ok\n";
        }
        catch (error &e)
        {
                std::cerr << "status: " << e.status << std::endl;
                std::cerr << "message: " << e.message << std::endl;
                return 1;
        }
        return 0;
}