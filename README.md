<p align="center">
  <img src="/TRIUMVIRATUS.png" alt="Triumviratus Logo" width="350" height="350">
</p>


# ♔ **Triumviratus** ♔
*Triumviratus 2.0* is a sophisticated UCI chess engine that employs advanced techniques, including bitboard representation and powerful algorithms such as alpha-beta pruning with Negamax, complemented by a quiescence search. The engine has evolved from its previous version, *Triumviratus 1.0*, which drew inspiration from "*Vice" by Bluefever*. Notably, version 1.0 lacked the bitboard representation that is now a key feature in the upgraded *Triumviratus 2.0*, enhancing its overall efficiency and capabilities in chess analysis and gameplay.   

The name *"Triumviratus"* is derived from the concept of a triumvirate, symbolizing a group of three individuals. In this context, it pays homage to the passion for chess that unites me and my two brothers (much like the classical concept of a triumvirate). As such, *Triumviratus* embodies the collaborative spirit and shared dedication of three minds harmoniously working together.
Embark on a strategic journey with *Triumviratus*, where advanced chess engine programming meets the collaborative spirit of a triumvirate, bound by the shared love for the intricacies of the game. Explore the culmination of collective expertise and synchronized efforts that bring this chess engine to life.

# 💪🏻 Playing Strength
As of January 2024, Triumviratus 2.0 continues to demonstrate its remarkable strength by consistently defeating bots rated approximately 2800 on Chess.Com with a few seconds of deliberation. This exceptional capability positions Triumviratus 2.0 as a formidable and challenging opponent for any human player. Its strategic acumen and rapid decision-making make it a worthy adversary in the realm of chess, providing an engaging and competitive experience for those seeking to test their skills against advanced artificial intelligence.  

The engine has achieved the impressive feat of defeating all Magnus Carlsen bots featured on Chess.com. Notably, one of these bots boasts a rating that should be of 2900 at least, further highlighting Triumviratus 2.0's capabilities.

## 🌱 Inspiration and Roots
*Triumviratus* draws significant inspiration from Codemonkeyking's YouTube video series, which offers a comprehensive exploration of the development of his BCC Chess Engine. The invaluable lessons and techniques distilled from this series have played a pivotal role in cultivating a profound understanding of the fundamental aspects of chess engine programming. Additionally, the fundamentals of chess engine programming have been further enriched through the examination of the development of "Vice" by Bluefever Software and their accompanying YouTube video series. The insights gained from studying "Vice" have provided valuable perspectives, contributing to the refinement and enhancement of Triumviratus.

Special thanks to Chessprogramming Wiki for providing clear and insightful explanations that have greatly assisted in understanding various aspects of chess engines development.

It's worth noting that certain aspects of the code, particularly the UCI (Universal Chess Interface) communication part and the implementation of *get_time_ms()*, have been forked from the codebase of **Vice**. This collaborative approach not only acknowledges the influence of Bluefever Software's work but also highlights the cooperative nature of chess engine development, where ideas and implementations are shared and built upon within the community.

# 📈 Project Status

## 🔑 Key Features 
- **Bitboard Representation:**
  Harnessing the efficiency and speed of bitboards for chess board representation, *Triumviratus* adopts a cutting-edge approach to optimize both move generation and evaluation processes, achieving unparalleled performance in the realm of chess engines. Bitboards, a compact and highly efficient data structure, empower Triumviratus to analyze chess positions with speed. Each chess piece type, as well as important game state information, is encoded using a set of bits, allowing for rapid operations.

- **Alpha-Beta Pruning with Negamax:**
  *Triumviratus* strategically utilizes the alpha-beta pruning algorithm, seamlessly integrated with the Negamax search algorithm. This synergistic combination enhances the engine's capacity to efficiently explore and evaluate potential moves, resulting in a significant boost to strategic decision-making.

- **Quiescence Search:**
  *Triumviratus* ensures precise move selection through its quiescence search, a strategic exploration of variations in positions where the game state is relatively quiet. This approach not only leads to more reliable and robust gameplay but is also instrumental in mitigating the horizon effect.

