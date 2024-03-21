from rich.console import Console
from rich.text import Text
from rich.progress import Progress, SpinnerColumn
import subprocess
import time

def select_ide(console):
    pass

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
