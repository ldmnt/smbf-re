""" Chapters are numbered from 0 - chipper grove to 4 - other side
levels are numbered from 0 to 11. 0 to 5 being the light levels and 6 to 11 the
dark ones.

The process of generating the light levels goes:
1) set the RNG state depending on the seed
2) generate all the chapters structures, that is which levels will contain the
    warpzones and pacifiers
3) for each light level:
    3a) set the RNG state depending on the chapter, level and the seed
    3b) if there is a warpzone or pacifier in the level, choose randomly the
        chunk that will contain it among those that support it
    3c) choose the rest of the chunks as a mixture of predetermined chunks and
        randomly selected chunks according to a preset pattern that is common to
        all levels

main entry points :
    - load_chunk_data : reads the chunk data from csv files
    - generate_file : generates all the information (such as par times) for a
        given seed
"""

import csv
import os
from collections import defaultdict

DIFFICULTIES = [8, 9, -1, -1, -1, -1, -1]
MAX_RANDOM_DIFFICULTY = 4
SPECIAL_PROBABILITY_INCREMENT = 1 / 6


def special_levels(chapter):
    """ levels that are candidates for warpzones and pacifiers """
    if chapter == 0:
        return list(range(3, 12))
    else:
        return list(range(0, 12))


random_state = 0x0


def set_random_seed(seed):
    global random_state
    random_state = max(seed % 0x7FFFFFFF, 1)


def rand_int(m):
    """ random integer between 0 and m included """
    # This is an unnecessarily convoluted reformulation of boost's minrand_std
    # linear congruential RNG...I noticed after the fact.
    if m == 0:
        return 0
    divisor = 0x7FFFFFFE // (m + 1)
    r = m + 1

    def update():
        global random_state
        nonlocal r
        a = random_state * 0xBC8F
        q = (a * 0x200000005) >> 0x40
        r = (a - q) >> 1
        r = (r + q) >> 0x1E
        r = a - r * 0x7FFFFFFF
        random_state = r - (r >> 0x20)
        r = (r - 1) // divisor

    while r > m:
        update()
    return r


def rand_bool(probability):
    """ true with the given probability """
    r = rand_int(1000000)
    return r <= int(1000000 * probability)


def level_generation_seed(seed, chapter, level):
    r = (chapter + 1) * seed + (level + 1)
    return r % 0x100000000


def chapters_generation_seed(seed):
    d = (seed * 3) >> 0x20
    r = (seed - d) >> 1
    r = (r + d) >> 0x1E
    r = r * 0x7FFFFFFF
    return (seed - r) % 0x100000000


class Chunk:
    def __init__(self, id, difficulty, par_time, has_warpzone, has_pacifier):
        self.id = id
        self.difficulty = difficulty
        self.par_time = par_time
        self.has_warpzone = has_warpzone
        self.has_pacifier = has_pacifier

    def __repr__(self):
        return f"[Chunk {self.id}]"


class Level:
    def __init__(self, chunks):
        self.chunks = chunks

    def par_time(self):
        return 3.0 + sum(c.par_time for c in self.chunks)

    def difficulty(self):
        return sum(c.difficulty for c in self.chunks) / len(self.chunks)


class Chapter:
    def __init__(self, levels, warpzone, pacifiers):
        self.levels, self.warpzone, self.pacifiers = levels, warpzone, pacifiers

    def par_time(self):
        return sum(level.par_time() for level in self.levels)

    def difficulty(self):
        return sum(level.difficulty() for level in self.levels) / len(
            self.levels
        )

    def any_par_time(self):
        levels = sorted(self.levels, key=lambda l: l.par_time())
        return sum(level.par_time() for level in levels[:4])


class File:
    def __init__(self, seed, chapters):
        self.seed, self.chapters = seed, chapters

    def par_time(self):
        return sum(chapter.par_time() for chapter in self.chapters)

    def difficulty(self):
        return sum(chapter.difficulty() for chapter in self.chapters) / len(
            self.chapters
        )

    def any_par_time(self):
        return sum(chapter.any_par_time() for chapter in self.chapters)

    def summary(self):
        s = (
            f"\nseed: {hex(self.seed)}\n"
            f"any% par time: {round(self.any_par_time(), 2)}\n"
        )
        for i, chapter in enumerate(self.chapters):
            s += (
                f"\nchapter {i + 1} - any% par time "
                f"{round(chapter.any_par_time(), 2)}\n"
            )
            s += (
                f"\twarpzone: {chapter.warpzone + 1} - pacifiers: "
                f"{[i + 1 for i in chapter.pacifiers]}\n"
            )
            for j, level in enumerate(chapter.levels):
                s += (
                    f"\t{i + 1}-{j + 1}: par time "
                    f"{round(level.par_time(), 2)}\n"
                )
        return s


