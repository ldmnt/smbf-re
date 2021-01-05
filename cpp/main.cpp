/* 
Usage : ./main.exe first_seed last_seed
Seeds are passed as hex integers, ommiting the 0x prefix, for instance 3456789A.

Some arrays are defined as static global variables and reused for every seed.
This reduces the computing time by avoiding too many allocations.
*/

#include <stdio.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <set>

using namespace std;

const char* CHUNK_DATA_FOLDER = "chunk_data";
int KEEP_BEST_SEEDS = 50;

float SPECIAL_LEVEL_PROBABILITY_INCREMENT = 1.0f / 6;
vector<int> special_levels_0{ 3, 4, 5, 6, 7, 8, 9, 10, 11 };
vector<int> special_levels_n{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
vector<int> difficulties{ 8, 9, -1, -1, -1, -1, -1 };

pair<int, unordered_set<int>> chapters_structure[5];  // warpzone and set of pacifiers
int special_indices[7];  // used temporarily during chapter structure construction
int selected_chunks[9][32];  // to store the results of random calls during level generation
int n_selected[9];  // sizes for the preceding array

uint64_t _random_state;

void SetRandomState(uint32_t state) {
  state = state % 0x7fffffff;
  if (state < 1)
    state = 1;
  _random_state = state;
}

int RandInt(int max) {
  // random integer from 0 to max included
  if (max == 0)
    return 0;
  int divisor = 0x7ffffffe / (max + 1);
  uint64_t r;
  do {
    _random_state = (_random_state * 0xbc8f) % 0x7fffffff;
    r = (_random_state - 1) / divisor;
  } while (r > max);

  return r;
}

bool RandBool(float probability) {
  int r = RandInt(1000000);
  return r <= static_cast<int>(probability * 1000000);
}

int LevelGenerationSeed(int seed, int chapter, int level) {
  int32_t r = (chapter + 1) * seed + (level + 1);
  return r;
}

struct Chunk {
  int id;
  float par_time;
  bool has_warpzone;
  bool has_pacifier;
};

struct LevelData {
  vector<vector<Chunk>> chunks;
  vector<pair<int, int>> warpzone_chunks;
  vector<pair<int, int>> pacifier_chunks;
};

LevelData LoadLevelData(string path) {
  LevelData level_data;
  level_data.chunks.resize(9);
  ifstream file;
  file.open(path);
  if (!file.is_open()) 
    throw runtime_error("Could not open file.");
  string line;
  getline(file, line);
  while (getline(file, line)) {
    Chunk c;
    istringstream ss(line);
    string val;
    getline(ss, val, ',');
    int i = stoi(val) - 1;
    for (int i = 0; i < 3; ++i) 
      getline(ss, val, ',');
    c.id = stoi(val);
    getline(ss, val, ',');
    c.par_time = stof(val);
    getline(ss, val, ',');
    c.has_warpzone = stoi(val);
    getline(ss, val, '\n');
    c.has_pacifier = stoi(val);
    level_data.chunks[i].push_back(c);
    if (i < 4) {
      int j = level_data.chunks[i].size() - 1;
      if (c.has_warpzone)
        level_data.warpzone_chunks.push_back(make_pair(i, j));
      if (c.has_pacifier)
        level_data.pacifier_chunks.push_back(make_pair(i, j));
    }
  }
  file.close();
  return level_data;
}

vector<vector<LevelData>> LoadChunkData(string folder) {
  vector<vector<LevelData>> chapters;
  for (int i = 0; i < 5; ++i) {
    vector<LevelData> levels;
    for (int j = 0; j < 6; ++j) {
      char path[256];
      sprintf_s(path, 256, "%s/%i-%i.csv", folder.c_str(), i + 1, j + 1);
      auto chunks = LoadLevelData(path);
      levels.push_back(chunks);
    }
    chapters.push_back(levels);
  }
  return chapters;
}

void ExtractWarpzoneChunk(const LevelData& chunks, int (&selected_chunks)[9][32], int (&n_selected)[9]) {
  const auto& candidates = chunks.warpzone_chunks;
  int index = RandInt(candidates.size() - 1);
  auto& selected = candidates[index];
  int i = get<0>(selected), j = get<1>(selected);
  selected_chunks[i][n_selected[i]] = j;
  n_selected[i]++;
}

void ExtractPacifierChunk(const LevelData& chunks, int (&selected_chunks)[9][32], int (&n_selected)[9]) {
  const auto& candidates = chunks.pacifier_chunks;
  int index = RandInt(candidates.size() - 1);
  auto& selected = candidates[index];
  int i = get<0>(selected), j = get<1>(selected);
  selected_chunks[i][n_selected[i]] = j;
  n_selected[i]++;
}

 /* Modify an array of indices A into _A so that extracting the elements of another array B
 with indices in _A is the same as extracting the elements of B with indices in A with
 removal of the selected element at each step. For instance if A = { 1, 3 }, _A = { 1, 4 }
 since removing B[1] shifts the element with index 4 to the left. Only modifies `count`
 indices. */
template<int size>
void TransformToRemovalSpace(int (&indices)[size], int count) {
  /* Naive implementation. For each index, increment it for all the smaller indices that
   were already selected. But now it might have become bigger than some other already
   selected indices, so perform another pass and continue until there is no more modification. */
  for (int i = 0; i < count; ++i) {
    int index = indices[i];
    int previous_index = -1;
    int adjusted = index;
    bool modified;
    do {
      modified = false;
      for (int k = i - 1; k >= 0; --k) {
        if (indices[k] <= index && indices[k] > previous_index) {
          adjusted++;
          modified = true;
        }
      }
      previous_index = index;
      index = adjusted;
    } while (modified);
    indices[i] = index;
  }
}

/* In the following functions, the randomly chosen indices are all computed at once and then
shifted. The purpose is to avoid copying the arrays to select from and having to perform
deletions after each selection of a random element. */
void GenerateChaptersStructure(int seed, pair<int, unordered_set<int>> (&chapters_structure)[5], 
    int (&special_indices)[7]) {
  SetRandomState(seed);
  for (int i = 0; i < 5; ++i) {
    const auto& special_levels = i == 0 ? special_levels_0 : special_levels_n;
    for (int j = 0; j < 7; ++j) {
      special_indices[j] = RandInt(special_levels.size() - j - 1);
    }
    TransformToRemovalSpace(special_indices, 7);

    auto& t = chapters_structure[i];
    get<0>(t) = special_levels[special_indices[0]];
    auto& pacifiers = get<1>(t);
    pacifiers.clear();
    for (int j = 0; j < 6; ++j)
      pacifiers.emplace(special_levels[special_indices[j + 1]]);
  }
}

float ComputeLevelParTime(const vector<vector<Chunk>>& chunks, int (&selected_chunks)[9][32], int (&n_selected)[9]) {
  float result = 3.0f;
  for (int i = 0; i < 9; ++i) {
    int (&indices)[32] = selected_chunks[i];
    TransformToRemovalSpace(indices, n_selected[i]);
    for (int j = 0; j < n_selected[i]; ++j) {
      result += chunks[i][indices[j]].par_time;
    }
  }
  return result;
}

float GenerateLevelParTime(int initial_random_state, const LevelData& level_data, 
    bool generate_warpzone, bool generate_pacifier, int (&selected_chunks)[9][32], int (&n_selected)[9]) {
  SetRandomState(initial_random_state);
  float warpzone_probability = SPECIAL_LEVEL_PROBABILITY_INCREMENT;

  if (generate_warpzone)
    ExtractWarpzoneChunk(level_data, selected_chunks, n_selected);

  if (generate_pacifier)
    ExtractPacifierChunk(level_data, selected_chunks, n_selected);

  const auto& chunk_data = level_data.chunks;
  for (int k = 0; k < 7; ++k) {
    pair<int, int>* selected_chunk = nullptr;
    int difficulty = difficulties[k];
    if (difficulty != -1 && chunk_data[difficulty - 1].size() > 0) {
      const vector<Chunk>& chunks = chunk_data[difficulty - 1];
      int index = RandInt(chunks.size() - 1);
      auto pair = make_pair(difficulty - 1, index);
      selected_chunk = &pair;
    }

    bool use_warpzone_chunk = false;
    if (generate_warpzone) {
      use_warpzone_chunk = RandBool(warpzone_probability);
    }
    bool use_pacifier_chunk = false;
    if (generate_pacifier) {
      use_pacifier_chunk = RandBool(warpzone_probability);
    }

    if (!selected_chunk) {
      warpzone_probability = warpzone_probability + SPECIAL_LEVEL_PROBABILITY_INCREMENT;
      if (warpzone_probability > 1.0f)
        warpzone_probability = 1.0f;
      if (use_warpzone_chunk) {
        selected_chunk = nullptr;
        generate_warpzone = false;
      }
      else if (use_pacifier_chunk) {
        selected_chunk = nullptr;
        generate_pacifier = false;
      }
      else {
        int i = RandInt(3);
        const vector<Chunk>& chunks = chunk_data[i];
        int j = RandInt(chunks.size() - n_selected[i] - 1);
        auto pair = make_pair(i, j);
        selected_chunk = &pair;
      }
    }
    
    if (selected_chunk) {
      int i = get<0>(*selected_chunk), j = get<1>(*selected_chunk);
      selected_chunks[i][n_selected[i]] = j;
      n_selected[i]++;
    }
  }
  return ComputeLevelParTime(chunk_data, selected_chunks, n_selected);
}

float GenerateChapterParTime(int i, int warpzone, const unordered_set<int>& pacifiers, int seed, 
    const vector<LevelData>& chunk_data, int (&selected_chunks)[9][32], int (&n_selected)[9]) {
  auto times = vector<float>(6);
  for (int j = 0; j < 6; ++j) {
    for (int k = 0; k < 9; ++k) {
      n_selected[k] = 0;
    }
    float level = GenerateLevelParTime(
      LevelGenerationSeed(seed, i, j),
      chunk_data[j],
      warpzone == j,
      pacifiers.count(j) == 1,
      selected_chunks,
      n_selected
    );
    times[j] = level;
  }

  sort(times.begin(), times.end());
  float result = 0.0f;
  for (int i = 0; i < 4; ++i) {
    result += times[i];
  }
  return result;
}

float GenerateFileParTime(
    int seed, 
    const vector<vector<LevelData>>& chunk_data,
    pair<int, unordered_set<int>> (&chapters_structure)[5],
    int (&special_indices)[7],
    int (&selected_chunks)[9][32],
    int (&n_selected)[9]
  ) {
  float result = 0.0f;
  GenerateChaptersStructure(seed, chapters_structure, special_indices);
  for (int i = 0; i < 5; i++) {
    const auto& structure = chapters_structure[i];
    float chapter = GenerateChapterParTime(i, get<0>(structure), get<1>(structure), seed, chunk_data[i],
      selected_chunks, n_selected);
    result += chapter;
  }
  return result;
}

string FormatDuration(int duration) {
  ostringstream ss;
  ss.fill('0');
  int days = duration / (3600 * 24);
  duration -= days * 3600 * 24;
  int hours = duration / 3600;
  duration -= hours * 3600;
  int minutes = duration / 60;
  int seconds = duration - 60 * minutes;
  if (days > 0)
    ss << days << " days ";
  string type = "seconds";
  if (hours > 0) {
    ss << setw(2) << hours << ":";
    type = "hours";
  }
  if (minutes > 0) {
    ss << setw(2) << minutes << ":";
    type = "minutes";
  }
  ss << setw(2) << seconds << " " << type;
  return ss.str();
}

string ToHex(int n) {
  ostringstream ss;
  ss << "0x" << setfill('0') << setw(8) << hex << n;
  return ss.str();
}

bool CompareTimes(pair<int, float> p1, pair<int, float> p2) {
  return get<1>(p1) < get<1>(p2);
}

int main(int argc, char* argv[])
{
  uint64_t first_seed = stoull(argv[1], nullptr, 16);
  uint64_t last_seed = stoull(argv[2], nullptr, 16);

  auto chunk_data = LoadChunkData(CHUNK_DATA_FOLDER);

  set<pair<int, float>, decltype(CompareTimes)*> best_seeds(CompareTimes);
  float best = 10000.0f;
  int best_seed = -1;
  float worst = 0.0f;
  int worst_seed = -1;
  int start_time = time(nullptr);
  int last_report = start_time;
  cout << "Crunching seeds from " << ToHex(first_seed) << " to " << ToHex(last_seed - 1) << endl;
  for (unsigned int seed = first_seed; seed < last_seed; seed++) {
    float r = GenerateFileParTime(seed, chunk_data, chapters_structure, special_indices, selected_chunks, n_selected);
    best_seeds.insert(make_pair(seed, r));
    if (best_seeds.size() > KEEP_BEST_SEEDS) {
      best_seeds.erase(--best_seeds.end());
    }
    if (r > worst) {
      worst = r;
      worst_seed = seed;
    }
    int current_time = time(nullptr);
    if (current_time - last_report >= 10 || seed == last_seed - 1) {
      last_report = current_time;
      int elapsed = current_time - start_time;
      float speed = static_cast<float>(seed - first_seed + 1) / elapsed;
      int estimated = static_cast<int>((last_seed - seed) / speed);
      float processed = static_cast<float>(seed - first_seed + 1) / (last_seed - first_seed + 1) * 100;
      cout << fixed << endl
        << "top " << KEEP_BEST_SEEDS << " seeds: \n";
      for (auto it = best_seeds.begin(); it != best_seeds.end(); ++it) {
        cout << ToHex(get<0>(*it)) << "  " << setprecision(2) << get<1>(*it) << endl;
      }
      cout << endl 
        << "worst seed: " << ToHex(worst_seed) << "  " << setprecision(2) << worst << endl
        << "processed: " << setprecision(3) << processed << "% -- "
        << "current seed: " << ToHex(seed) << " -- "
        << "remaining time: " << FormatDuration(estimated) << endl;
    }
  }
  int total_time = time(nullptr) - start_time;
  cout << endl << "total time: " << FormatDuration(total_time) << endl;
}