- **NNUE for Position Evaluation:**
  *Triumviratus* ventures beyond conventional methods by integrating NNUE (Efficiently Updatable Neural Network) for position evaluation, a cutting-edge technology that imparts a layer of sophistication to the engine's ability to assess and understand the complexities of chess positions. This state-of-the-art approach, akin to that employed by Stockfish, significantly refines Triumviratus' strategic prowess and elevates its overall performance in the dynamic landscape of chess.

- **Move Ordering:**
  Implementing move ordering techniques to enhance the efficiency of the search algorithm and prioritize the most promising moves early in the search.

- **PV Table (Principal Variation Table):**
  *Triumviratus* strategically employs a Principal Variation (PV) table to store and retrieve principal variations, enhancing the efficiency of its search algorithm by preventing redundant evaluations of the same position. This sophisticated mechanism ensures that the engine optimally navigates through the vast chess search space, avoiding unnecessary recalculations and contributing to faster and more streamlined decision-making.

- **Null Move Pruning:**
  *Triumviratus* incorporates null move pruning as a strategic optimization in its search process. This technique expedites the search by efficiently identifying positions where making no move leads to a favorable outcome. By temporarily simulating a scenario where the side to move skips a turn, Triumviratus efficiently prunes branches of the search tree where such a null move does not lead to a significant disadvantage. This approach aids in accelerating the overall search process, enabling the engine to focus on more promising lines of play and improving its computational efficiency.

- **Late Move Reduction:**
  *Triumviratus* employs late move reduction as a strategic enhancement in its search algorithm. This optimization involves reducing the search depth for less promising moves late in the search process. By intelligently scaling back the exploration of certain branches, Triumviratus streamlines its search, allocating more computational resources to promising lines of play. This approach contributes to a more efficient search, allowing the engine to prioritize the analysis of potentially impactful moves and enhancing its overall decision-making capabilities.

- **Transposition Table with Zobrist Hashing:**
  *Triumviratus* enhances search efficiency by implementing a transposition table. This table stores previously evaluated positions utilizing Zobrist hashing, a technique that minimizes redundancy in evaluations. Zobrist hashing assigns unique keys to chess positions, allowing Triumviratus to quickly identify and retrieve information from the transposition table. By avoiding redundant evaluations of positions, this approach significantly improves overall performance, making the engine more adept at navigating complex chess scenarios with greater speed and precision.

- **Aspiration Window:**
  *Triumviratus* implements an aspiration window as a strategic tool in its search process. This optimization involves focusing the search on a narrow range of evaluation values, allowing the engine to explore variations within a specific target.

## 🪄 Magic Bitboard Move Generation:

Triumviratus incorporates magic bitboards into its move generation process, enhancing the efficiency of move generation. Magic bitboards are a sophisticated technique that involves using precomputed "magic" numbers to efficiently generate sliding piece moves (such as those of bishops and rooks) on a chess board. By leveraging this approach, Triumviratus accelerates the generation of legal moves, contributing to an overall improvement in the engine's speed and responsiveness during the search process. This optimized move generation allows the engine to explore a broader range of candidate moves within the available time frame, enhancing its decision-making capabilities and tactical awareness.

# 🚀 Future Roadmap 
*Triumviratus* is an evolving project with future updates planned to include the implementation of multithreading. This enhancement will enable the engine to harness the power of parallel processing, significantly improving its performance and speed.

# 🎉 Contribute
Contributions and feedback from the chess and open-source community are highly encouraged. Whether you are a chess enthusiast, developer, or both, your input is valuable in shaping the future of Triumviratus.

# 🔧 Getting Started
To start using *Triumviratus*, you have two convenient options:

-  **Compile from Visual Studio**   
If you prefer to build the engine from source, follow the instructions in the provided Visual Studio project. Ensure you have the necessary dependencies installed, open the project in Visual Studio, and compile the code to generate the executable. This option gives you the flexibility to customize and modify Triumviratus according to your preferences.

- **Use the Precompiled Executable (.exe)**    
For a quick start, download the precompiled executable (.exe) from the releases section. This ready-to-use version allows you to run Triumviratus without the need for compilation. It's a hassle-free way to get started and experience the chess engine in action. Happy chess playing!

# 📜 References:

