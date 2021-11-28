#include <iostream>
#include <array>
#include <optional>
#include <algorithm>
#include <iomanip>
#include <conio.h>
#include <cassert>
#include <vector>
#include <Windows.h>

using namespace std;


HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

enum ConsoleColor {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    BROWN = 6,
    LIGHTGRAY = 7,
    DARKGRAY = 8,
    LIGHTBLUE = 9,
    LIGHTGREEN = 10,
    LIGHTCYAN = 11,
    LIGHTRED = 12,
    LIGHTMAGENTA = 13,
    YELLOW = 14,
    WHITE = 15
};

const ConsoleColor DEFAULT_TEXT_COLOR = LIGHTGRAY;

void ColorConsolePrint(char ch, ConsoleColor color = DEFAULT_TEXT_COLOR) {
    SetConsoleTextAttribute(hConsole, (WORD) ((0 << 4) | color));
    cout << ch;
    SetConsoleTextAttribute(hConsole, (WORD) ((0 << 4) | DEFAULT_TEXT_COLOR));
}

void ColorConsolePrint(const string& s, ConsoleColor color = DEFAULT_TEXT_COLOR) {
    SetConsoleTextAttribute(hConsole, (WORD) ((0 << 4) | color));
    cout << s;
    SetConsoleTextAttribute(hConsole, (WORD) ((0 << 4) | DEFAULT_TEXT_COLOR));
}

const int BOARD_SZ = 15;
const int N_CELLS_TO_WIN = 5;

enum class CellType {
    Empty,
    Cross,
    Nought,
};

class Board {
public:
    static const int VICTORY_EVALUATION = INT_MAX / 2;
    static const int EVAL_EXP = 80;
    static constexpr const array<int, N_CELLS_TO_WIN + 1> LINE_FULLNESS_EVALUATION = {
            0, 1, EVAL_EXP, EVAL_EXP * EVAL_EXP,
            EVAL_EXP * EVAL_EXP * EVAL_EXP, VICTORY_EVALUATION};

    // Проверка корректности весов (4 - кол-во направлений линий)
    static_assert(VICTORY_EVALUATION / 2 > 4 * BOARD_SZ * BOARD_SZ * LINE_FULLNESS_EVALUATION[N_CELLS_TO_WIN - 1]);
//    static_assert(static_cast<long long>(VICTORY_EVALUATION) +
//                  static_cast<long long>(4 * BOARD_SZ * BOARD_SZ * LINE_FULLNESS_EVALUATION[N_CELLS_TO_WIN - 1])
//                  <= INT_MAX);

    static const int MAX_DEPTH = 4;
    // При переборе ходов будут учитываться K_VARIANTS_TO_TRY лучших
    // с точки зрения оценки доски после хода
    static const int K_VARIANTS_TO_TRY = 120;

    using Pos = pair<int, int>;

    static bool ValidPos(Pos pos) {
        return 0 <= pos.first && pos.first < BOARD_SZ
               && 0 <= pos.second && pos.second < BOARD_SZ;
    }

    bool ValidMove(Pos pos) {
        return ValidPos(pos) && cells_[pos.first][pos.second] == CellType::Empty;
    }

    enum class State {
        GameIsNotOver,
        Draw,
        CrossesWon,
        NoughtsWon,
    };

    // Чей сейчас ход
    bool CrossesMove() const {
        return !(moves_history_.size() & 1);
    }

    State GetState() const {
        if (evaluation_for_crosses_ > VICTORY_EVALUATION / 2) {
            return State::CrossesWon;
        } else if (evaluation_for_crosses_ < -(VICTORY_EVALUATION / 2)) {
            return State::NoughtsWon;
        } else if (moves_history_.size() == BOARD_SZ * BOARD_SZ) {
            return State::Draw;
        } else {
            return State::GameIsNotOver;
        }
    }

    // После хода мы перерасчитываем оценку доски
    void MakeMove(Pos cell) {
        UpdateCell(cell, CrossesMove() ? CellType::Cross : CellType::Nought);
        moves_history_.push_back(cell);
    }

    // Возвращает отмененный ход
    Pos UndoMove() {
        assert(!moves_history_.empty());
        UpdateCell(moves_history_.back(), CellType::Empty);
        Pos result = moves_history_.back();
        moves_history_.pop_back();
        return result;
    }

    vector<Pos> GetMovesHistory() const {
        return moves_history_;
    }

