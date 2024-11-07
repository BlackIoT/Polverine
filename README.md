# Polverine
The Polverine project is a mikroBUS™ board designed for environmental sensing and monitoring applications.

# Visual Studio Code
### Custom Embedded Board
Reference:
[Custom embedded board](https://docs.platformio.org/en/stable/platforms/creating_board.html)

The custom embedded board JSON file for polverine board to be used in PlatoformIO environmentis in folder /boards .

1) Create `boards` directory in core_dir if it doesn’t exist.
2) Copy `polverine.json` file in this boards directory.
3) Search available boards via `pio boards` command. You should see `polverine` board.

Now, you can use `polverine` for the board option in “platformio.ini” (Project Configuration File).


