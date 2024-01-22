# Triumviratus
Welcome to Triumviratus, an adept UCI (Universal Chess Interface) chess engine that leverages bitboard representation and sophisticated algorithms such as alpha-beta pruning with Negamax, coupled with a quiescence search.

The name "Triumviratus" is derived from the concept of a triumvirate, symbolizing a group of three individuals. In this context, it pays homage to the passion for chess that unites me and my two brothers (much like the classical concept of a triumvirate). As such, Triumviratus embodies the collaborative spirit and shared dedication of three minds harmoniously working together.
Embark on a strategic journey with Triumviratus, where advanced chess engine programming meets the collaborative spirit of a triumvirate, bound by the shared love for the intricacies of the game. Explore the culmination of collective expertise and synchronized efforts that bring this chess engine to life.

## Inspiration and Roots
Triumviratus draws inspiration from Codemonkeyking's YouTube video series detailing the development of his BCC Chess Engine. The lessons and techniques gleaned from this series have been instrumental in fostering a deep understanding of the fundamentals of chess engine programming.


# Key Features
### Bitboard Representation:
Harness the efficiency and speed of bitboards for chess board representation, allowing Triumviratus to optimize move generation and evaluation processes with unparalleled performance.

### Alpha-Beta Pruning with Negamax:
Triumviratus employs the alpha-beta pruning algorithm in conjunction with the Negamax search algorithm, elevating the engine's capability to efficiently explore and evaluate potential moves, enhancing strategic decision-making.

### Quiescence Search:
Ensure precise move selection with Triumviratus's quiescence search, which strategically explores variations in positions where the game state is relatively quiet, leading to more reliable and robust gameplay.

### NNUE for Position Evaluation:
Triumviratus goes beyond conventional methods by incorporating NNUE (Efficiently Updatable Neural Network) for position evaluation. This cutting-edge technology adds a layer of sophistication to the engine's ability to assess and understand the complexities of chess positions, further refining its strategic prowess.

### Move Ordering:
Implementing move ordering techniques to enhance the efficiency of the search algorithm and prioritize the most promising moves early in the search.

### PV Table (Principal Variation Table):
Utilizing a PV table to store and retrieve principal variations, enhancing the efficiency of the search algorithm by avoiding redundant evaluations of the same position.

### Null Move Pruning:
Incorporating null move pruning to expedite the search process by quickly identifying positions where making no move leads to a favorable outcome.

### Late Move Reduction:
Applying late move reduction to optimize the search algorithm further by reducing the search depth for less promising moves late in the search.

### Transposition Table with Zobrist Hashing:
Enhancing search efficiency through a transposition table that stores previously evaluated positions using Zobrist hashing, reducing redundant evaluations and improving overall performance.

### Aspiration Window:
Implementing an aspiration window to focus the search on a narrow range of evaluation values, optimizing the search process for more efficient results.


## Future Roadmap
Triumviratus is an evolving project with future updates planned to include the implementation of multithreading. This enhancement will enable the engine to harness the power of parallel processing, significantly improving its performance and speed.

## Contribute
Contributions and feedback from the chess and open-source community are highly encouraged. Whether you are a chess enthusiast, developer, or both, your input is valuable in shaping the future of Triumviratus.

## Getting Started
To start using Triumviratus, you have two convenient options:

Compile from Visual Studio:
If you prefer to build the engine from source, follow the instructions in the provided Visual Studio project. Ensure you have the necessary dependencies installed, open the project in Visual Studio, and compile the code to generate the executable. This option gives you the flexibility to customize and modify Triumviratus according to your preferences.

Use the Precompiled Executable (.exe):
For a quick start, download the precompiled executable (.exe) from the releases section. This ready-to-use version allows you to run Triumviratus without the need for compilation. It's a hassle-free way to get started and experience the chess engine in action. Happy chess playing!

## References:

A bitboard-based chess engine guided by Code Monkey King:

Code Monkey King's YouTube Channel: [Code Monkey King](https://www.youtube.com/channel/UClA-jNuyJKqN-xCm7KPG_XA)
Code Monkey King's Chess Engine Series: [BCC Chess Engine](https://www.youtube.com/channel/UCB9-prLkPwgvlKKqDgXhsMQ)
Neural Network Architecture from David Miller: [NNUE](http://www.millermattson.com/dave/)