    // Оценивает позицию для крестиков либо для ноликов
    long long Evaluate(bool crosses) const {
        return (crosses ? 1 : -1) * evaluation_for_crosses_;
    }

    pair<optional<Pos>, long long> FindBestMove(int depth = 1, long long alpha = -INT_MAX, long long betta = INT_MAX) {
        // Кто сейчас ходит
        bool crosses = CrossesMove();
        if (GetState() != State::GameIsNotOver) {
            return {nullopt, Evaluate(crosses)};
        }
        pair<optional<Pos>, long long> result;
        vector<pair<long long, Pos>> moves_and_evals;
        for (int row = 0; row < BOARD_SZ; ++row) {
            for (int col = 0; col < BOARD_SZ; ++col) {
                if (cells_[row][col] == CellType::Empty) {
                    MakeMove({row, col});
                    // moves_and_evals.emplace_back(Pos{row, col}, Evaluate(cross));
                    moves_and_evals.emplace_back(Evaluate(crosses), Pos{row, col});
                    UndoMove();
                }
            }
        }
        sort(rbegin(moves_and_evals), rend(moves_and_evals));
        for (int i = 0; i < moves_and_evals.size() && i < K_VARIANTS_TO_TRY; ++i) {
            auto [move_eval, move] = moves_and_evals[i];
            if (depth != MAX_DEPTH) {
                MakeMove(move);
                move_eval = -FindBestMove(depth + 1, -betta, -alpha).second;
                UndoMove();
            }
            if (!result.first || move_eval > result.second) {
                result = {move, move_eval};
            }
            alpha = max(alpha, result.second);
            if (alpha > betta) {
                return result;
            }
        }
        assert(result.first);
        return result;
    }

    void PrintToConsole() const {
        const int cell_print_sz = 3;
        const string col_numbers = [cell_print_sz]() {
            stringstream col_numbers_ss;
            col_numbers_ss << string(cell_print_sz, ' ') << " ";
            for (int col = 1; col <= BOARD_SZ; ++col) {
                col_numbers_ss << setw(cell_print_sz) << col << " ";
            }
            return col_numbers_ss.str();
        }();
        cout << col_numbers << endl;
        const string row_delimiter = string(cell_print_sz, ' ')
                + string(BOARD_SZ * (cell_print_sz + 1) + 1, '-');
        cout << row_delimiter << endl;
        for (int row = 0; row < BOARD_SZ; ++row) {
            cout << setw(cell_print_sz - 1) << row + 1 << " | ";
            for (int col = 0; col < BOARD_SZ; ++col) {
                string cell_value;
                ConsoleColor color;
                if (cells_[row][col] == CellType::Cross) {
                    cell_value = "X";
                    color = Pos{row, col} == moves_history_.back() ?
                            ConsoleColor::LIGHTRED : ConsoleColor::RED;
                } else if (cells_[row][col] == CellType::Nought){
                    cell_value = "O";
                    color = Pos{row, col} == moves_history_.back() ?
                            ConsoleColor::LIGHTBLUE : ConsoleColor::BLUE;
                } else {
                    cell_value = " ";
                    color = DEFAULT_TEXT_COLOR;
                }
                ColorConsolePrint(cell_value, color);
                cout << " | ";
            }
            cout << left << setw(cell_print_sz) << row + 1 << right << endl;
            cout << row_delimiter << endl;
        }
        cout << col_numbers << endl;
    }

private:
    void UpdateCell(Pos cell, CellType new_value) {
        evaluation_for_crosses_ -= EvaluateLinesThroughCell(cell);
        cells_[cell.first][cell.second] = new_value;
        evaluation_for_crosses_ += EvaluateLinesThroughCell(cell);
    }

