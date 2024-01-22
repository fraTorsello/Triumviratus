# ♘ **Triumviratus** ♘
Welcome to Triumviratus, an adept UCI (Universal Chess Interface) chess engine that leverages bitboard representation and sophisticated algorithms such as alpha-beta pruning with Negamax, coupled with a quiescence search.

The name *"Triumviratus"* is derived from the concept of a triumvirate, symbolizing a group of three individuals. In this context, it pays homage to the passion for chess that unites me and my two brothers (much like the classical concept of a triumvirate). As such, Triumviratus embodies the collaborative spirit and shared dedication of three minds harmoniously working together.
Embark on a strategic journey with Triumviratus, where advanced chess engine programming meets the collaborative spirit of a triumvirate, bound by the shared love for the intricacies of the game. Explore the culmination of collective expertise and synchronized efforts that bring this chess engine to life.

## Inspiration and Roots
Triumviratus draws significant inspiration from Codemonkeyking's YouTube video series, providing an in-depth exploration of the development of his BCC Chess Engine. The lessons and techniques distilled from this series have played a pivotal role in fostering a profound understanding of the fundamental aspects of chess engine programming.

# Key Features 🔑
### Bitboard Representation:
Harnessing the efficiency and speed of bitboards for chess board representation, Triumviratus adopts a cutting-edge approach to optimize both move generation and evaluation processes, achieving unparalleled performance in the realm of chess engines.
Bitboards, a compact and highly efficient data structure, empower Triumviratus to analyze chess positions with speed. Each chess piece type, as well as important game state information, is encoded using a set of bits, allowing for rapid operations. 

### Alpha-Beta Pruning with Negamax:
Triumviratus strategically utilizes the alpha-beta pruning algorithm, seamlessly integrated with the Negamax search algorithm. This synergistic combination enhances the engine's capacity to efficiently explore and evaluate potential moves, resulting in a significant boost to strategic decision-making.

### Quiescence Search:
Triumviratus ensures precise move selection through its quiescence search, a strategic exploration of variations in positions where the game state is relatively quiet. This approach not only leads to more reliable and robust gameplay but is also instrumental in mitigating the horizon effect.

### NNUE for Position Evaluation:
Triumviratus ventures beyond conventional methods by integrating NNUE (Efficiently Updatable Neural Network) for position evaluation, a cutting-edge technology that imparts a layer of sophistication to the engine's ability to assess and understand the complexities of chess positions. This state-of-the-art approach, akin to that employed by Stockfish, significantly refines Triumviratus' strategic prowess and elevates its overall performance in the dynamic landscape of chess.

### Move Ordering:
Implementing move ordering techniques to enhance the efficiency of the search algorithm and prioritize the most promising moves early in the search.

### PV Table (Principal Variation Table):
Triumviratus strategically employs a Principal Variation (PV) table to store and retrieve principal variations, enhancing the efficiency of its search algorithm by preventing redundant evaluations of the same position. This sophisticated mechanism ensures that the engine optimally navigates through the vast chess search space, avoiding unnecessary recalculations and contributing to faster and more streamlined decision-making.

### Null Move Pruning:
Triumviratus incorporates null move pruning as a strategic optimization in its search process. This technique expedites the search by efficiently identifying positions where making no move leads to a favorable outcome. By temporarily simulating a scenario where the side to move skips a turn, Triumviratus efficiently prunes branches of the search tree where such a null move does not lead to a significant disadvantage. This approach aids in accelerating the overall search process, enabling the engine to focus on more promising lines of play and improving its computational efficiency.

### Late Move Reduction:
Triumviratus employs late move reduction as a strategic enhancement in its search algorithm. This optimization involves reducing the search depth for less promising moves late in the search process. By intelligently scaling back the exploration of certain branches, Triumviratus streamlines its search, allocating more computational resources to promising lines of play. This approach contributes to a more efficient search, allowing the engine to prioritize the analysis of potentially impactful moves and enhancing its overall decision-making capabilities.

### Transposition Table with Zobrist Hashing:
Triumviratus enhances search efficiency by implementing a transposition table. This table stores previously evaluated positions utilizing Zobrist hashing, a technique that minimizes redundancy in evaluations. Zobrist hashing assigns unique keys to chess positions, allowing Triumviratus to quickly identify and retrieve information from the transposition table. By avoiding redundant evaluations of positions, this approach significantly improves overall performance, making the engine more adept at navigating complex chess scenarios with greater speed and precision.

### Aspiration Window:
Triumviratus implements an aspiration window as a strategic tool in its search process. This optimization involves focusing the search on a narrow range of evaluation values, allowing the engine to explore variations within a specific target.

## Future Roadmap 🚀
Triumviratus is an evolving project with future updates planned to include the implementation of multithreading. This enhancement will enable the engine to harness the power of parallel processing, significantly improving its performance and speed.

## Contribute
Contributions and feedback from the chess and open-source community are highly encouraged. Whether you are a chess enthusiast, developer, or both, your input is valuable in shaping the future of Triumviratus.

## Getting Started
To start using Triumviratus, you have two convenient options:

#### Compile from Visual Studio:
If you prefer to build the engine from source, follow the instructions in the provided Visual Studio project. Ensure you have the necessary dependencies installed, open the project in Visual Studio, and compile the code to generate the executable. This option gives you the flexibility to customize and modify Triumviratus according to your preferences.

#### Use the Precompiled Executable (.exe):
For a quick start, download the precompiled executable (.exe) from the releases section. This ready-to-use version allows you to run Triumviratus without the need for compilation. It's a hassle-free way to get started and experience the chess engine in action. Happy chess playing!

## References:

Code Monkey King's YouTube Channel: [Code Monkey King](https://www.youtube.com/channel/UClA-jNuyJKqN-xCm7KPG_XA)
Code Monkey King's Chess Engine Series: [BCC Chess Engine](https://www.youtube.com/channel/UCB9-prLkPwgvlKKqDgXhsMQ)
Neural Network Architecture from David Miller: [NNUE](http://www.millermattson.com/dave/)




