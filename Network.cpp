//
// Created by Nick Yu on 12/09/2024.
//

#include "Network.h"

#include <random>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

template<typename T>
T sigmoid(const T &z) {
    return 1.0 / (1.0 + exp(-z));
}

std::valarray<double> sigmoid_derivative(const std::valarray<double> &z) {
    auto s = sigmoid(z);
    return s * (1.0 - s);
}

const std::valarray<double> &toynet::Layer::feedforward(const std::valarray<double> &input) {
    assert(input.size() == this->m);

    for (size_t i = 0; i < n; i++) {
        z[i] = this->bias[i];

        for (size_t j = 0; j < this->m; j++) {
            z[i] += this->weights[i * m + j] * input[j];
        }

        // TODO: functionality for alternative activation functions
        activation[i] = sigmoid(z[i]);
    }

    return this->activation;
}

toynet::Layer::Layer(toynet::ActFunc a, size_t n, size_t m) : func(a), n(n), m(m), bias(n), weights(n * m),
                                                              activation(n), z(n), dC_db(n), dC_dw(n * m) {
    // XXX: Gaussian distribution for now
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<double> d{0.0, 1.0};

    auto randf = [&d, &gen] { return d(gen); };

    for (size_t i = 0; i < n; i++) {
        bias[i] = randf();

        for (size_t j = 0; j < m; j++) {
            weights[i * n + j] = randf();
        }
    }
}

std::string toynet::valarray_to_json(const std::valarray<double> &a) {
    std::ostringstream strm;
    strm << "[";
    for (size_t i = 0; i < a.size(); ++i) {
        if (i > 0) {
            strm << ", ";
        }
        strm << a[i];
    }
    strm << "]";
    return strm.str();
}

std::ostream &operator<<(std::ostream &strm, const toynet::Layer &l) {
    strm << "{\n"
         << "  \"n\": " << l.n << ",\n"
         << "  \"m\": " << l.m << ",\n"
         << "  \"weights\": " << toynet::valarray_to_json(l.weights) << ",\n"
         << "  \"bias\": " << toynet::valarray_to_json(l.bias) << ",\n"
         << "  \"activation\": " << toynet::valarray_to_json(l.activation) << ",\n"
         << "  \"z\": " << toynet::valarray_to_json(l.z) << ",\n"
         << "  \"dC_dw\": " << toynet::valarray_to_json(l.dC_dw) << ",\n"
         << "  \"dC_db\": " << toynet::valarray_to_json(l.dC_db) << "\n"
         << "}";

    return strm;
}

const std::valarray<double> &toynet::Network::feedforward(const std::valarray<double> &input) {
    // Set input layer activation
    layers[0].activation = input;

    for (size_t l = 1; l < layers.size(); l++) {
        layers[l].feedforward(layers[l - 1].activation);
    }

    return layers.back().activation;
}

toynet::Network::Network(std::vector<unsigned int> sizes) {
    // Input layer
    layers.push_back(std::move(Layer(Sigmoid, sizes[0], 1)));

    for (size_t i = 0; i < sizes.size() - 1; i++) {
        unsigned int m = sizes[i];
        unsigned int n = sizes[i + 1];

        Layer l(Sigmoid, n, m);
        layers.push_back(std::move(l));
    }
}

void
toynet::Network::SGD(std::vector<TrainingSample> training_data, unsigned int epochs, unsigned int mini_batch_size,
                     double eta) {
    std::default_random_engine rng{};
    std::shuffle(training_data.begin(), training_data.end(), rng);

    auto it = training_data.begin();

}

void toynet::Network::update_mini_batch(std::vector<TrainingSample>::iterator mini_batch_begin,
                                        std::vector<TrainingSample>::iterator mini_batch_end, double eta) {

    std::vector<std::valarray<double>> dC_db;
    std::vector<std::valarray<double>> dC_dw;

    dC_dw.reserve(this->layers.size());
    dC_db.reserve(this->layers.size());

    for (const auto &l : this->layers) {
        dC_dw.push_back(std::move(std::valarray<double>(l.n * l.m)));
        dC_db.push_back(std::move(std::valarray<double>(l.n)));
    }

    for (auto sample = mini_batch_begin; sample != mini_batch_end; sample++) {
        // Calculate error and for this training sample
        backpropogate_and_update(*sample);

        // Sum cost function gradients
        for (size_t i = 0; i < layers.size(); ++i) {
            dC_dw[i] += layers[i].dC_dw;
            dC_db[i] += layers[i].dC_db;
        }
    }

    auto m = static_cast<double>(std::distance(mini_batch_begin, mini_batch_end));

    // update weights and biases
    for (int i = 0; i < layers.size(); ++i) {
        layers[i].weights -= (eta / m) * dC_dw[i];
        layers[i].bias -= (eta / m) * dC_db[i];
    }
}

void toynet::Network::backpropogate_and_update(const toynet::TrainingSample &sample) {
    this->feedforward(sample.first);

    // Calculate the error for the last layer
    std::valarray<double> cost_derivative = this->layers.back().activation - sample.second;

    // compute delta = C_gradient hadamard sigmoid_prime
    std::valarray<double> delta = cost_derivative * sigmoid_derivative(this->layers.back().z);

    this->layers.back().dC_db = delta;
    this->layers.back().dC_dw = this->layers[this->layers.size() - 2].activation * delta;

    // propagate the error backwards
    for (size_t l_i = this->layers.size() - 2; l_i > 0; l_i--) {
        Layer &l = this->layers[l_i];
        Layer &l_next = this->layers[l_i + 1];

        std::valarray<double> t(l.z.size());

        for (size_t row = 0; row < l_next.m; row++) {
            for (size_t col = 0; col < l_next.n; col++) {
                // transposed matrix mul
                t[col] += l_next.weights[row + col * l_next.n] * delta[col];
            }
        }

        // hadamard
        t *= sigmoid_derivative(l.activation);

        delta = t;

        l.dC_db = delta;
        l.dC_dw = this->layers[l_i - 1].activation * delta;
    }
}

void toynet::Network::load_parameters(const std::string &json_state) {
    auto layer_parameters = nlohmann::json::parse(json_state).get<std::vector<nlohmann::json>>();

    // Example for a network of {2, 2, 2}
    //  [
    //      {"weights": [1, 2, 1, 0], "bias": [1, 0]},
    //      {"weights": [2, 2, 0, 1], "bias": [0, 3]}
    //  ]
    for (size_t l = 0; l < layer_parameters.size(); ++l) {
        auto &p = layer_parameters[l];
        auto w = p["weights"].get<std::vector<double>>();
        if (w.size() != this->layers[l + 1].n * this->layers[l + 1].m) {
            throw std::runtime_error("Given weights matrix does not match layer size");
        }
        this->layers[l + 1].weights = std::move(std::valarray<double>(w.data(), w.size()));

        auto b = p["bias"].get<std::vector<double>>();
        if (b.size() != this->layers[l + 1].n) {
            throw std::runtime_error("Given bias vector does not match layer size");
        }
        this->layers[l + 1].bias = std::move(std::valarray<double>(b.data(), b.size()));
    }
}