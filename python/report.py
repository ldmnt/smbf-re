import argparse

from generation import load_chunk_data, generate_file

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "seed",
        type=lambda n: int(n, 16),
        help="use hex format, for instance 0x1234578",
    )
    parser.add_argument(
        "-d",
        "--chunk-data",
        help="Specify a directory containing the chunk data. Defaults to "
        '"./chunk_data".',
        nargs="?",
        default="chunk_data",
        const="chunk_data",
    )
    args = parser.parse_args()

    chunk_data = load_chunk_data(args.chunk_data)
    file = generate_file(args.seed, chunk_data)
    print(file.summary())