def load_chunk_data(folder):
    chapters = []
    for i in range(5):
        levels = []
        for j in range(6):
            filepath = os.path.join(folder, f"{i+1}-{j+1}.csv")
            with open(filepath, "r") as file:
                try:
                    reader = csv.reader(file)
                    next(reader)  # discard column names
                    chunk_data = defaultdict(list)
                    for row in reader:
                        (
                            difficulty,
                            _,
                            _,
                            chunk_id,
                            par_time,
                            has_warpzone,
                            has_pacifier,
                        ) = row
                        difficulty = int(difficulty)
                        chunk = Chunk(
                            difficulty=difficulty,
                            id=int(chunk_id),
                            par_time=float(par_time),
                            has_warpzone=has_warpzone == "1",
                            has_pacifier=has_pacifier == "1",
                        )
                        chunk_data[difficulty].append(chunk)
                except ValueError:
                    raise ValueError(f"Invalid chunk data in {filepath}")
            levels.append(chunk_data)
        chapters.append(levels)
    return chapters


def extract_special_chunk(chunks, filtering_function):
    """ choose randomly a chunk among those that verify a condition """
    candidates = []
    for i in range(len(chunks)):
        for j in range(len(chunks[i])):
            c = chunks[i][j]
            if filtering_function(c):
                candidates.append((i, j, c))
    index = rand_int(len(candidates) - 1)
    i, j, result = candidates[index]
    del chunks[i][j]
    return result


def generate_chapters_structure(seed):
    """
    1) choose randomly a warpzone chunk
    2) choose 6 pacifier chunks among those that remain
    """
    set_random_seed(chapters_generation_seed(seed))
    result = []
    for chapter in range(5):
        available_levels = special_levels(chapter)
        index = rand_int(len(available_levels) - 1)
        warpzone = available_levels[index]
        del available_levels[index]
        pacifiers = set()
        for level in range(6):
            index = rand_int(len(available_levels) - 1)
            pacifiers.add(available_levels[index])
            del available_levels[index]
        result.append((warpzone, list(sorted(pacifiers))))
    return result


def generate_level(
    initial_random_state,
    difficulties,
    max_random_difficulty,
    chunk_data,
    generate_warpzone,
    generate_pacifier,
):
    """some details make the logic a bit confusing, but essentially :
    1) if there’s a warpzone or a pacifier, set aside a chunk that supports it
    2) for each difficulty, choose randomly a chunk of that difficulty. If
        difficulty is -1 or no chunks of that difficulty are available, choose
        randomly a chunk with difficulty <= 4
    3) replace one of the chunks by the eventual warpzone/pacifier chunk
    4) reorder the chunks with difficulty < 8 by increasing difficulty
    """
    result = []
    set_random_seed(initial_random_state)
    special_probability = SPECIAL_PROBABILITY_INCREMENT
    remaining_random_chunks = []
    for difficulty in range(1, max_random_difficulty + 1):
        remaining_random_chunks.append(chunk_data[difficulty].copy())

    warpzone_chunk = None
    if generate_warpzone:
        warpzone_chunk = extract_special_chunk(
            remaining_random_chunks, lambda c: c.has_warpzone
        )
    pacifier_chunk = None
    if generate_pacifier:
        pacifier_chunk = extract_special_chunk(
            remaining_random_chunks, lambda c: c.has_pacifier
        )

    for difficulty in difficulties:
        selected_chunk = None
        if difficulty != -1 and chunk_data[difficulty]:
            chunks = chunk_data[difficulty]
            index = rand_int(len(chunks) - 1)
            selected_chunk = chunks[index]
        if warpzone_chunk:
            use_warpzone_chunk = rand_bool(special_probability)
        else:
            use_warpzone_chunk = False
        if pacifier_chunk:
            use_pacifier_chunk = rand_bool(special_probability)
        else:
            use_pacifier_chunk = False

        if selected_chunk is None:
            special_probability = min(
                special_probability + SPECIAL_PROBABILITY_INCREMENT, 1.0
            )
            if use_warpzone_chunk:
                selected_chunk = warpzone_chunk
                warpzone_chunk = None
            elif use_pacifier_chunk:
                selected_chunk = pacifier_chunk
                pacifier_chunk = None
            else:
                index = rand_int(len(remaining_random_chunks) - 1)
                chunks = remaining_random_chunks[index]
                index = rand_int(len(chunks) - 1)
                selected_chunk = chunks[index]
                del chunks[index]
        result.append(selected_chunk)
    result = sorted(
        result,
        key=lambda c: c.difficulty + 10 if c.difficulty < 8 else c.difficulty,
    )
    return Level(chunks=list(result))


def generate_chapter(i, warpzone, pacifiers, seed, chunk_data):
    levels = []
    for j in range(6):
        level = generate_level(
            initial_random_state=level_generation_seed(seed, i, j),
            difficulties=DIFFICULTIES,
            max_random_difficulty=MAX_RANDOM_DIFFICULTY,
            chunk_data=chunk_data[j],
            generate_warpzone=warpzone == j,
            generate_pacifier=j in pacifiers,
        )
        levels.append(level)
    return Chapter(levels, warpzone, pacifiers)


def generate_file(seed, chunk_data):
    if seed < 0 or seed > (1 << 32):
        raise ValueError(f"Invalid seed: {hex(seed)}")
    chapters = []
    chapters_structure = generate_chapters_structure(seed)
    for i in range(5):
        warpzone, pacifiers = chapters_structure[i]
        chapter = generate_chapter(i, warpzone, pacifiers, seed, chunk_data[i])
        chapters.append(chapter)
    return File(seed, chapters)
