import argparse
import os
import random
import sys

from generation import generate_file, load_chunk_data

OUT_DIR = "out"
SAVE_DIR = os.path.join(sys.path[0], "../save")
CHUNK_DATA_DIR = os.path.join(sys.path[0], "../chunk_data")


def read_save(path):
    with open(path, "rb") as f:
        save = f.read()
    return bytearray(save)


def write_save(save, path):
    with open(path, "wb") as f:
        f.write(bytes(save))


def edit_seed(save, seed):
    seed_bytes = [
        (seed >> (8 * i)) % 0x100
        for i in range(4)
    ]
    save[4:8] = seed_bytes


def edit_chapters_structure(save, chapter, file):
    i = chapter
    chapter = file.chapters[i]
    base_pos = len(save) - (0x126 + 0x12b * (5 - i))    # chapter structure
    for j in range(12):
        if j == chapter.warpzone:
            byte = 0x10
        elif j in chapter.pacifiers:
            byte = 0x20
        else:
            byte = 0x00
        save[base_pos + 0x15 * j + 2] = byte


def edit_save(save, seed, chapter, chunk_data):
    file = generate_file(seed, chunk_data)
    edit_seed(save, seed)
    edit_chapters_structure(save, chapter, file)


def parse_seed(s):
    if s == "random":
        return None
    else:
        return int(s, 16)


def parse_chapter(s):
    chapter = int(s)
    if chapter < 1 or chapter > 5:
        raise ValueError(f"Invalid chapter: {chapter}")
    return chapter - 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "chapter",
        type=parse_chapter,
        help="Generate a save at the start of this chapter. "
             "1 = chipper grove, ...",
    )
    parser.add_argument(
        "seed",
        type=parse_seed,
        help='New seed as a hex number (ex: 0x12345678), '
             'or "random" for a random seed.'
    )
    parser.add_argument(
        "-w", "--write",
        nargs=1,
        type=str,
        help="Write the generated save to this file",
    )
    args = parser.parse_args()

    if args.seed is None:
        args.seed = random.randrange(0, 0x100000000)

    source_file = os.path.join(SAVE_DIR, "SAVESLOT_ch" + str(args.chapter + 1))
    save = read_save(source_file)
    chunk_data = load_chunk_data(CHUNK_DATA_DIR)
    edit_save(save, args.seed, args.chapter, chunk_data)

    if args.write is None:
        if not os.path.exists(OUT_DIR):
            os.mkdir(OUT_DIR)
        target_path = os.path.join(OUT_DIR, "SAVESLOT")
        if os.path.exists(target_path):
            raise FileExistsError(f"Could not write output to {target_path} "
                                  f"because the file already exists.")
    else:
        target_path = args.write[0]
    write_save(save, target_path)
