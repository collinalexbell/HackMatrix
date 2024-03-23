from rich.console import Console
from rich.text import Text
from rich.progress import Progress, SpinnerColumn
import subprocess
import time
import json
import hackMatrix.api as matrix

def select_ide(console):
    subprocess.run(["kitty --title select-ide --config ../apps/.select_ide.conf python select-ide.py"], shell=True)
    console.clear()
    text = Text()

    matrix.player_move((-0.38, 1.3, -4.6), (0,30,0), 1)

    # Specify the file path
    file_path = "../settings.json"

    # Read data from JSON file
    with open(file_path, "r") as json_file:
        data = json.load(json_file)

    # Extract the 'ide' value and store it in the 'ide' variable
    ide = data["ide"]

    if ide == "Vim":
        text.append("Vim is a simple, yet powerful\n")
        text.append("    used by some of the oldest and wisest of code mages\n")

    if ide == "Emacs":
        text.append("Emacs is extensive, deep, and highly customizable\n")
        text.append("      used by unique code mages\n")
        text.append("              who love the power of LISP\n")

    if ide == "VSCode":
        text.append("VSCode is popular amongst code mages\n")
        text.append("          for its sleek and extensible design\n")

    text.append("\nGreat choice!\n")
    console.print(text)

def talk(console):
    text = Text()
    text.append("Hello! Welcome to ")
    text.append("HackMatrix", style="bold green")
    console.print(text)

    progress = Progress(SpinnerColumn())
    # Start the progress rendering
    with progress:
        # Update the progress continuously for 1 second
        for _ in progress.track(range(100)):
            time.sleep(0.01)  # Adjust this sleep duration to control the speed of the spinner animation

    text = Text()
    text.append("You should `select-ide`")
    console.print(text)

knownCommands = {
    "talk": talk,
    "select-ide": select_ide
}

cmd = ""
console = Console()
console.clear()
while cmd != "exit":
    text = Text("npc@matrix:~/$ ")
    text.stylize("bold magenta", 0, 3)
    text.stylize("bold green", 4, 10)
    cmd = console.input(text)
    if cmd in knownCommands:
        knownCommands[cmd](console)
    else:
        subprocess.run(cmd, shell=True)
