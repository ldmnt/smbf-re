These are some tools related to Super Meat Boy Forever speedrunning.

## Seed generation

### Save file simulation
The `generation` module reproduces the logic that the game uses to generate
the various levels given the initial random seed. The generation of a
save file may be simulated using the `report.py` script.

Example:
```sh
python python/report.py 0x456789AB
```
displays various information about the contents of the chapters for the given
seed. 
The seed is passed as an 8 digit hexadecimal number that corresponds to the
in-game "enter seed" pictures by 0 = meat boy, 1 = bandage girl, ... F = betus.

### Seed exploration
The cpp folder contains a more efficient c++ loop that generates all the
seeds using the same logic as above to find the ones with the best par times.


## Save file manipulation
Please backup your save files before you use any of this, just in case.

The `generate_iw.py` script may be used to generate a save file with
a given chapter unlocked (and still unplayed) for a specific seed.

Example:
```sh
python python/generate_iw.py 4 0x456789AB
```
will produce a save file for that seed with lab unlocked. The file is placed
in a directory called "out" where the python script was called.

The seed argument can be replaced by `random` and the save file will be
generated for a random seed.
There is also a `--write` option that may be used to place the generated file
to a specific location (overwrites if already exists).

Example:
```sh
python python/generate_iw.py 3 random --write "path/to/saves/SAVESLOT0"
```
will produce a save file with tetanusville unlocked for a random seed and
write it to `path/to/saves/SAVESLOT0`. This combination is useful for individual
world random seed practice.

Note that only the requested chapter will correspond to the requested seed. The
earlier chapters will not be the ones you would get with that seed.