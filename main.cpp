
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <map>

const std::vector<std::vector<int>> dir4{{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
const std::vector<std::vector<int>> dir8{{0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

enum class CellType {
	EMPTY,
	BOMB,
};

struct Cell {
	CellType type;
	bool is_flagged;
	bool is_revealed;

	Cell(CellType type) 
		: type(type), is_revealed(false), is_flagged(false) {}

	void reset() {
		is_flagged = false;
		is_revealed = false;
	}
};

constexpr bool outside(const std::vector<std::vector<Cell>>& grid, int i, int j) {
	return i < 0 || j < 0 || i >= grid.size() || j >= grid[0].size();
}

constexpr uint32_t count_neighbors(const std::vector<std::vector<Cell>>& grid, int i, int j, const std::function<bool(Cell)>& predicate) {
	if (outside(grid, i, j)) {
		return 0;
	}

	uint32_t result = 0;

	for (auto& dir : dir8) {
		if (!outside(grid, i + dir[0], j + dir[1]) && predicate(grid[i + dir[0]][j + dir[1]])) {
			result++;
		}
	}

	return result;
}

inline uint32_t count_bomb_neighbors(const std::vector<std::vector<Cell>>& grid, int i, int j) {
	return count_neighbors(grid, i, j, [](Cell cell) {
			return cell.type == CellType::BOMB;
	});
}

inline uint32_t count_flagged_neighbors(const std::vector<std::vector<Cell>>& grid, int i, int j) {
	return count_neighbors(grid, i, j, [](Cell cell) { return cell.is_flagged; });
}

enum class GameState {
	OVER,
	ACTIVE,
};

struct Game {
	float bomb_likelihood;
	uint32_t count_bombs;
	uint32_t count_flagged;
	uint32_t count_revealed;

	bool first_move;
	GameState state;
	std::vector<std::vector<Cell>> grid;

	Game(size_t m, size_t n, float bomb_likelihood)
		: bomb_likelihood(bomb_likelihood), state(GameState::ACTIVE), count_bombs(0), count_flagged(0), count_revealed(0)
	{
		fill_grid(m, n);
	}

	void fill_grid(size_t m, size_t n) {
		grid = std::vector<std::vector<Cell>>(m, std::vector<Cell>(n, Cell(CellType::EMPTY)));
		for (auto& row : grid) {
			for (auto& cell : row) {
				if ((rand() % 100) / 100.0f <= bomb_likelihood) {
					cell.type = CellType::BOMB;
					count_bombs++;
				}
				
			}
		}
	}


	void restart() {
		first_move = true;
		state = GameState::ACTIVE;
		count_flagged = 0;
		count_revealed = 0;

		for (auto& row : grid) {
			for (auto& cell : row) {
				cell.is_flagged = false;
				cell.is_revealed = false;
			}
		}
	}
};

inline bool is_won(const Game& game) {
	return game.count_revealed == game.grid.size() * game.grid[0].size() - game.count_bombs;
}

void print(std::ostream& os, const Game& game, int i, int j) {
	auto& cell = game.grid[i][j];
	if (cell.is_flagged || is_won(game) && cell.type == CellType::BOMB) {
		os << "\033[1;44mF\033[0m";
		return;
	} 

	if (game.state != GameState::OVER && !cell.is_revealed) {
		os << ".";
		return;
	}

	if (cell.type == CellType::BOMB) {
		os << "\033[30;41;1mB\033[0m";
		return;
	}
		
	auto bombs = count_bomb_neighbors(game.grid, i, j);
	std::string result = (!bombs) ? "\033[47m \033[0m" : "\033[43;30;1m" + std::to_string(bombs) + "\033[0m";
	os << result;
}

std::ostream& operator<<(std::ostream& os, const Game& game) {
	os << "   ";
	for (int i = 0; i < game.grid[0].size(); i++) {
		os << i << " ";
	}
	os << '\n';
	for (int i = 0; i < game.grid.size(); i++) {
		os << i << "  ";
		for (int j = 0; j < game.grid[i].size(); j++) {
			print(os, game, i, j);
			os << " ";
		}
		os << '\n';
	}

	return os;
}

enum class PlayerMove {
	Success,
	NA,
	LosingMove,
	OutBounds,
};

void expand(Game& game, int i, int j) {
	if (outside(game.grid, i, j) || game.grid[i][j].type == CellType::BOMB || game.grid[i][j].is_revealed || game.grid[i][j].is_flagged) {
		return;
	}

	game.grid[i][j].is_revealed = true;
	game.count_revealed++;

	if (count_flagged_neighbors(game.grid, i, j) != count_bomb_neighbors(game.grid, i, j)) {
		return;
	}

	for (auto& dir : dir4) {
		expand(game, i + dir[0], j + dir[1]);
	}
}

PlayerMove try_reveal(Game& game, const std::pair<int, int>& place) {
	auto [i, j] = place;
	if (outside(game.grid, i, j)) {
		return PlayerMove::OutBounds;
	}

	if (game.grid[i][j].is_flagged) {
		return PlayerMove::NA;
	}

	if (game.grid[i][j].type == CellType::BOMB) {
		game.grid[i][j].is_revealed = true;
		return PlayerMove::LosingMove;
	}

	if (!game.grid[i][j].is_revealed) {
		expand(game, i, j);
		game.grid[i][j].is_revealed = true;
		return PlayerMove::Success;
	}
	
	// expanding on an already revealed cell
	if (count_flagged_neighbors(game.grid, i, j) != count_bomb_neighbors(game.grid, i, j)) {
		return PlayerMove::Success;	
	}

	for (auto& dir : dir8) {
		expand(game, i + dir[0], j + dir[1]);
	}

	return PlayerMove::Success;
}

PlayerMove try_set_flag(Game& game, const std::pair<int, int>& place, bool value) {
	auto [i, j] = place;
	if (outside(game.grid, place.first, place.second)) {
		return PlayerMove::OutBounds;
	}

	if (game.grid[i][j].is_revealed) {
		return PlayerMove::NA;
	}

	game.count_flagged += (value) ? 1 : -1;
	game.grid[i][j].is_flagged = value;
	return PlayerMove::Success;
}

void print_welcome() {
	std::cout << "Welcome to B O M B S\n";
}

typedef std::vector<std::string> Command;
Command to_command(const std::string& str) {
	std::istringstream is(str);
	Command result;
	std::string word;
	while (is >> word) {
		result.push_back(word);
	}

	return result;
}

void print_help() {
	std::cout 	<< "H E L P:\n"
			<< "(1.) Type \"flag i1 j1 i2 j2 ... in jn\" to flag the cell in the ith row (0-indexed) of the jth column (0-indexed) of the grid.\n"
			<< "(2.) Type \"unflag i1 j1 i2 j2 ... in jn\" to unflag the cell in the ith row (0-indexed) of the jth column (0-indexed) of the grid.\n"
			<< "(3.) Type \"reveal i1 j1 i2 j2 ... in jn\" to reveal the cell in the ith row (0-indexed) of the jth column (0-indexed) of the grid.\n"
			<< "(4.) Type \"exit\" to exit the game.\n"
			<< "(5.) Type \"restart\" to restart the game.\n"
			<< "(6.) Type \"bombs_left?\" to query how many bombs haven't been flagged.\n";
}


bool flag(Game& game, const Command& command, bool value) {
	if (command.size() < 3 || command.size() % 2 == 0) {
		return false;
	}

	for (size_t k = 1; k < command.size(); k += 2) {
		int i, j;
		i = std::stoi(command[k]);
		j = std::stoi(command[k + 1]);

		switch (try_set_flag(game, {i, j}, value)) {
			case PlayerMove::OutBounds:
				std::cout << "Failed (un)flagging cell: " << i << ", " << j << "], as it does not exist in grid.\n";
				break;
			case PlayerMove::NA:
				std::cout << "Failed (un)flagging cell: " << i << ", " << j << "], as the cell has already been revealed.\n";
				break;
			default: 
				break;
		}
	}

	return true;
}

bool reveal(Game& game, const Command& command) {
	if (command.size() < 3 || command.size() % 2 == 0) {
		return false;
	}

	for (size_t k = 1; k < command.size(); k += 2) {
		int i, j;
		i = std::stoi(command[k]);
		j = std::stoi(command[k + 1]);

		if (game.first_move) {
			game.grid[i][j].type = CellType::EMPTY;
		}

		game.first_move = false;
		bool success = true;

		switch (try_reveal(game, {i, j})) {
			case PlayerMove::NA:
				std::cout << "Failed revealing cell: [" << i << ", " << j << "], as you cannot reveal a flagged cell.\n";
				break;
			case PlayerMove::OutBounds:
				std::cout << "Failed revealing cell: [" << i << ", " << j << "], as it does not exist in the grid.\n";
				break;
			case PlayerMove::LosingMove:
				game.state = GameState::OVER;
				return true;
			default: 
				break;
		}
	}


	return true;
}

void print_bombs_left(Game& game) {
	std::cout << "There are " << std::max(game.count_bombs - game.count_flagged, 0u) << " bombs left.\n";
}

#define Option [](Game& game, const Command& command)

const std::map<std::string, std::function<bool(Game& game, const Command& command)>> table{
	{"flag", Option { return flag(game, command, true); }},
	{"unflag", Option { return flag(game, command, false); }},
	{"reveal", reveal},
	{"help", Option { print_help(); return true; }},
	{"exit", Option { exit(0); return true; }},
	{"restart", Option { game.restart(); return true; }},
	{"bombs_left?", Option { print_bombs_left(game); return true; }},
};

bool accept_input(Game& game) {
	std::string ln;
	std::getline(std::cin, ln);
	Command command = to_command(ln);
	
	if (command.empty()) {
		return accept_input(game);
	}

	if (!table.count(command.front())) {
		return false;
	}

	return table.at(command.front())(game, command);
}

bool prompt(Game& game) {
	std::cout << "Please enter a command or \"help\" for a list of commands.\n";
	return accept_input(game);
}

Game from_cmd_ln_args(int argc, char **argv) {
	size_t m = 8;
	size_t n = 8;
	float likelihood = .12;
	if (argc >= 4) {
		m = std::min(10, std::stoi(argv[1]));
		n = std::min(10, std::stoi(argv[2]));
		likelihood = std::max(0.0f, std::min(.50f, (float) std::stod(argv[3])));
	}

	return Game(m, n, likelihood);
}

int main(int argc, char **argv) {
	srand(time(nullptr));
	Game game = from_cmd_ln_args(argc, argv);
	print_welcome();
	std::cout << game << '\n';
	
	while (game.state != GameState::OVER) {
		bool accepted_input = prompt(game);

		if (is_won(game)) {
			game.state = GameState::OVER;
		}

		if (accepted_input) {
			std::cout << game << '\n';
		}
	}

	return 0;
}