    long long EvaluateLinesThroughCell(Pos cell) const {
        long long result = 0;
        static const array<pair<int, int>, 4> directions = {pair<int, int>{-1, 1},
                                                            pair<int, int>{0, 1},
                                                            pair<int, int>{1, 1},
                                                            pair<int, int>{1, 0}};

        for (auto [d_row, d_col] : directions) {
            int line_sz = 0, crosses_cnt = 0, noughts_cnt = 0;
            for (int shift = -N_CELLS_TO_WIN + 1; shift <= N_CELLS_TO_WIN - 1; ++shift) {
                int row = cell.first + d_row * shift;
                int col = cell.second + d_col * shift;
                if (ValidPos({row, col})) {
                    ++line_sz;
                    if (cells_[row][col] == CellType::Cross) {
                        ++crosses_cnt;
                    } else if (cells_[row][col] == CellType::Nought){
                        ++noughts_cnt;
                    }
                }
                if (line_sz > N_CELLS_TO_WIN) {
                    int row_ = row - d_row * N_CELLS_TO_WIN;
                    int col_ = col - d_col * N_CELLS_TO_WIN;
                    if (cells_[row_][col_] == CellType::Cross) {
                        --crosses_cnt;
                    } else if (cells_[row_][col_] == CellType::Nought){
                        --noughts_cnt;
                    }
                    --line_sz;
                }
                if (line_sz == N_CELLS_TO_WIN) {
                    if (crosses_cnt && !noughts_cnt) {
                        result += LINE_FULLNESS_EVALUATION[crosses_cnt];
                    } else if (noughts_cnt && !crosses_cnt){
                        result -= LINE_FULLNESS_EVALUATION[noughts_cnt];
                    }
                }
            }
        }
        return result;
    }

    array<array<CellType, BOARD_SZ>, BOARD_SZ> cells_ = {CellType::Empty};
    vector<Pos> moves_history_;
    long long evaluation_for_crosses_ = 0;
};

string Strip(string_view sv) {
    while (!sv.empty() && isspace(sv.front())) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && isspace(sv.back())) {
        sv.remove_suffix(1);
    }
    return string(sv);
}

string GetStrippedLine(istream& input = cin) {
    string result;
    getline(cin, result);
    return Strip(result);
}

template<typename MoveIt>
void WatchReplay(MoveIt moves_begin, MoveIt moves_end) {
    Board board;
    MoveIt cur_move_to_make = moves_begin;
    while (true) {
        system("cls");
        board.PrintToConsole();
        // TODO: add +n, -n commands
        cout << R"(command:
"+" - to watch next move
"-" - to watch previous move
"quit" - to quit)" << endl;
        cout << "Enter your command:" << endl;
        string command = GetStrippedLine();
        if (command == "+") {
            if (cur_move_to_make != moves_end) {
                board.MakeMove(*(cur_move_to_make++));
            }
        } else if (command == "-") {
          if (cur_move_to_make != moves_begin) {
              board.UndoMove();
              --cur_move_to_make;
          }
        } else if (command == "quit") {
            break;
        }
    }
}

void Play() {
    Board board;
    vector<Board::Pos> moves;
    while (true) {
        system("cls");
        board.PrintToConsole();
        if (board.GetState() == Board::State::NoughtsWon) {
            cout << "You lost!" << endl;
            break;
        } else if (board.GetState() == Board::State::Draw) {
            cout << "Draw!" << endl;
            break;
        }
        { // Ход игрока
            cout << R"(command:
row column - to make a move
"surrender" - to surrender)" << endl;
            cout << "Enter you command: " << endl;
            while (true) {
                string command = GetStrippedLine();
                if (command == "surrender") {
                    return;
                } else {
                    stringstream ss(command);
                    int row, col;
                    ss >> row >> col;
                    --row;
                    --col;
                    if (ss && board.ValidMove({row, col})) {
                        board.MakeMove({row, col});
                        break;
                    } else {
                        cout << "Invalid command! Try again: " << endl;
                    }
                }
            }
        }
        system("cls");
        board.PrintToConsole();
        if (board.GetState() == Board::State::CrossesWon) {
            cout << "You won!" << endl;
            break;
        }
        { // Ход компьютера
            moves.push_back(*board.FindBestMove().first);
            board.MakeMove(moves.back());
            moves.pop_back();
        }
    }
    //
    cout << "Do u want to watch replay?(y/n)" << endl;
    string answer = GetStrippedLine();
    transform(begin(answer), end(answer), begin(answer),
              [](char ch) { return tolower(ch); });
    if (answer == "y" || answer == "yes") {
        auto moves = board.GetMovesHistory();
        WatchReplay(begin(moves), end(moves));
    } else if (answer != "n" && answer != "no") {
        cout << "I assume it was \"no\"" << endl;
    }
}

void TestWatchReplay() {
    Board board;
    for (int i = 0; i < 100; ++i) {
        if (board.GetState() != Board::State::GameIsNotOver) {
            break;
        }
        board.MakeMove(*board.FindBestMove().first);
    }
    auto moves = board.GetMovesHistory();
    WatchReplay(begin(moves), end(moves));
}

int main() {
    //TestWatchReplay();
    Play();
    return 0;
}
