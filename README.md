# Branch Predictor

Simulates the Alpha 21264 tournament branch predictor and a custom perceptron + gshare branch predictor in C. 

## Design and Limitations

The tournament branch predictor compares two branch predictions, one trained on local history and the other trained on global history, and then chooses which prediction to use. The chooser itself is also a predictor that is trained alongside the local and global predictors. Each predictor uses 2-bit saturating counters.

The custom perceptron + gshare branch predictor is a tournament between perceptron and gshare, where again a chooser predicts which predictor to use. Perceptron keeps track of a set of weights for each program counter value and computes the dot product between the weights and the global history, where a 1 in the global history causes the weight to be added and a 0 in the global history causes the weight to be subtracted. The final prediction is then 1 if the value is positive and 0 if it's negative. These weights are trained according to section 3.3 of https://www.cs.utexas.edu/~lin/papers/hpca01.pdf.

The hardware limitation used in the design of these branch predictors is 32 kilobits for branch predictor data structures, like pattern history tables or tables of weights, and 320 bits for registers.

The base Gshare implementation and testing framework were provided by https://github.com/gdpande97.

## How to Run

In the `src` directory, run `make`. Then run `bunzip2 -kc <path to trace> | ./predictor --<predictor type>`. Replace `<path to trace>` and `<predictor type>` with the path to the desired trace (see traces folder) and either `--gshare`, `--tournament`, or `--custom` respectively.

## Results

The custom predictor outperforms both Gshare and the Alpha 21264 branch predictor in all traces.