ChessProgrammingWiki: [Wiki](https://www.chessprogramming.org/UCI)    
Code Monkey King's YouTube Channel: [Code Monkey King](https://www.youtube.com/channel/UClA-jNuyJKqN-xCm7KPG_XA)    
Code Monkey King's Chess Engine Series: [BCC Chess Engine](https://www.youtube.com/channel/UCB9-prLkPwgvlKKqDgXhsMQ)     
Neural Network Architecture from David Miller: [NNUE](http://www.millermattson.com/dave/)    
Vice Chess Engine by Bluefever: [VICE](https://github.com/bluefeversoft/vice)   

# 🧪 Perft Test Results

Perft test results, single-core *Ryzen 7 6800H* processor running *Windows 11*. *(23/01/2024)*  

| Depth | Nodes           | Time     |
|:-----:|:---------------:|:--------:|
|   4   |     197,281     |    16    |
|   5   |   4,865,609     |   156    |
|   6   | 119,060,324     | 3,766    |
|   7   |3,195,901,860    |107,328   |
 

# 🔎 Search results at Test Positions

Search results, single-core *Ryzen 7 6800H* processor running *Windows 11*. *(23/01/2024)*  
Positions: [Test](https://www.chessprogramming.org/The_Nolot_Suite)

### **Pos 1**:    
r3qb1k/1b4p1/p2pr2p/3n4/Pnp1N1N1/6RP/1B3PP1/1B1QR1K1 w - -    *bm Nxh6*  

| Depth | Moves                                             | Time |
|:-----:|:-------------------------------------------------:|:----:|
|   1   | Move a4a5, Time 0                                 |   0  |
|   2   | Move g4h6 b4d3 b1d3, Time 0                       |   0  |
|   3   | Move g4h6 b4d3 b1d3 c4d3, Time 0                  |   0  |
|   6   | Move b2a1 c4c3 g4h6 d5f4 a1c3 b7e4, Time 31       |  31  |
|   8   | Move g4h6 b4d3 h6f5 d3b2 d1d4 d5f6 e4f6 e6e1, Time 250 |  250  |
|  10   | Move g4h6 c4c3 b2a1 c3c2 b1c2 b4c2 d1c2 e6h6 c2b1 e8f7, Time 969 |  969  |
|  12   | Move g4h6 c4c3 b2a1 c3c2 d1g4 e6e7 b1c2 b4c2 h6f5 c2e1 f5e7 e8e7, Time 3484 |  3484  |

**Best Move:**   
- Move: d2d4  


### **Pos 3**:  
r2qk2r/ppp1b1pp/2n1p3/3pP1n1/3P2b1/2PB1NN1/PP4PP/R1BQK2R w KQkq - *bm Nxg5*    

| Depth | Moves                                             | Time |
|:-----:|:-------------------------------------------------:|:----:|
|   1   | Move c1e3, Time 0                                 |   0  |
|   2   | Move e1g1 g5f3, Time 0                             |   0  |
|   4   | Move c1g5 e7g5 h2h3 g5f4, Time 0                   |   0  |
|   5   | Move c1g5 e7g5 h2h3 g5f4 e1g1, Time 0              |   0  |
|   6   | Move c1g5 e7g5 h2h3 g4f3 d1f3 d8e7, Time 16       |  16  |
|   8   | Move c1g5 e7g5 h2h3 g4f3 d1f3 d8e7 f3h5 e8d7 e1g1, Time 31 |  31  |
|  10   | Move c1g5 e7g5 e1g1 d8d7 h2h3 g4f3 d1f3 e8c8 g3h5 h7h6, Time 469 |  469  |
|  12   | Move f3g5 g4d1 g5e6 d8b8 e6g7 e8d8 e1d1 h8f8 d3h7 a7a5 g3f5 d8d7 h7g6, Time 1875 |  1875  |
|  14   | Move f3g5 g4d1 g5e6 d8c8 e6g7 e8f7 d3f5 c8b8 c1h6 e7f8 e5e6 f7f6 e1g1 f8g7 f5d3 f6e6 h6g7, Time 6172 |  6172  |

**Best Move:**    
- Move: f3g5  

# 🗨️ UCI usage and documentation

The usage and documentation for the UCI (Universal Chess Interface) can be found on this website [UCI](https://www.wbec-ridderkerk.nl/html/UCIProtocol.html), providing comprehensive information on how to implement and utilize this standardized protocol for communication between chess engines and graphical user interfaces.


