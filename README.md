# Toynet

Basic implementation of a neural network and stochastic gradient descent.

Configure
```bash
$ cmake -S . -B build/ -D CMAKE_BUILD_TYPE=Debug
```

Build the binaries
```bash
$ cmake --build build/
```

Run all tests
```bash
$ ./build/test_main
```

Run MNIST examples
```bash
$ ./build/examples/train_mnist
$ ./build/examples/eval_mnist
```